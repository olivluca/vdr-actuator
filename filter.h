#ifndef CFILTER_H
#define CFILTER_H

#include <stdint.h>
#include <vdr/eit.h>
#include <vdr/filter.h>
#include <vdr/channels.h>
#include <libsi/section.h>
#include <libsi/descriptor.h>

#define CMAXPMTENTRIES 256
#define CMAXSIDENTRIES 256
#define MAXFILTERS 10
#define FILTERTIMEOUT 10

class SdtFilter;

class PatFilter : public cFilter {
private:
  int pSid[CMAXSIDENTRIES];
  int pSidCnt;
  
  time_t lastPmtScan[MAXFILTERS];
  int pmtIndex;
  int pmtPid[MAXFILTERS];
  
  int Sids[MAXFILTERS];
  uint64_t pmtVersion[CMAXPMTENTRIES];
  int numPmtEntries;
  bool PmtVersionChanged(int PmtPid, int Sid, int Version);
  int num,pit,pnum,numRunning;
  SdtFilter *sdtFilter;
  bool endofScan;
  bool SidinSdt(int Sid);
  bool sdtfinished;
  time_t lastFound;
  int waitingForGodot;
protected:
  virtual void Process(u_short Pid, u_char Tid, const u_char *Data, int Length);
public:
  PatFilter(void);
  void SetSdtFilter(SdtFilter* SdtFilter);
  virtual void SetStatus(bool On);
  bool EndOfScan() {return endofScan;};
  void Trigger(void);
  void SdtFinished(void) {sdtfinished=true;};
   time_t LastFoundTime(void){return lastFound;};
  void GetFoundNum(int &current, int &total);
 };

int GetCaDescriptors(int Source, int Transponder, int ServiceId, const unsigned short *CaSystemIds, int BufSize, uchar *Data, bool &StreamFlag);

class SdtFilter : public cFilter {
friend class PatFilter;
private:
  static int channelsFound;
  static int newFound;
  int numSid,sid[CMAXSIDENTRIES];
  int usefulSid[CMAXSIDENTRIES];
  int numUsefulSid;
  cSectionSyncer sectionSyncer;
  PatFilter *patFilter;
  int AddServiceType;
protected:
  virtual void Process(u_short Pid, u_char Tid, const u_char *Data, int Length);
public:
  SdtFilter(PatFilter *PatFilter);
  virtual void SetStatus(bool On);
  static void ResetFound(void);
  static int ChannelsFound(void);
  static int NewFound(void);
  };

#endif
