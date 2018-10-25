/*
 * scan.h: actuator - A plugin for the Video Disk Recordes 
 *
 * adapted from statemachine.h of the wirbelscan plugin 
 * (License GPL, Written by  Winfried Koehler <w_scan AT gmx MINUS topmail DOT de>
 *  see  wirbel.htpc-forum.de/wirbelscan/index2.html)
 *
 */

#ifndef __SCAN_H_
#define __SCAN_H_

#include <vdr/tools.h>
#include "actuator.h"
#include "common.h"
#include "scanfilter.h"

class cDevice;
class TChannel;

class dvbScanner:private cThread {
private:
   cMainMenuActuator *m_parent;
   cDvbDevice *m_device;
   int m_progress;
   int m_channels;
   int m_newchannels;
   cSource *m_source;
   bool m_abort;
   TChannels NewChannels;
   TChannels NewTransponders;
   TSdtData SdtData;
   TNitData NitData;
   void AddChannels(void);
   void ResetLists(void);
public:
   dvbScanner(cMainMenuActuator *parent, cDvbDevice *device);
   virtual ~dvbScanner();
   void StartScan(cSource *source, cTransponders *transponders);
   void StartScan(cSource *source, int delsys, int modulation, int freq, char pol, int sr, int fec);
   void StopScan(void);
   bool Running(void) { return Active(); }
   int Progress(void) { return m_progress; }
   int ChannelsCount(void) { return m_channels; }
   int NewChannelsCount(void) { return m_newchannels; }
protected:
    void Action(void);
};


#endif
