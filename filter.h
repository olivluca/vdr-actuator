#ifndef CFILTER_H
#define CFILTER_H

#include <vdr/filter.h>
#include <stdint.h>

#define MAXPMTENTRIES 64
#define MAXFILTERS 10
#define FILTERTIMEOUT 10

class SdtFilter;

class PatFilter : public cFilter {
private:
  int pSid[100];
  time_t lastPmtScan[MAXFILTERS];
  int pmtIndex;
  int pmtPid[MAXFILTERS];
  int Sids[MAXFILTERS];
  uint64_t pmtVersion[MAXPMTENTRIES];
  int numPmtEntries;
  bool PmtVersionChanged(int PmtPid, int Sid, int Version);
  int num,pit,pnum;
  SdtFilter *sdtFilter;
  bool endofScan;
  bool SidinSdt(int Sid);
  bool sdtfinished;
protected:
  virtual void Process(u_short Pid, u_char Tid, const u_char *Data, int Length);
public:
  PatFilter(void);
  void SetSdtFilter(SdtFilter* SdtFilter);
  virtual void SetStatus(bool On);
  bool EndOfScan() {return endofScan;};
  void Trigger(void);
  void SdtFinished(void) {sdtfinished=true;};
  };

int GetCaDescriptors(int Source, int Transponder, int ServiceId, const unsigned short *CaSystemIds, int BufSize, uchar *Data, bool &StreamFlag);

class SdtFilter : public cFilter {
friend class PatFilter;
private:
  static int channelsFound;
  static int newFound;
  int numSid,sid[100];
  cSectionSyncer sectionSyncer;
  PatFilter *patFilter;
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
