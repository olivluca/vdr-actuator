#ifndef _SCAN_H
#define _SCAN_H
#include "actuator.h"
extern "C" {
#include <libdvbv5/dvb-file.h>
#include <libdvbv5/dvb-demux.h>
#include <libdvbv5/dvb-dev.h>
#include <libdvbv5/dvb-v5-std.h>
#include <libdvbv5/dvb-scan.h>
#include <libdvbv5/dvb-fe.h>
}
int PolarizationCharToEnum(char p);
char PolarizationEnumToChar(int p);


class dvbScanner:private cThread {
private:
   cMainMenuActuator *m_parent;
   cDvbDevice *m_device;
   dvb_file * m_entries;
   int m_progress;
   int m_channels;
   int m_newchannels;
   void StartScan(dvb_file * entries);
   struct dvb_v5_fe_parms *m_parms; 
public:
   dvbScanner(cMainMenuActuator *parent, cDvbDevice *device);
   virtual ~dvbScanner();
   void StartScan(cTransponders *transponders);
   void StartScan(int delsys, int modulation, int freq, char pol, int sr);
   void StopScan(void);
   bool Running(void) { return Active(); }
   int Progress(void) { return m_progress; }
   int Channels(void) { return m_channels; }
   int NewChannels(void) { return m_newchannels; }
protected:
    void Action(void);
};

int PolarizationCharToEnum(char p);
char PolarizationEnumToChar(int p);


#endif
