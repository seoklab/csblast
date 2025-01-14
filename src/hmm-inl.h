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

#ifndef CS_HMM_INL_H_
#define CS_HMM_INL_H_

#include "hmm.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <iostream>
#include <vector>

#include "exception.h"
#include "co_emission-inl.h"
#include "context_profile_state-inl.h"
#include "chain_graph-inl.h"
#include "count_profile-inl.h"
#include "log.h"
#include "profile-inl.h"
#include "shared_ptr.h"
#include "utils.h"

namespace cs {

template<class Alphabet>
const char* Hmm<Alphabet>::kClassID = "HMM";

template<class Alphabet>
Hmm<Alphabet>::Hmm(int num_states, int num_cols)
    : ChainGraph<Alphabet, ContextProfileState>(num_states, num_cols),
      states_logspace_(false)  {}

template<class Alphabet>
Hmm<Alphabet>::Hmm(FILE* fin)
    : ChainGraph<Alphabet, ContextProfileState>(),
      states_logspace_(false) {
  Read(fin);
}

template<class Alphabet>
Hmm<Alphabet>::Hmm(
    int num_states,
    int num_cols,
    const ::cs::StateInitializer<Alphabet, ContextProfileState>& st_init,
    const ::cs::TransitionInitializer<Alphabet, ContextProfileState>& tr_init)
    : ChainGraph<Alphabet, ContextProfileState>(num_states, num_cols),
      states_logspace_(false) {
  st_init.Init(*this);
  tr_init.Init(*this);
}

template<class Alphabet>
int Hmm<Alphabet>::AddState(const Profile<Alphabet>& profile) {
  if (full())
    throw Exception("HMM contains already %i states!", num_states());
  if (profile.num_cols() != num_cols())
    throw Exception("Profile to add as state has %i columns but should have %i!",
                    profile.num_cols(), num_cols());

  StatePtr sp(new State(states_.size(), num_states(), profile));
  sp->set_prior(1.0 / num_states());

  states_.push_back(sp);
  return states_.size() - 1;
}

template<class Alphabet>
inline void Hmm<Alphabet>::TransformStatesToLogSpace() {
  if (!states_logspace()) {
    for (StateIter si = states_begin(); si != states_end(); ++si)
      (*si)->TransformToLogSpace();
    states_logspace_ = true;
  }
}

template<class Alphabet>
inline void Hmm<Alphabet>::TransformStatesToLinSpace() {
  if (states_logspace()) {
    for (StateIter si = states_begin(); si != states_end(); ++si)
      (*si)->TransformToLinSpace();
    states_logspace_ = false;
  }
}

template<class Alphabet>
void Hmm<Alphabet>::ReadHeader(FILE* fin) {
  ChainGraph<Alphabet, ContextProfileState>::ReadHeader(fin);

  char buffer[kBufferSize];
  const char* ptr = buffer;

  // Read states logspace
  if (fgetline(buffer, kBufferSize, fin) && strstr(buffer, "STLOG")) {
    ptr = buffer;
    states_logspace_ = strtoi(ptr) == 1;
  } else {
    throw Exception("Bad format: HMM does not contain 'STLOG' record!");
  }
}

template<class Alphabet>
void Hmm<Alphabet>::WriteHeader(FILE* fout) const {
  ChainGraph<Alphabet, ContextProfileState>::WriteHeader(fout);
  fprintf(fout, "STLOG\t%i\n", states_logspace() ? 1 : 0);
}


template<class Alphabet>
SamplingStateInitializerHmm<Alphabet>::SamplingStateInitializerHmm(
    ProfileVec profiles,
    float sample_rate,
    const Pseudocounts<Alphabet>* pc,
    float pc_admixture)
    : SamplingStateInitializer<Alphabet, ContextProfileState>(profiles,
                                                              sample_rate,
                                                              pc,
                                                              pc_admixture) {}

template<class Alphabet>
LibraryBasedStateInitializerHmm<Alphabet>::LibraryBasedStateInitializerHmm(
    const ProfileLibrary<Alphabet>* lib)
    : LibraryBasedStateInitializer<Alphabet, ContextProfileState>(lib) {}

template<class Alphabet>
CoEmissionTransitionInitializerHmm<Alphabet>::CoEmissionTransitionInitializerHmm(
    const SubstitutionMatrix<Alphabet>* sm,
    float thresh)
    : CoEmissionTransitionInitializer<Alphabet, ContextProfileState>(sm, thresh) {}

}  // namespace cs

#endif  // CS_HMM_INL_H_
