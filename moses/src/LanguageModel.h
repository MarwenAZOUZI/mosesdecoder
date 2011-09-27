// $Id$

/***********************************************************************
Moses - factored phrase-based language decoder
Copyright (C) 2006 University of Edinburgh

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
***********************************************************************/

#ifndef moses_LanguageModel_h
#define moses_LanguageModel_h

#include <string>
#include <vector>
#include "Factor.h"
#include "TypeDef.h"
#include "Util.h"
#include "FeatureFunction.h"
#include "Word.h"
#include "LanguageModelImplementation.h"

#ifdef WITH_THREADS
#include <boost/shared_ptr.hpp>
#endif

namespace Moses
{

class FactorCollection;
class Factor;
class Phrase;

//! Abstract base class which represent a language model on a contiguous phrase
class LanguageModel : public StatefulFeatureFunction
{
protected:
#ifdef WITH_THREADS
  // if we have threads, we also have boost and can let it handle thread-safe reference counting
  boost::shared_ptr<LanguageModelImplementation> m_implementation;
#else
  LanguageModelImplementation *m_implementation;
#endif
  bool m_enableOOVFeature;
  

  void ShiftOrPush(std::vector<const Word*> &contextFactor, const Word &word) const;

public:
  /**
   * Create a new language model
   */
  LanguageModel(ScoreIndexManager &scoreIndexManager, LanguageModelImplementation *implementation);

  /**
   * Create a new language model reusing an already loaded implementation
   */
  LanguageModel(ScoreIndexManager &scoreIndexManager, LanguageModel *implementation);

  // MJD: Added copy constructor
  LanguageModel(const LanguageModel& copy);
  
  virtual ~LanguageModel();

  //! see ScoreProducer.h
  size_t GetNumScoreComponents() const;

  /* whether this LM can be used on a particular phrase.
   * Should return false if phrase size = 0 or factor types required don't exists
   */
  bool Useable(const Phrase &phrase) const {
    return m_implementation->Useable(phrase);
  }

  /* calc total unweighted LM score of this phrase and return score via arguments.
   * Return scores should always be in natural log, regardless of representation with LM implementation.
   * Uses GetValue() of inherited class.
   * Useable() should be called beforehand on the phrase
   * \param fullScore scores of all unigram, bigram... of contiguous n-gram of the phrase
   * \param ngramScore score of only n-gram of order m_nGramOrder
   * \param oovCount number of LM OOVs
   */
  void CalcScore(
    const Phrase &phrase,
    float &fullScore,
    float &ngramScore,
    size_t &oovCount) const;

  //! max n-gram order of LM
  size_t GetNGramOrder() const {
    return m_implementation->GetNGramOrder();
  }

  virtual std::string GetScoreProducerDescription(unsigned idx=0) const {
    return m_implementation->GetScoreProducerDescription(idx);
  }

  float GetWeight() const;
  float GetOOVWeight() const;

  std::string GetScoreProducerWeightShortName(unsigned) const {
    return "lm";
  }

  void InitializeBeforeSentenceProcessing() {
    m_implementation->InitializeBeforeSentenceProcessing();
  }

  void CleanUpAfterSentenceProcessing() {
    m_implementation->CleanUpAfterSentenceProcessing();
  }

  virtual const FFState* EmptyHypothesisState(const InputType &input) const;

  virtual FFState* Evaluate(
    const Hypothesis& cur_hypo,
    const FFState* prev_state,
    ScoreComponentCollection* accumulator) const;

  FFState* EvaluateChart(
    const ChartHypothesis& cur_hypo,
    int featureID,
    ScoreComponentCollection* accumulator) const {
    return m_implementation->EvaluateChart(cur_hypo, featureID, accumulator, this);
  }

#ifdef WITH_THREADS
  // if multi-threaded return boost ptr 
  boost::shared_ptr<LanguageModelImplementation> 
#else // return normal LM ptr
  LanguageModelImplementation* 
#endif
  GetLMImplementation() const {
    return m_implementation;
  }
};

}

#endif
