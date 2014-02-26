/*
 * Rule.cpp
 *
 *  Created on: 18 Feb 2014
 *      Author: s0565741
 */

#include <limits>
#include <cassert>
#include "Rule.h"
#include "Parameter.h"
#include "LatticeArc.h"
#include "ConsistentPhrases.h"
#include "AlignedSentence.h"

using namespace std;

Rule::Rule(const LatticeArc &arc)
:m_isValid(true)
,m_canExtend(true)
,m_consistentPhrase(NULL)
{
	m_arcs.push_back(&arc);
}

Rule::Rule(const Rule &prevRule, const LatticeArc &arc)
:m_arcs(prevRule.m_arcs)
,m_isValid(true)
,m_canExtend(true)
,m_consistentPhrase(NULL)
{
	m_arcs.push_back(&arc);
}

Rule::~Rule() {
	// TODO Auto-generated destructor stub
}

bool Rule::IsValid(const Parameter &params) const
{
  if (!m_isValid) {
	  return false;
  }

  return true;
}

bool Rule::CanExtend(const Parameter &params) const
{
  if (!m_canExtend) {
	  return false;
  }

  return true;
}

void Rule::Prevalidate(const Parameter &params)
{
  int sourceStart = m_arcs.front()->GetStart();
  int sourceEnd = m_arcs.back()->GetEnd();

  if (sourceStart == 7 && sourceEnd == 8) {
	  Debug(cerr);
	  cerr << endl;
  }

  if (m_arcs.size() >= params.maxSymbolsSource) {
	  m_canExtend = false;
	  if (m_arcs.size() > params.maxSymbolsSource) {
		  m_isValid = false;
		  return;
	  }
  }

  // last word is a non-term
  if (m_arcs.back()->IsNonTerm()) {
	  const ConsistentRange *sourceRange = static_cast<const ConsistentRange *>(m_arcs.back());

	  // check number of non-terms
	  int numNonTerms = 0;
	  for (size_t i = 0; i < m_arcs.size(); ++i) {
		  const LatticeArc *arc = m_arcs[i];
		  if (arc->IsNonTerm()) {
			  ++numNonTerms;
		  }
	  }

	  if (numNonTerms > params.maxNonTerm) {
		  m_isValid = false;
		  m_canExtend = false;
		  return;
	  }

	  // check if non-term is big enough
	  if (sourceRange->GetWidth() < params.minHoleSource) {
		  m_isValid = false;
		  m_canExtend = false;
		  return;
	  }

	  // check if 2 consecutive non-terms in source
	  if (!params.nonTermConsecSource) {
		  size_t numSymbols = m_arcs.size();
		  if (numSymbols > 1 && m_arcs[numSymbols - 2]->IsNonTerm()) {
			  m_isValid = false;
			  m_canExtend = false;
			  return;
		  }
	  }

	  //check to see if it overlaps with any other non-terms
	  const ConsistentRange &lastTargetRange = sourceRange->GetOtherRange();

	  for (size_t i = 0; i < m_arcs.size() - 1; ++i) {
		  const LatticeArc *arc = m_arcs[i];

		  if (arc->IsNonTerm()) {
			  const ConsistentRange *sourceRange = static_cast<const ConsistentRange *>(arc);
			  const ConsistentRange &targetRange = sourceRange->GetOtherRange();

			  if (lastTargetRange.Overlap(targetRange)) {
				  m_isValid = false;
				  m_canExtend = false;
				  return;
			  }
		  }
	  }
  }

  if (params.requireAlignedWord) {
	  bool ok = false;
	  for (size_t i = 0; i < m_arcs.size(); ++i) {
		  const LatticeArc *arc = m_arcs[i];

		  if (!arc->IsNonTerm()) {
			  const Word *word = static_cast<const Word *>(arc);
			  if (word->GetAlignment().size()) {
				  ok = true;
				  break;
			  }
		  }
	  }

	  if (!ok) {
		  m_isValid = false;
	  }

  }
}

