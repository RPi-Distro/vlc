/*
 * AtmoInput.cpp:  abstract class for retrieving precalculated image data
 * from different sources in the live view mode
 *
 *
 * See the README.txt file for copyright information and how to reach the author(s).
 *
 * $Id: fc0ba660bd13f81cbdb7fd288ae31f014faf4765 $
 */
#include "AtmoDefs.h"
#include "AtmoInput.h"

#if defined(_ATMO_VLC_PLUGIN_)
CAtmoInput::CAtmoInput(CAtmoDynData *pAtmoDynData) : CThread(pAtmoDynData->getAtmoFilter())
{
  m_pAtmoDynData         = pAtmoDynData;
  m_pAtmoColorCalculator = new CAtmoColorCalculator(pAtmoDynData->getAtmoConfig());
}
#else
CAtmoInput::CAtmoInput(CAtmoDynData *pAtmoDynData)
{
  m_pAtmoDynData = pAtmoDynData;
  m_pAtmoColorCalculator = new CAtmoColorCalculator(pAtmoDynData->getAtmoConfig());
}
#endif

CAtmoInput::~CAtmoInput(void)
{
  delete m_pAtmoColorCalculator;
}

