/*
 * dvb_wrapper.h: actuator - A plugin for the Video Disk Recorder
 *
 * adapted from the wirbelscan plugin 
 * (License GPL, Written by  Winfried Koehler <w_scan AT gmx MINUS topmail DOT de>
 *  see  wirbel.htpc-forum.de/wirbelscan/index2.html) * adapted from the wirbelscan plugin.
 */
#ifndef __DVB_WRAPPER_H_
#define __DVB_WRAPPER_H_

#include <string>
#include <vdr/dvbdevice.h>
#include "extended_frontend.h"

void PrintDvbApi(std::string& s);

// DVB frontend capabilities
unsigned int GetFrontendStatus(cDevice* dev);

bool GetTerrCapabilities (cDevice* dev, bool* CodeRate, bool* Modulation, bool* Inversion, bool* Bandwidth, bool* Hierarchy, bool* TransmissionMode, bool* GuardInterval, bool* DvbT2);
bool GetCableCapabilities(cDevice* dev, bool* Modulation, bool* Inversion);
bool GetAtscCapabilities (cDevice* dev, bool* Modulation, bool* Inversion, bool* VSB, bool* QAM);
bool GetSatCapabilities  (cDevice* dev, bool* CodeRate, bool* Modulation, bool* RollOff, bool* DvbS2); //DvbS2: true if supported.

#endif
