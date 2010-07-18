/*
* DmxTools.h: functions to convert , or ; seperatered numbers into an integer array
 *
 * See the README.txt file for copyright information and how to reach the author(s).
 *
 * $Id: a714bc973ee992a86be9958967ed5e5e83e581bc $
 */

#ifndef _dmxtools_h_
#define _dmxtools_h_

int *ConvertDmxStartChannelsToInt(int numChannels, char *startChannels);
char *ConvertDmxStartChannelsToString(int numChannels, int *startChannels);
int IsValidDmxStartString(char *startChannels);

#endif
