// --------------------------------------------------------------------------
//                   OpenMS -- Open-Source Mass Spectrometry
// --------------------------------------------------------------------------
// Copyright The OpenMS Team -- Eberhard Karls University Tuebingen,
// ETH Zurich, and Freie Universitaet Berlin 2002-2018.
//
// This software is released under a three-clause BSD license:
//  * Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//  * Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//  * Neither the name of any author or any participating institution
//    may be used to endorse or promote products derived from this software
//    without specific prior written permission.
// For a full list of authors, refer to the file AUTHORS.
// --------------------------------------------------------------------------
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL ANY OF THE AUTHORS OR THE CONTRIBUTING
// INSTITUTIONS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
// OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
// OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
// ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// --------------------------------------------------------------------------
// $Maintainer: Chris Bielow $
// $Authors: Maria Trofimova, Franziska Fritz, Chris Bielow $
// --------------------------------------------------------------------------

#include <OpenMS/APPLICATIONS/MapAlignerBase.h>
#include <OpenMS/ANALYSIS/MAPMATCHING/MapAlignmentAlgorithmPoseClustering.h>
#include <OpenMS/KERNEL/StandardTypes.h>
#include <OpenMS/FORMAT/FileTypes.h>
#include <OpenMS/FORMAT/FileHandler.h>
#include <OpenMS/FORMAT/FeatureXMLFile.h>
#include <OpenMS/FORMAT/TransformationXMLFile.h>
#include <OpenMS/FORMAT/ConsensusXMLFile.h>
#include <OpenMS/DATASTRUCTURES/StringListUtils.h>
#include <OpenMS/ANALYSIS/MAPMATCHING/FeatureGroupingAlgorithmQT.h>
#include <OpenMS/MATH/STATISTICS/StatisticFunctions.h>
#include <OpenMS/KERNEL/FeatureHandle.h>
#include <OpenMS/ANALYSIS/MAPMATCHING/SpanningGraph.h>
#include <boost/regex.hpp>
#include <algorithm>
#include <numeric>
#include <math.h>  

using namespace OpenMS;
using namespace std;
typedef map<pair<AASequence, int>, double> ChargedAAseqMap; //map of key (sequence + charge) and value (RT)


//two vertex indices and the weight of the edge between those
struct VertexPairDist {
	int vertex1;
	int vertex2;
	float dist;
	VertexPairDist(int x, int y, float z) : vertex1(x), vertex2(y), dist(z) {};

};


