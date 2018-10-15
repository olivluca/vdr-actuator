/*
 *  Class to scan one satellite transponder to 
 *  obtain service information.
 *
 *  Loosely based on the scan utility from dvb-apps and on
 *  sdt.c and pat.c from vdr
 */

#include <sys/ioctl.h>
#include <linux/dvb/dmx.h>
#include <libsi/section.h>
#include <libsi/descriptor.h>
#include <vdr/dvbdevice.h>


class cSectionBuf : public cListObject {
  friend class cChannelScanner;
private:
	int retries;
	int fd;
	int pid;
	int table_id;
	int table_id_ext;
	int section_version_number;
	uint8_t section_done[32];
	bool sectionfilter_done;
	unsigned char buf[1024];
	time_t timeout;
	time_t start_time;
	time_t running_time;
};

class cServiceInSdt : public cListObject {
  friend class cChannelScanner;
private:
       int service_id;
       bool pids_found;
       cServiceInSdt(int sid) {
             service_id=sid;
             pids_found=false;
       };      
};        

class cChannelScanner:public cThread {
private:
  cString dmx_devname;
  cChannel *transponderData;
  cSectionSyncer sectionSyncer;
#define MAX_RUNNING_FILTERS 10
  struct pollfd poll_fds[MAX_RUNNING_FILTERS];
  cSectionBuf* poll_section_bufs[MAX_RUNNING_FILTERS];
  cSectionBuf* SetupFilter(int pid, int tid, int tid_ext, int timeout);
  int totalFound;
  int newFound;
  cList<cServiceInSdt> services_in_sdt;
  cList<cSectionBuf> running_filters;
  cList<cSectionBuf> waiting_filters;
  
  
  void NewServiceInSdt(int service_id);
  void FoundPidsForService(int service_id);
  void CheckFoundPids(void);
  bool IsServiceInSdt(int service_id);
  

/* service_ids are guaranteed to be unique within one TP
 * (the DVB standards say theay should be unique within one
 * network, but in real life...)
 */
  bool ParsePat(const unsigned char *buf, int section_length,
		      int transport_stream_id);
  bool ParsePmt(const unsigned char *Data, u_short Pid);
  bool ParseSdt(const unsigned char *Data);

/**
 *   returns 0 when more sections are expected
 *	   1 when all sections are read on this pid
 *	   -1 on invalid table id
 */
  int ParseSection(cSectionBuf *s);
  int ReadSections(cSectionBuf *s);
  void UpdatePollFds(void);
  bool StartFilter(cSectionBuf* s);
  void StopFilter(cSectionBuf *s);
  void AddFilter(cSectionBuf *s);
  void RemoveFilter(cSectionBuf *s);
  void ReadFilters(void);
  void Cleanup(void);
  void ScanTransponder(void);
public:
  cChannelScanner(cDevice *ADevice, cChannel *TransponderData);
  ~cChannelScanner(void);
  int TotalFound() { return totalFound;};
  int NewFound() { return newFound;};
protected:
  void Action(void);  
};


