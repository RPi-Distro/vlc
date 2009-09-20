/*
 * AtmoInput.cpp:  abstract class for retrieving precalculated image data
 * from different sources in the live view mode
 *
 *
 * See the README.txt file for copyright information and how to reach the author(s).
 *
 * $Id: 3cf6ef1c0fe36f5a37bdaf3f198732bf0c9e328c $
 */

#include "AtmoInput.h"

CAtmoInput::CAtmoInput(CAtmoDynData *pAtmoDynData)
{
  this->m_pAtmoDynData = pAtmoDynData;
}

CAtmoInput::~CAtmoInput(void)
{
}

void CAtmoInput::WaitForNextFrame(DWORD timeout)
{
    return;
}