class MapAlignerTreeBased:
  public TOPPMapAlignerBase
{
public:
  MapAlignerTreeBased():
	TOPPMapAlignerBase("MapAlignerTreeBased","Tree-based alignment with identification algorithm.")
	{
	}

private:
	void registerOptionsAndFlags_()
	{
	  String file_formats = "featureXML,consensusXML";
	  registerInputFileList_("in", "<files>", StringList(), "Input files to align (all must have the same file type)", true);
         setValidFormats_("in", ListUtils::create<String>(file_formats));
         registerOutputFile_("out", "<file>", "", "Output file.", false);
         setValidFormats_("out", ListUtils::create<String>(file_formats));
         registerSubsection_("algorithm", "Algorithm parameters section");
         registerSubsection_("model", "Options to control the modeling of retention time transformations from data");
         registerFlag_("keep_subelements", "For consensusXML input only: If set, the sub-features of the inputs are transferred to the output.");
	} 

  
  ExitCodes main_(int, const char**)
  {
    StringList input_files = getStringList_("in");
    String output_file = getStringOption_("out");
    vector<ConsensusMap> maps;
	maps.reserve(input_files.size());
    vector<FeatureMap> fmaps;
    FileTypes::Type in_type = FileHandler::getType(input_files[0]); 
    
    if (in_type == FileTypes::FEATUREXML)
    {
	  for(size_t i = 0; i < input_files.size(); ++i)
	  {
		FeatureMap f;
	    ConsensusMap c;
		FeatureXMLFile().load(input_files[i],f);
	    MapConversion::convert(0, f, c, -1);
	    c.getColumnHeaders()[0].filename = input_files[i]; //get filenames
		c.applyMemberFunction(&UniqueIdInterface::setUniqueId);
		c.getColumnHeaders()[0].size = c.size();
	    maps.push_back(c);   
	  }
    }
    
    else if (in_type == FileTypes::CONSENSUSXML)
    {
      size_t index = 0;
      for(StringList::const_iterator it = input_files.begin(); it!=input_files.end(); ++it)
      {
        ConsensusMap c;
        ConsensusXMLFile().load(*it,c);
        c.getColumnHeaders()[index].filename = input_files[index]; //get filenames
        c.getColumnHeaders()[index].size = maps[index].size();
        c.applyMemberFunction(&UniqueIdInterface::setUniqueId);
        maps.push_back(c);
        index++;
      }	
    }
        
    vector<unsigned int> ori_index;
    vector<pair<int,String>> files;
        
    vector<vector<double>> M;
    ConsensusMap out;
    
    for (unsigned int i = 0; i < maps.size(); i++)
    {
      vector<double> row;
      for (unsigned int j = 0; j < maps.size(); j++)
      {
        if (i==j) {row.push_back(DBL_MAX);}
        else {row.push_back(0);}
      }
      M.push_back(row);
    }


    computeMetric(M,maps);   
    vector<VertexPairDist> queue;
    computeSpanningTree(M,queue); 
    alignSpanningTree(queue,maps,input_files,out); 
    
    ConsensusXMLFile().store(output_file, out); 

    return EXECUTION_OK;
  
  }
  
  
  
  Param getSubsectionDefaults_(const String & section) const override
  {
    if (section == "algorithm")
    {
		MapAlignmentAlgorithmIdentification algo;
		return algo.getParameters();
    }
    if (section == "model")
    {
		return TOPPMapAlignerBase::getModelDefaults("b_spline");
    }

    return Param(); 
  }
  
  

  //Fill in distance matrix
  void computeMetric(vector<vector<double>>& matrix, vector<ConsensusMap>& maps) const
  { 
  //map of a the pair of sequence and charge as key (first) and a list of retention times (second
  vector<ChargedAAseqMap> all_seq;

  for (Size m = 0; m < maps.size(); m++) 
  {
	ChargedAAseqMap seq_rts;
	//for Identifications without assigned feature?
    vector<PeptideIdentification> un_pep = maps[m].getUnassignedPeptideIdentifications();
	//get only unique ones, save duplicates to erase
	vector<pair<AASequence, int>> duplicates;

    for (vector<PeptideIdentification>::iterator pep_it = un_pep.begin(); pep_it!=un_pep.end();pep_it++)
    {

		pair<AASequence,int> seq_ch = make_pair(pep_it->getHits()[0].getSequence(),pep_it->getHits()[0].getCharge());
		if(!seq_rts.empty() && seq_rts.count(seq_ch)>0)
		{
			duplicates.push_back(seq_ch);
		}
		else
		{
			seq_rts[seq_ch] = (pep_it->getRT());
		}
    }
    
	//all maps
    for (vector<ConsensusFeature>::iterator c_it = maps[m].begin(); c_it!=maps[m].end();c_it++) 
    {
      
      if (!c_it->getPeptideIdentifications().empty())
      { 
		//get only unique ones
        //all Identifications with assigned feature
        for (vector<PeptideIdentification>::iterator p_it = 
             c_it->getPeptideIdentifications().begin(); 
             p_it!=c_it->getPeptideIdentifications().end();++p_it)
          {
            if (!p_it->getHits().empty())
            {
              //Writes peptide hit with the highest score
              p_it->sort();
              pair<AASequence,int> seq_ch = make_pair(p_it->getHits()[0].getSequence(),p_it->getHits()[0].getCharge());
			  if (!seq_rts.empty() && seq_rts.count(seq_ch) > 0)
			  {
				  duplicates.push_back(seq_ch);
			  }
			  else
			  {
				  seq_rts[seq_ch] = (p_it->getRT());
			  }
              
            }
          }
       }
    }

	//eliminate duplicates with duplicate vector
	for (unsigned int d = 0; d < duplicates.size(); d++)
	{
		seq_rts.erase(duplicates[d]);
	}

	//vector of all key, retention time pairs
    all_seq.push_back(seq_rts);

  }




  for(unsigned int i = 0; i < all_seq.size()-1; i++)
  {
    for(unsigned int j = i; j < all_seq.size(); j++) 
    {
      vector<float> map1;
      vector<float> map2;

      for (ChargedAAseqMap::iterator a_it = all_seq[i].begin(); a_it != all_seq[i].end(); ++a_it)
      {
		auto b = all_seq[j].find(a_it->first);
		if (b!= all_seq[j].end())
		{
			map1.push_back(a_it->second);
			map2.push_back(b->second);
		}

      }
      if (map1.size()>2)
      {
        
        double pearson = Math::pearsonCorrelationCoefficient(map1.begin(), map1.end(), map2.begin(), map2.end());
        LOG_INFO << "Found " << map1.size() << " matching peptides for " << i << " and " << j << endl;

		//case 1: correlation coefficient could be calculated
        if (!isnan(pearson))
        {
          matrix[i][j] = 1-fmax(0,pearson); 
          matrix[j][i] = 1-fmax(0, pearson);
          LOG_INFO << matrix[i][j] << endl;
        }

		//case 2: coefficient was not defined
        else
        {
          matrix[i][j] = 1;
          matrix[j][i] = 1;
          LOG_INFO << matrix[i][j] << endl;
        }
      }
	  //case 3: dataset not big enough
      else 
      {
        matrix[i][j] = 2; //no correlation
        matrix[j][i] = 2; 
        LOG_INFO << matrix[i][j] << endl;
      } 
    }
  }
  
}


//compute MST for the tree-based alignment
void computeSpanningTree(vector<vector<double>> matrix,vector<VertexPairDist>& queue)
{
	int V = matrix.size();
	vector<int> parent; //stores constructed MST 
	parent.resize(V);
	vector<int> key; // Key values used to pick minimum weight edge in cut 
	key.resize(V);
	vector<bool> mstSet; // To represent set of vertices not yet included in MST 
	mstSet.resize(V);

	// Initialize all keys as INFINITE 
	for (int i = 0; i < V; i++)
		key[i] = INT_MAX, mstSet[i] = false;

	// Always include first 1st vertex in MST. 
	// Make key 0 so that this vertex is picked as first vertex. 
	key[0] = 0;
	parent[0] = -1; // First node is always root of MST  

	// The MST will have V vertices 
	for (int count = 0; count < V - 1; count++)
	{
		// Pick the minimum key vertex from the  
		// set of vertices not yet included in MST 
		int u = findMin(key, mstSet, V);

		// Add the picked vertex to the MST Set 
		mstSet[u] = true;

		// Update key value and parent index of  
		// the adjacent vertices of the picked vertex.  
		// Consider only those vertices which are not  
		// yet included in MST 
		for (int v = 0; v < V; v++)

			if (matrix[u][v] && mstSet[v] == false && matrix[u][v] < key[v])
				parent[v] = u, key[v] = matrix[u][v];
	}

	// add MST to struct
	for (int i = 1; i < V; i++) {
		queue.emplace_back(parent[i], i, matrix[i][parent[i]]);
	}
	
	LOG_INFO << "Minimum spanning tree: " << endl;
	for (unsigned int i = 0; i < queue.size(); i++)
	{
		LOG_INFO << queue[i].vertex1 << " <-> " << queue[i].vertex2 << ", weight: " << queue[i].dist << endl;
	}

}



//sorting function for queue
static bool sortByScore(const VertexPairDist &lhs, const VertexPairDist &rhs) { return lhs.dist < rhs.dist; }

//alignment util
void align(vector<ConsensusMap>& to_align, vector<TransformationDescription>& transformations, int reference_index)

//void align(vector<ConsensusMap>& to_align, vector<TransformationDescription>& transformations)
{
  
  MapAlignmentAlgorithmIdentification algorithm;
  Param algo_params = getParam_().copy("algorithm:", true);
  algorithm.setParameters(algo_params);
  algorithm.setLogType(log_type_);
  
  algorithm.align(to_align,transformations,reference_index); //ADD REFERENCE INDEX
  //algorithm.align(to_align, transformations); //ADD REFERENCE INDEX

  Param model_params = getParam_().copy("model:", true);
  String model_type = model_params.getValue("type");
  if (model_type != "none")
  {
      model_params = model_params.copy(model_type + ":", true);
      for (vector<TransformationDescription>::iterator it = 
             transformations.begin(); it != transformations.end(); ++it)
      {	
        it->fitModel(model_type, model_params);
      }
  }
  
  for (unsigned int i = 0; i < transformations.size(); i++)
  {
      MapAlignmentTransformer::transformRetentionTimes(to_align[i], transformations[i]);
      addDataProcessing_(to_align[i], getProcessingInfo_(DataProcessing::ALIGNMENT));
  }
 
}

//Main alignment function
void alignSpanningTree(vector<VertexPairDist>& queue, vector<ConsensusMap>& maps,
                       StringList input_files, ConsensusMap& out_map)
{  
 
	//go through sorted Edges vector queue
	int test_i = 0;
  for (unsigned int i = 0; i < queue.size(); i++)
  {
	//in every iteration: two maps which are connected to current edge get aligned
	test_i++;

	vector<ConsensusMap> to_align;
    int A = queue[i].vertex1;
    int B = queue[i].vertex2;
    to_align.push_back(maps[A]);
    to_align.push_back(maps[B]);

    vector<TransformationDescription> transformations(to_align.size());
	//Use map with bigger RT range as reference for align() function
	int ref_index;
	maps[A].sortByRT();
	maps[B].sortByRT();
	double range_A = maps[A][(maps[A].size() - 1)].getRT() - maps[A][0].getRT();
	double range_B = maps[B][(maps[B].size() - 1)].getRT() - maps[B][0].getRT();
	
	//there are always only two maps in align (Indices 0 and 1)
	if (range_A >= range_B) {
		ref_index = 0;
	}
	else {
		ref_index = 1;
	}

    align(to_align,transformations,ref_index);

  
    //Grouping step
  
    ConsensusMap out;
    
 
    FeatureGroupingAlgorithmQT grouping;
    
    out.getColumnHeaders()[0].filename = input_files[A];
    out.getColumnHeaders()[0].size = maps[A].size();
    out.getColumnHeaders()[0].unique_id = maps[A].getColumnHeaders()[0].unique_id;
    out.getColumnHeaders()[1].filename = input_files[B];
    out.getColumnHeaders()[1].size = maps[B].size();
    out.getColumnHeaders()[1].unique_id = maps[B].getColumnHeaders()[1].unique_id;
    
    grouping.group(to_align,out);
    
    grouping.transferSubelements(to_align,out);
  
    out.applyMemberFunction(&UniqueIdInterface::setUniqueId);

    addDataProcessing_(out,getProcessingInfo_(DataProcessing::FEATURE_GROUPING));

    out.sortPeptideIdentificationsByMapIndex();
  
    
    if (i < queue.size()-1)
    {
      for (size_t q = i+1; q < queue.size(); ++q)
      {
        if (queue[q].vertex1==B) {queue[q].vertex1=A;}
        else if (queue[q].vertex2==B) {queue[q].vertex2=A;}
      }
    }
     
     
    maps[A] = out;
    maps[B].clear();
        
    map<Size, UInt> num_consfeat_of_size;
    for (auto cmit = out.begin();
         cmit !=out.end(); ++cmit)
    {
      ++num_consfeat_of_size[cmit->size()];
    }

    LOG_INFO << "Number of consensus features:" << endl;
    for (auto i = num_consfeat_of_size.rbegin();
         i != num_consfeat_of_size.rend(); ++i)
    {
      LOG_INFO << "  of size " << setw(2) << i->first << ": " << setw(6) 
               << i->second << endl;
    }
    LOG_INFO << "  total:      " << setw(6) << out.size() << endl;
    
    out_map = out;
    out.clear();
  }

} 

};


int main(int argc, const char** argv)
{
  MapAlignerTreeBased tool;
  return tool.main(argc,argv);
}
