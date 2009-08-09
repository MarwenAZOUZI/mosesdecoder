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

#include <cassert>
#include <algorithm>
#include "TargetPhrase.h"
#include "PhraseDictionaryMemory.h"
#include "GenerationDictionary.h"
#include "LanguageModel.h"
#include "StaticData.h"
#include "ScoreIndexManager.h"
#include "LMList.h"
#include "ScoreComponentCollection.h"
#include "DummyScoreProducers.h"
#include "Util.h"

using namespace std;

namespace Moses
{
bool TargetPhrase::wordalignflag=StaticData::Instance().UseAlignmentInfo();
bool TargetPhrase::printalign=StaticData::Instance().PrintAlignmentInfo();

//bool TargetPhrase::wordalignflag;
//bool TargetPhrase::printalign;

TargetPhrase::TargetPhrase(FactorDirection direction, size_t reserveSize)
: Phrase(direction, reserveSize)
, m_transScore(0.0)
, m_ngramScore(0.0)
, m_fullScore(0.0)
, m_sourcePhrase(0)
, m_debugOutput(NULL)
{
	assert(direction == Output);
	wordalignflag=StaticData::Instance().UseAlignmentInfo();
	printalign=StaticData::Instance().PrintAlignmentInfo();
}

void TargetPhrase::SetScore()
{ // used when creating translations of unknown words:
	m_transScore = m_ngramScore = 0;
	m_fullScore = - StaticData::Instance().GetWeightWordPenalty();
}

#ifdef HAVE_PROTOBUF
void TargetPhrase::WriteToRulePB(hgmert::Rule* pb) const {
	pb->add_trg_words("[X,1]");
	for (size_t pos = 0 ; pos < GetSize() ; pos++)
		pb->add_trg_words(GetWord(pos)[0]->GetString());
}
#endif

void TargetPhrase::SetScore(float score)
{
	//we use an existing score producer to figure out information for score setting (number of scores and weights)
	//TODO: is this a good idea?
	ScoreProducer* prod = StaticData::Instance().GetPhraseDictionaries()[0];

	//get the weight list
	unsigned int id = prod->GetScoreBookkeepingID();

	const vector<float> &allWeights = StaticData::Instance().GetAllWeights();

	size_t beginIndex = StaticData::Instance().GetScoreIndexManager().GetBeginIndex(id);
	size_t endIndex = StaticData::Instance().GetScoreIndexManager().GetEndIndex(id);

	vector<float> weights;

	std::copy(allWeights.begin() +beginIndex, allWeights.begin() + endIndex,std::back_inserter(weights));

	//find out how many items are in the score vector for this producer
	size_t numScores = prod->GetNumScoreComponents();

	//divide up the score among all of the score vectors
	vector <float> scoreVector(numScores,score/numScores);

	//Now we have what we need to call the full SetScore method
	SetScore(prod,scoreVector,weights,StaticData::Instance().GetWeightWordPenalty(),StaticData::Instance().GetAllLM());
}

void TargetPhrase::SetScore(const ScoreProducer* translationScoreProducer,
														const Scores &scoreVector,
														const vector<float> &weightT,
														float weightWP, const LMList &languageModels)
{
	assert(weightT.size() == scoreVector.size());
	// calc average score if non-best

	m_transScore = std::inner_product(scoreVector.begin(), scoreVector.end(), weightT.begin(), 0.0f);
	m_scoreBreakdown.PlusEquals(translationScoreProducer, scoreVector);

  // Replicated from TranslationOptions.cpp
	float totalFutureScore = 0;
	float totalNgramScore  = 0;
	float totalFullScore   = 0;

	LMList::const_iterator lmIter;
	for (lmIter = languageModels.begin(); lmIter != languageModels.end(); ++lmIter)
	{
		const LanguageModel &lm = **lmIter;

		if (lm.Useable(*this))
		{ // contains factors used by this LM
			const float weightLM = lm.GetWeight();
			float fullScore, nGramScore;

			lm.CalcScore(*this, fullScore, nGramScore);
			m_scoreBreakdown.Assign(&lm, nGramScore);

			// total LM score so far
			totalNgramScore  += nGramScore * weightLM;
			totalFullScore   += fullScore * weightLM;

		}
	}
  m_ngramScore = totalNgramScore;

	m_fullScore = m_transScore + totalFutureScore + totalFullScore
		- (this->GetSize() * weightWP);	 // word penalty
}

void TargetPhrase::SetScoreChart(const ScoreProducer* translationScoreProducer,
														const Scores &scoreVector
														,const vector<float> &weightT
														,const LMList &languageModels
														,bool calcWordPenalty)
{
	const StaticData &staticData = StaticData::Instance();

	assert(weightT.size() == scoreVector.size());

	// calc average score if non-best
	m_transScore = std::inner_product(scoreVector.begin(), scoreVector.end(), weightT.begin(), 0.0f);
	m_scoreBreakdown.PlusEquals(translationScoreProducer, scoreVector);

  // Replicated from TranslationOptions.cpp
	float totalNgramScore  = 0;
	float totalFullScore   = 0;

	LMList::const_iterator lmIter;
	for (lmIter = languageModels.begin(); lmIter != languageModels.end(); ++lmIter)
	{
		const LanguageModel &lm = **lmIter;

		if (lm.Useable(*this))
		{ // contains factors used by this LM
			const float weightLM = lm.GetWeight();
			float fullScore, nGramScore;

			lm.CalcScore(*this, fullScore, nGramScore);
			m_scoreBreakdown.Assign(&lm, nGramScore);

			// total LM score so far
			totalNgramScore  += nGramScore * weightLM;
			totalFullScore   += fullScore * weightLM;
		}
	}

	// word penalty
	if (calcWordPenalty)
	{
		size_t wordCount = GetNumTerminals();
		m_scoreBreakdown.Assign(staticData.GetWordPenaltyProducer(), - (float) wordCount * 0.434294482); // TODO log -> ln ??
	}

	m_fullScore = m_scoreBreakdown.GetWeightedScore() - totalNgramScore + totalFullScore;
}

void TargetPhrase::SetScore(const ScoreProducer* producer, const Scores &scoreVector)
{ // used when creating translations of unknown words (chart decoding)
	m_scoreBreakdown.Assign(producer, scoreVector);
	m_transScore = m_ngramScore = 0;
	m_fullScore = m_scoreBreakdown.GetWeightedScore();
}

void TargetPhrase::SetWeights(const ScoreProducer* translationScoreProducer, const vector<float> &weightT)
{
	// calling this function in case of confusion net input is undefined
	assert(StaticData::Instance().GetInputType()==SentenceInput);

	/* one way to fix this, you have to make sure the weightT contains (in
     addition to the usual phrase translation scaling factors) the input
     weight factor as last element
	*/

	m_transScore = m_scoreBreakdown.PartialInnerProduct(translationScoreProducer, weightT);
}

void TargetPhrase::ResetScore()
{
	m_fullScore = m_ngramScore = 0;
	m_scoreBreakdown.ZeroAll();
}

TargetPhrase *TargetPhrase::MergeNext(const TargetPhrase &inputPhrase) const
{
	if (! IsCompatible(inputPhrase))
	{
		return NULL;
	}

	// ok, merge
	TargetPhrase *clone				= new TargetPhrase(*this);
	clone->m_sourcePhrase = m_sourcePhrase;
	int currWord = 0;
	const size_t len = GetSize();
	for (size_t currPos = 0 ; currPos < len ; currPos++)
	{
		const Word &inputWord	= inputPhrase.GetWord(currPos);
		Word &cloneWord = clone->GetWord(currPos);
		cloneWord.Merge(inputWord);

		currWord++;
	}

	return clone;
}

void TargetPhrase::CreateAlignmentInfo(const std::list<std::pair<size_t,size_t> > &alignmentInfo)
{
	m_alignmentInfo.AddAlignment(alignmentInfo);
}

TO_STRING_BODY(TargetPhrase);

std::ostream& operator<<(std::ostream& os, const TargetPhrase& tp)
{
	const string *str = tp.GetDebugOutput();

	if (str != NULL)
	{
		os << *str;
	}
	else
	{
		os << static_cast<const Phrase&>(tp) << ", "
			<< tp.GetAlignmentInfo() << ","
			<< " fullScore=" << tp.m_fullScore << " "
			<< tp.GetScoreBreakdown();
	}
	return os;
}

}
