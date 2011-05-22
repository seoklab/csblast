// Copyright 2009, Andreas Biegert

#ifndef CS_CONTEXT_LIB_PSEUDOCOUNTS_INL_H_
#define CS_CONTEXT_LIB_PSEUDOCOUNTS_INL_H_

#include "library_pseudocounts.h"

namespace cs {

template<class Abc>
LibraryPseudocounts<Abc>::LibraryPseudocounts(const ContextLibrary<Abc>& lib,
                                              double weight_center,
                                              double weight_decay)
        : lib_(lib), emission_(lib.wlen(), weight_center, weight_decay) {}

template<class Abc>
void LibraryPseudocounts<Abc>::AddToSequence(const Sequence<Abc>& seq, Profile<Abc>& p) const {
    assert_eq(seq.length(), p.length());
    LOG(INFO) << "Adding library pseudocounts to sequence ...";

    Matrix<double> pp(seq.length(), lib_.size(), 0.0);  // posterior probabilities
    int len = static_cast<int>(seq.length());

    // Calculate and add pseudocounts for each sequence window X_i separately
#pragma omp parallel for schedule(static)
    for (int i = 0; i < len; ++i) {
        double* ppi = &pp[i][0];
        // Calculate posterior probability of state k given sequence window around 'i'
        CalculatePosteriorProbs(lib_, emission_, seq, i, ppi);
        // Calculate pseudocount vector P(a|X_i)
        double* pc = p[i];
        for (size_t a = 0; a < Abc::kSize; ++a) pc[a] = 0.0;
        for (size_t k = 0; k < lib_.size(); ++k) {
            for(size_t a = 0; a < Abc::kSize; ++a)
                pc[a] += ppi[k] * lib_[k].pc[a];
        }
        Normalize(&pc[0], Abc::kSize);
    }
}

template<class Abc>
void LibraryPseudocounts<Abc>::AddToProfile(const CountProfile<Abc>& cp, Profile<Abc>& p) const {
    assert_eq(cp.counts.length(), p.length());
    LOG(INFO) << "Adding library pseudocounts to profile ...";

    Matrix<double> pp(cp.counts.length(), lib_.size(), 0.0);  // posterior probs
    int len = static_cast<int>(cp.length());

    // Calculate and add pseudocounts for each sequence window X_i separately
#pragma omp parallel for schedule(static)
    for (int i = 0; i < len; ++i) {
        double* ppi = &pp[i][0];
        // Calculate posterior probability of state k given sequence window around 'i'
        CalculatePosteriorProbs(lib_, emission_, cp, i, ppi);
        // Calculate pseudocount vector P(a|X_i)
        double* pc = p[i];
        for (size_t a = 0; a < Abc::kSize; ++a) pc[a] = 0.0;
        for (size_t k = 0; k < lib_.size(); ++k) {
            for(size_t a = 0; a < Abc::kSize; ++a)
                pc[a] += ppi[k] * lib_[k].pc[a];
        }
        Normalize(&pc[0], Abc::kSize);
    }
}

template<class Abc>
void LibraryPseudocounts<Abc>::AddToPOHmm(const POHmm<Abc>* hmm, Profile<Abc>& p) const {
    LOG(INFO) << "Adding library pseudocounts to profile ...";
    Matrix<double> pp(hmm->size(), lib_.size(), 0.0);  // posterior probs
    int len = static_cast<int>(hmm->size());

#pragma omp parallel for schedule(static)
    for (int i = 1; i <= len; ++i) {
        double* ppi = pp[i - 1];
        // Calculate posterior probability of state k given sequence window around 'i'
        CalculatePosteriorProbs(lib_, emission_, hmm->g, i, ppi);
        // Calculate pseudocount vector P(a|X_i)
        double* pc = p[i - 1];
        for (size_t a = 0; a < Abc::kSize; ++a) pc[a] = 0.0;
        for (size_t k = 0; k < lib_.size(); ++k) {
            for(size_t a = 0; a < Abc::kSize; ++a)
                pc[a] += ppi[k] * lib_[k].pc[a];
        }
        Normalize(&pc[0], Abc::kSize);
    }
}

}  // namespace cs

#endif  // CS_CONTEXT_LIB_PSEUDOCOUNTS_INL_H_
