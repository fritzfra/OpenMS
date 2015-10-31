// --------------------------------------------------------------------------
//                   OpenMS -- Open-Source Mass Spectrometry
// --------------------------------------------------------------------------
// Copyright The OpenMS Team -- Eberhard Karls University Tuebingen,
// ETH Zurich, and Freie Universitaet Berlin 2002-2015.
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
// $Maintainer: Lars Nilse $
// $Authors: Lars Nilse $
// --------------------------------------------------------------------------

#include <OpenMS/KERNEL/StandardTypes.h>
#include <OpenMS/CONCEPT/Constants.h>
#include <OpenMS/TRANSFORMATIONS/FEATUREFINDER/MultiplexDeltaMasses.h>

#include <vector>
#include <algorithm>
#include <iostream>

using namespace std;

namespace OpenMS
{

  MultiplexDeltaMasses::DeltaMass::DeltaMass(double dm, LabelSet ls) :
    delta_mass(dm), label_set(ls)
  {
  }
  
  MultiplexDeltaMasses::DeltaMass::DeltaMass(double dm, String l) :
    delta_mass(dm), label_set()
  {
    label_set.insert(l);
  }

  MultiplexDeltaMasses::MultiplexDeltaMasses()
  {
  }

  MultiplexDeltaMasses::MultiplexDeltaMasses(vector<MultiplexDeltaMasses::DeltaMass> dm) :
    delta_masses_(dm)
  {
  }

  void MultiplexDeltaMasses::addDeltaMass(MultiplexDeltaMasses::DeltaMass dm)
  {
    delta_masses_.push_back(dm);
  }

  std::vector<MultiplexDeltaMasses::DeltaMass> MultiplexDeltaMasses::getDeltaMasses()
  {
    return delta_masses_;
  }

  unsigned MultiplexDeltaMasses::size() const
  {
    return delta_masses_.size();
  }

  double MultiplexDeltaMasses::getMassShiftAt(int i) const
  {
    return delta_masses_[i].delta_mass;
  }
  
  MultiplexDeltaMasses::LabelSet MultiplexDeltaMasses::getLabelSetAt(int i) const
  {
    return delta_masses_[i].label_set;
  }
  
  bool operator<(const MultiplexDeltaMasses &dm1, const MultiplexDeltaMasses &dm2)
  {
    if (dm1.size() != dm2.size())
    {
      // Search first for complete multiplets, then knock-out cases.
      return (dm1.size() > dm2.size());
    }
    else
    {
      for (unsigned i = 0; i < dm1.size(); ++i)
      {
        double ms1 = dm1.getMassShiftAt(i) - dm1.getMassShiftAt(0);
        double ms2 = dm2.getMassShiftAt(i) - dm2.getMassShiftAt(0);
        
        if (ms1 != ms2)
        {
          // Search first for cases without miscleavages.
          return (ms1 < ms2);
        }
      }
    }

    return (false);
  }
  
}
