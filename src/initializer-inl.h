/*
  Copyright 2009 Andreas Biegert

  This file is part of the CS-BLAST package.

  The CS-BLAST package is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  The CS-BLAST package is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef CS_INITIALIZER_INL_H_
#define CS_INITIALIZER_INL_H_

#include "initializer.h"


#include <vector>

#include "exception.h"
#include "context_profile-inl.h"
#include "count_profile-inl.h"
#include "profile-inl.h"
#include "profile_library-inl.h"
#include "pseudocounts-inl.h"
#include "shared_ptr.h"
#include "utils.h"

namespace cs {

template<class Alphabet>
bool PriorCompare(const shared_ptr< ContextProfile<Alphabet> >& lhs,
                  const shared_ptr< ContextProfile<Alphabet> >& rhs) {
  return lhs->prior() > rhs->prior();
}

template< class Alphabet, template<class> class Graph >
void NormalizeTransitions(Graph<Alphabet>* graph) {
  const bool logspace = graph->transitions_logspace();
  if (logspace) graph->TransformTransitionsToLinSpace();

  for (int k = 0; k < graph->num_states(); ++k) {
    double sum = 0.0;
    for (int l = 0; l < graph->num_states(); ++l)
      if (graph->test_transition(k,l)) sum += graph->tr(k,l);

    if (sum != 0.0) {
      float fac = 1.0 / sum;
      for (int l = 0; l < graph->num_states(); ++l)
        if (graph->test_transition(k,l))
          graph->tr(k,l) = graph->tr(k,l) * fac;
    }
  }

  if (logspace) graph->TransformTransitionsToLogSpace();
}


template< class Alphabet, template<class> class Graph >
SamplingStateInitializer<Alphabet, Graph>::SamplingStateInitializer(
    profile_vector profiles,
    float sample_rate,
    const Pseudocounts<Alphabet>* pc,
    float pc_admixture)
    : profiles_(profiles),
      sample_rate_(sample_rate),
      pc_(pc),
      pc_admixture_(pc_admixture) {
  random_shuffle(profiles_.begin(), profiles_.end());
}

template< class Alphabet, template<class> class Graph >
void SamplingStateInitializer<Alphabet, Graph>::Init(Graph<Alphabet>& graph) const {
  // Iterate over randomly shuffled profiles; from each profile we sample a
  // fraction of profile windows.
  for (profile_iterator pi = profiles_.begin();
       pi != profiles_.end() && !graph.full(); ++pi) {
    if ((*pi)->num_cols() < graph.num_cols()) continue;

    // Prepare sample of indices
    std::vector<int> idx;
    for (int i = 0; i <= (*pi)->num_cols() - graph.num_cols(); ++i)
      idx.push_back(i);

    random_shuffle(idx.begin(), idx.end());

    const int sample_size = iround(sample_rate_ * idx.size());
    // sample only a fraction of the profile indices.
    idx.erase(idx.begin() + sample_size, idx.end());

    // Add sub-profiles at sampled indices to graph
    for (std::vector<int>::const_iterator i = idx.begin();
         i != idx.end() && !graph.full(); ++i) {
      CountProfile<Alphabet> p(**pi, *i, graph.num_cols());
      if (pc_) pc_->AddPseudocountsToProfile(ConstantAdmixture(pc_admixture_), &p);
      graph.AddState(p);
    }
  }
  if (!graph.full())
    throw Exception("Could not fully initialize all %i states. "
                    "Maybe too few training profiles provided?",
                    graph.num_states());
}

template< class Alphabet, template<class> class Graph >
LibraryBasedStateInitializer<Alphabet, Graph>::LibraryBasedStateInitializer(
    const ProfileLibrary<Alphabet>* lib)  : lib_(lib) {}

template< class Alphabet, template<class> class Graph >
void LibraryBasedStateInitializer<Alphabet, Graph>::Init(Graph<Alphabet>& graph) const {
  assert(lib_->num_cols() == graph.num_cols());

  typedef std::vector< shared_ptr< ContextProfile<Alphabet> > > ContextProfiles;
  typedef typename ContextProfiles::const_iterator ContextProfileIter;
  ContextProfiles profiles(lib_->begin(), lib_->end());
  sort(profiles.begin(), profiles.end(), PriorCompare<Alphabet>);

  for (ContextProfileIter it = profiles.begin(); it != profiles.end() &&
         !graph.full(); ++it) {
    graph.AddState(**it);
  }
  graph.states_logspace_ = lib_->logspace();
  graph.TransformStatesToLinSpace();

  if (!graph.full())
    throw Exception("Could not fully initialize all %i states. "
                    "Context library contains too few profiles!",
                    graph.num_states());
}


template< class Alphabet, template<class> class Graph >
void HomogeneousTransitionInitializer<Alphabet, Graph>::Init(
    Graph<Alphabet>& graph) const {
  float w = 1.0f / graph.num_states();
  for (int k = 0; k < graph.num_states(); ++k) {
    for (int l = 0; l < graph.num_states(); ++l) {
      graph(k,l) = w;
    }
  }
}

template< class Alphabet, template<class> class Graph >
void RandomTransitionInitializer<Alphabet, Graph>::Init(
    Graph<Alphabet>& graph) const {
  srand(static_cast<unsigned int>(clock()));
  for (int k = 0; k < graph.num_states(); ++k)
    for (int l = 0; l < graph.num_states(); ++l)
      graph(k,l) =
        static_cast<float>(rand()) / (static_cast<float>(RAND_MAX) + 1.0f);
  NormalizeTransitions(&graph);
}

template< class Alphabet, template<class> class Graph >
CoEmissionTransitionInitializer<Alphabet, Graph>::CoEmissionTransitionInitializer(
    const SubstitutionMatrix<Alphabet>* sm, float score_thresh)
    : co_emission_(sm), score_thresh_(score_thresh) {}

template< class Alphabet, template<class> class Graph >
void CoEmissionTransitionInitializer<Alphabet, Graph>::Init(
    Graph<Alphabet>& graph) const {
  const int ncols = graph.num_cols() - 1;

  for (int k = 0; k < graph.num_states(); ++k) {
    for (int l = 0; l < graph.num_states(); ++l) {
      float score = co_emission_(graph[k], graph[l], 1, 0, ncols);
      if (score > score_thresh_)
        graph(k,l) = score - score_thresh_;
    }
  }
  NormalizeTransitions(&graph);
}

}  // namespace cs

#endif  // CS_INITIALIZER_INL_H_
