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

#pragma once
#include <stdio.h>
#include <stdlib.h>


class SpanningGraph
{  
    
public:
    int V;   
	std::vector<std::vector<int>> adj;
    SpanningGraph(int V);   
    void addEdge(int v, int w);   
    bool containsCycle();  
    bool DFS(int v, int source, std::vector<bool>& visited); 
};
 
SpanningGraph::SpanningGraph(int V)
{
    this->V = V;
	std::vector<std::vector<int>> adj;
    for (OpenMS::Size i = 0; i < V; i++)
    {
      std::vector<int> a;
      adj.push_back(a);
    }
    this->adj = adj;
}
 
void SpanningGraph::addEdge(int v, int w)
{
    adj[v].push_back(w);
    adj[w].push_back(v);
}

bool SpanningGraph::containsCycle()
{
  std::vector<bool> visited(V);
  for (int i = 0; i < V; i++)
  {
    visited[i] = false;
  }
    
  for (int v = 0; v < V; v++)
  {
    if (!visited[v])
    {
      if (DFS(v, -1, visited))
      {
        return true;
      }
    }
  } 
  return false;

}

//DFS helper
bool SpanningGraph::DFS(int v, int source, std::vector<bool>& visited)
{
    visited[v] = true;
    for (unsigned int i = 0; i < adj[v].size(); i++)
    {
      if (!visited[adj[v][i]])
      {
        if (DFS(adj[v][i], v, visited)) {return true;}
      }
      else if (adj[v][i] != source) {return true;}
    }
    return false;
}
