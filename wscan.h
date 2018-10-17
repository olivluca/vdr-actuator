/*
 * wscan.h: integrate wirbelscan in the actuator plugin using its service interface 
 * 
 * adapted from the wirbelscancontrol plugin
 */

#ifndef _W_SCAN_H_
#define _W_SCAN_H_

#include <vdr/plugin.h>
#include <vdr/dvbdevice.h>
#include "wirbelscan_services.h"

class cWscanner {
private:
   cPlugin * wirbelscan;
   WIRBELSCAN_SERVICE::cWirbelscanInfo * info;
   WIRBELSCAN_SERVICE::cWirbelscanStatus status;
   WIRBELSCAN_SERVICE::cWirbelscanScanSetup setup;
   // user scan
   int frequency;
   int modulation;
   int fec_hp;
   int fec_lp;
   int bandwidth;
   int guard;
   int hierarchy;
   int transmission;
   int useNit;
   int symbolrate;
   int satsystem;
   int polarisation;
   int rolloff;
   uint32_t userdata[3];
   // auto scan
   int systems[6];
   int sat;
   int country;
   int source;
   int terrinv;
   int qam;
   int cableinv;
   int atsc;
   int srate;
   bool Tp_unsupported;
   bool ok;

   cString transponder;
public:
   cWscanner(void);
   virtual ~cWscanner();
   int GetSatId(int source);
   void StartScan(int id);
   void StartScanSingle(cChannel * Channel, cDvbTransponderParameters * Transponder); //FIXME
   void StopScan(void);
   void PutCommand(WIRBELSCAN_SERVICE::s_cmd command);
   void GetStatus(WIRBELSCAN_SERVICE::cWirbelscanStatus *status);
   bool Ok(void) { return ok; }
};

#endif