void Rule::Fillout(const ConsistentPhrases &consistentPhrases,
				const AlignedSentence &alignedSentence,
				const Parameter &params)
{
  if (!m_isValid) {
	  return;
  }

  // find out if it's a consistent phrase
  int sourceStart = m_arcs.front()->GetStart();
  int sourceEnd = m_arcs.back()->GetEnd();

  if (sourceStart == 7 && sourceEnd == 8) {
	  Debug(cerr);
	  cerr << endl;
  }

  if (sourceEnd - sourceStart + 1 >= params.maxSpan) {
	  m_canExtend = false;
	  if (sourceEnd - sourceStart + 1 > params.maxSpan) {
		  m_isValid = false;
	  }
  }

  int targetStart = numeric_limits<int>::max();
  int targetEnd = -1;

  for (size_t i = 0; i < m_arcs.size(); ++i) {
	  const LatticeArc &arc = *m_arcs[i];
	  if (arc.GetLowestAlignment() < targetStart) {
		  targetStart = arc.GetLowestAlignment();
	  }
	  if (arc.GetHighestAlignment() > targetEnd) {
		  targetEnd = arc.GetHighestAlignment();
	  }
  }

  m_consistentPhrase = consistentPhrases.Find(sourceStart, sourceEnd, targetStart, targetEnd);
  if (m_consistentPhrase == NULL) {
	  m_isValid = false;
	  return;
  }

  // everything looks ok, create target phrase
  // get a list of all target non-term
  vector<const ConsistentRange*> targetNonTerms;
  for (size_t i = 0; i < m_arcs.size(); ++i) {
	  const LatticeArc *arc = m_arcs[i];

	  if (arc->IsNonTerm()) {
		  const ConsistentRange *sourceRange = static_cast<const ConsistentRange *>(arc);
		  const ConsistentRange &targetRange = sourceRange->GetOtherRange();
		  targetNonTerms.push_back(&targetRange);
	  }
  }

  // targetNonTerms will be deleted element-by-element as it is used
  CreateTargetPhrase(alignedSentence.GetPhrase(Moses::Output),
		  targetStart,
		  targetEnd,
		  targetNonTerms);
  assert(targetNonTerms.size() == 0);

  if (m_targetArcs.size() >= params.maxSymbolsTarget) {
	  m_canExtend = false;
	  if (m_targetArcs.size() > params.maxSymbolsTarget) {
		  m_isValid = false;
	  }
  }

}

void Rule::CreateTargetPhrase(const Phrase &targetPhrase,
		int targetStart,
		int targetEnd,
		vector<const ConsistentRange*> &targetNonTerms)
{
	for (int pos = targetStart; pos <= targetEnd; ++pos) {
		const ConsistentRange *range = Overlap(pos, targetNonTerms);
		if (range) {
			// part of non-term.
			m_targetArcs.push_back(range);

			pos = range->GetEnd();
		}
		else {
			// just use the word
			const Word *word = targetPhrase[pos];
			m_targetArcs.push_back(word);
		}
	}
}

const ConsistentRange *Rule::Overlap(int pos, vector<const ConsistentRange*> &targetNonTerms)
{
	vector<const ConsistentRange*>::iterator iter;
	for (iter = targetNonTerms.begin(); iter != targetNonTerms.end(); ++iter) {
		const ConsistentRange *range = *iter;
		if (range->Overlap(pos)) {
			// is part of a non-term. Delete the range
			targetNonTerms.erase(iter);
			return range;
		}
	}

	return NULL;
}

Rule *Rule::Extend(const LatticeArc &arc) const
{
	Rule *ret = new Rule(*this, arc);

	return ret;
}

void Rule::Output(std::ostream &out, const std::vector<const LatticeArc*> &arcs) const
{
  for (size_t i = 0; i < arcs.size(); ++i) {
	  const LatticeArc &arc = *arcs[i];
	  arc.Output(out);
	  out << " ";
  }
}

void Rule::Output(std::ostream &out) const
{
	assert(m_consistentPhrase);

	Output(out, m_arcs);
	m_consistentPhrase->GetConsistentRange(Moses::Input).Output(out);

	out << "||| ";

	Output(out, m_targetArcs);
	m_consistentPhrase->GetConsistentRange(Moses::Output).Output(out);
}

void Rule::Debug(std::ostream &out) const
{
	Output(out, m_arcs);

	if (m_consistentPhrase) {
		m_consistentPhrase->GetConsistentRange(Moses::Input).Output(out);
	}
	else {
		out << "_BLANK_";
	}


	out << "||| ";

	Output(out, m_targetArcs);

	if (m_consistentPhrase) {
		m_consistentPhrase->GetConsistentRange(Moses::Output).Output(out);

		cerr << "||| m_consistentPhrase=";
		m_consistentPhrase->Debug(out);
	}
	else {
		out << "_BLANK_";
	}
}
