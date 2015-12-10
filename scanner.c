/*
 *  Class to scan one satellite transponder to 
 *  obtain service information.
 *
 *  Loosely based on the scan utility from dvb-apps and on
 *  sdt.c and pat.c from vdr
 */

#include "scanner.h"


static int get_bit(uint8_t *bitfield, int bit)
{
  return(bitfield[bit/8] >> (bit % 8)) & 1;
}

static void set_bit(uint8_t *bitfield, int bit)
{
  bitfield[bit/8] |= 1 << (bit % 8);
}

static int scanverbosity = 2;

#define dprintf(level, fmt...)			\
	do {					\
		if (level <= scanverbosity)		\
			fprintf(stderr, fmt);	\
	} while (0)

#define dpprintf(level, fmt, args...) \
	dprintf(level, "%s:%d: " fmt, __FUNCTION__, __LINE__ , ##args)

#define fatal(fmt, args...) do { dpprintf(-1, "FATAL: " fmt , ##args); exit(1); } while(0)
#define error(msg...) dprintf(0, "ERROR: " msg)
#define errorn(msg) dprintf(0, "%s:%d: ERROR: " msg ": %d %m\n", __FUNCTION__, __LINE__, errno)
#define warning(msg...) dprintf(1, "WARNING: " msg)
#define info(msg...) dprintf(2, msg)
#define verbose(msg...) dprintf(3, msg)
#define moreverbose(msg...) dprintf(4, msg)
#define debug(msg...) dpprintf(5, msg)
#define verbosedebug(msg...) dpprintf(6, msg)

/****************************************************************************
   The following classes:
      cCaDescriptor
      cCaDescriptors
      cCaDescriptorsHandler
   are copied verbatim from pat.c
 ****************************************************************************/
      
      
// --- cCaDescriptor ---------------------------------------------------------

class cCaDescriptor : public cListObject {
private:
  int caSystem;
  int esPid;
  int length;
  uchar *data;
public:
  cCaDescriptor(int CaSystem, int CaPid, int EsPid, int Length, const uchar *Data);
  virtual ~cCaDescriptor();
  bool operator== (const cCaDescriptor &arg) const;
  int CaSystem(void) { return caSystem; }
  int EsPid(void) { return esPid; }
  int Length(void) const { return length; }
  const uchar *Data(void) const { return data; }
  };

cCaDescriptor::cCaDescriptor(int CaSystem, int CaPid, int EsPid, int Length, const uchar *Data)
{
  caSystem = CaSystem;
  esPid = EsPid;
  length = Length + 6;
  data = MALLOC(uchar, length);
  data[0] = SI::CaDescriptorTag;
  data[1] = length - 2;
  data[2] = (caSystem >> 8) & 0xFF;
  data[3] =  caSystem       & 0xFF;
  data[4] = ((CaPid   >> 8) & 0x1F) | 0xE0;
  data[5] =   CaPid         & 0xFF;
  if (Length)
     memcpy(&data[6], Data, Length);
}

cCaDescriptor::~cCaDescriptor()
{
  free(data);
}

bool cCaDescriptor::operator== (const cCaDescriptor &arg) const
{
  return esPid == arg.esPid && length == arg.length && memcmp(data, arg.data, length) == 0;
}

// --- cCaDescriptors --------------------------------------------------------

class cCaDescriptors : public cListObject {
private:
  int source;
  int transponder;
  int serviceId;
  int numCaIds;
  int caIds[MAXCAIDS + 1];
  cList<cCaDescriptor> caDescriptors;
  void AddCaId(int CaId);
public:
  cCaDescriptors(int Source, int Transponder, int ServiceId);
  bool operator== (const cCaDescriptors &arg) const;
  bool Is(int Source, int Transponder, int ServiceId);
  bool Is(cCaDescriptors * CaDescriptors);
  bool Empty(void) { return caDescriptors.Count() == 0; }
  void AddCaDescriptor(SI::CaDescriptor *d, int EsPid);
  int GetCaDescriptors(const int *CaSystemIds, int BufSize, uchar *Data, int EsPid);
  const int *CaIds(void) { return caIds; }
  };

cCaDescriptors::cCaDescriptors(int Source, int Transponder, int ServiceId)
{
  source = Source;
  transponder = Transponder;
  serviceId = ServiceId;
  numCaIds = 0;
  caIds[0] = 0;
}

bool cCaDescriptors::operator== (const cCaDescriptors &arg) const
{
  const cCaDescriptor *ca1 = caDescriptors.First();
  const cCaDescriptor *ca2 = arg.caDescriptors.First();
  while (ca1 && ca2) {
        if (!(*ca1 == *ca2))
           return false;
        ca1 = caDescriptors.Next(ca1);
        ca2 = arg.caDescriptors.Next(ca2);
        }
  return !ca1 && !ca2;
}

bool cCaDescriptors::Is(int Source, int Transponder, int ServiceId)
{
  return source == Source && transponder == Transponder && serviceId == ServiceId;
}

bool cCaDescriptors::Is(cCaDescriptors *CaDescriptors)
{
  return Is(CaDescriptors->source, CaDescriptors->transponder, CaDescriptors->serviceId);
}

void cCaDescriptors::AddCaId(int CaId)
{
  if (numCaIds < MAXCAIDS) {
     for (int i = 0; i < numCaIds; i++) {
         if (caIds[i] == CaId)
            return;
         }
     caIds[numCaIds++] = CaId;
     caIds[numCaIds] = 0;
     }
}

void cCaDescriptors::AddCaDescriptor(SI::CaDescriptor *d, int EsPid)
{
  cCaDescriptor *nca = new cCaDescriptor(d->getCaType(), d->getCaPid(), EsPid, d->privateData.getLength(), d->privateData.getData());
  for (cCaDescriptor *ca = caDescriptors.First(); ca; ca = caDescriptors.Next(ca)) {
      if (*ca == *nca) {
         delete nca;
         return;
         }
      }
  AddCaId(nca->CaSystem());
  caDescriptors.Add(nca);
//#define DEBUG_CA_DESCRIPTORS 1
#ifdef DEBUG_CA_DESCRIPTORS
  char buffer[1024];
  char *q = buffer;
  q += sprintf(q, "CAM: %04X %5d %5d %04X %04X -", source, transponder, serviceId, d->getCaType(), EsPid);
  for (int i = 0; i < nca->Length(); i++)
      q += sprintf(q, " %02X", nca->Data()[i]);
  dsyslog("%s", buffer);
#endif
}

// EsPid is to select the "type" of CaDescriptor to be returned
// >0 - CaDescriptor for the particular esPid
// =0 - common CaDescriptor
// <0 - all CaDescriptors regardless of type (old default)

int cCaDescriptors::GetCaDescriptors(const int *CaSystemIds, int BufSize, uchar *Data, int EsPid)
{
  if (!CaSystemIds || !*CaSystemIds)
     return 0;
  if (BufSize > 0 && Data) {
     int length = 0;
     for (cCaDescriptor *d = caDescriptors.First(); d; d = caDescriptors.Next(d)) {
         if (EsPid < 0 || d->EsPid() == EsPid) {
            const int *caids = CaSystemIds;
            do {
               if (d->CaSystem() == *caids) {
                  if (length + d->Length() <= BufSize) {
                     memcpy(Data + length, d->Data(), d->Length());
                     length += d->Length();
                     }
                  else
                     return -1;
                  }
               } while (*++caids);
            }
         }
     return length;
     }
  return -1;
}

// --- cCaDescriptorHandler --------------------------------------------------

class cCaDescriptorHandler : public cList<cCaDescriptors> {
private:
  cMutex mutex;
public:
  int AddCaDescriptors(cCaDescriptors *CaDescriptors);
      // Returns 0 if this is an already known descriptor,
      // 1 if it is an all new descriptor with actual contents,
      // and 2 if an existing descriptor was changed.
  int GetCaDescriptors(int Source, int Transponder, int ServiceId, const int *CaSystemIds, int BufSize, uchar *Data, int EsPid);
  };

int cCaDescriptorHandler::AddCaDescriptors(cCaDescriptors *CaDescriptors)
{
  cMutexLock MutexLock(&mutex);
  for (cCaDescriptors *ca = First(); ca; ca = Next(ca)) {
      if (ca->Is(CaDescriptors)) {
         if (*ca == *CaDescriptors) {
            delete CaDescriptors;
            return 0;
            }
         Del(ca);
         Add(CaDescriptors);
         return 2;
         }
      }
  Add(CaDescriptors);
  return CaDescriptors->Empty() ? 0 : 1;
}

int cCaDescriptorHandler::GetCaDescriptors(int Source, int Transponder, int ServiceId, const int *CaSystemIds, int BufSize, uchar *Data, int EsPid)
{
  cMutexLock MutexLock(&mutex);
  for (cCaDescriptors *ca = First(); ca; ca = Next(ca)) {
      if (ca->Is(Source, Transponder, ServiceId))
         return ca->GetCaDescriptors(CaSystemIds, BufSize, Data, EsPid);
      }
  return 0;
}

cCaDescriptorHandler CaDescriptorHandler;

int GetCaDescriptors(int Source, int Transponder, int ServiceId, const int *CaSystemIds, int BufSize, uchar *Data, int EsPid)
{
  return CaDescriptorHandler.GetCaDescriptors(Source, Transponder, ServiceId, CaSystemIds, BufSize, Data, EsPid);
}


// --- cChannelScanner --------------------------------------------------------

void cChannelScanner::NewServiceInSdt(int service_id)
{
  cServiceInSdt *s=new cServiceInSdt(service_id);
  services_in_sdt.Add(s);
}

void cChannelScanner::FoundPidsForService(int service_id)
{
  for (cServiceInSdt *s=services_in_sdt.First(); s; s=services_in_sdt.Next(s))
    if (s->service_id==service_id) {
      s->pids_found=true;
      return;
    }  
}

void cChannelScanner::CheckFoundPids(void)
{
  for (cServiceInSdt *s=services_in_sdt.First(); s; s=services_in_sdt.Next(s))
    if (!s->pids_found) {
      LOCK_CHANNELS_READ;
      const cChannel *Channel = Channels->GetByServiceID(transponderData->Source(), transponderData->Transponder(), s->service_id);
      warning("Pids not found for service id %d %s\n",s->service_id, Channel ? Channel->Name() : "");
    }   
}

bool cChannelScanner::IsServiceInSdt(int service_id)
{
  for (cServiceInSdt *s=services_in_sdt.First(); s; s=services_in_sdt.Next(s))
    if (s->service_id==service_id) 
      return true;
  return false;    
}

bool cChannelScanner::ParsePat(const unsigned char *buf, int section_length,
		      int transport_stream_id)
{
  (void)transport_stream_id;

  int pmt_pid;
  while (section_length > 0) {
    int service_id = (buf[0] << 8) | buf[1];

    if (service_id == 0)
      goto skip; /* nit pid entry */

    if (!IsServiceInSdt(service_id)) {
      warning("Skipping service %d\n",service_id);
      goto skip; /* service not found/used in SDT */
    }        

    pmt_pid = ((buf[2] & 0x1f) << 8) | buf[3];
    if (pmt_pid)
      AddFilter(SetupFilter(pmt_pid, 0x02, service_id, 5));

skip:
    buf += 4;
    section_length -= 4;
  };
  return true;
}

bool cChannelScanner::ParsePmt(const unsigned char *Data)
{
     SI::PMT pmt(Data, false);
     if (!pmt.CheckCRCAndParse())
        return false;
     LOCK_CHANNELS_WRITE;   
     cChannel *Channel = (cChannel *) Channels->GetByServiceID(transponderData->Source(), transponderData->Transponder(), pmt.getServiceId());
     /**************************************************
      Following part copied from pat.c
      Modified lines marked with //LO
      **************************************************/
     if (Channel) {
        SI::CaDescriptor *d;
        cCaDescriptors *CaDescriptors = new cCaDescriptors(Channel->Source(), Channel->Transponder(), Channel->Sid());
        // Scan the common loop:
        for (SI::Loop::Iterator it; (d = (SI::CaDescriptor*)pmt.commonDescriptors.getNext(it, SI::CaDescriptorTag)); ) {
            CaDescriptors->AddCaDescriptor(d, 0);
            delete d;
            }
        // Scan the stream-specific loop:
        SI::PMT::Stream stream;
        int Vpid = 0;
        int Ppid = pmt.getPCRPid();
        int Vtype = 0;
        int Apids[MAXAPIDS + 1] = { 0 }; // these lists are zero-terminated
        int Atypes[MAXAPIDS + 1] = { 0 };
        int Dpids[MAXDPIDS + 1] = { 0 };
        int Dtypes[MAXDPIDS + 1] = { 0 };
        int Spids[MAXSPIDS + 1] = { 0 };
        uchar SubtitlingTypes[MAXSPIDS + 1] = { 0 };
        uint16_t CompositionPageIds[MAXSPIDS + 1] = { 0 };
        uint16_t AncillaryPageIds[MAXSPIDS + 1] = { 0 };
        char ALangs[MAXAPIDS][MAXLANGCODE2] = { "" };
        char DLangs[MAXDPIDS][MAXLANGCODE2] = { "" };
        char SLangs[MAXSPIDS][MAXLANGCODE2] = { "" };
        int Tpid = 0;
        int NumApids = 0;
        int NumDpids = 0;
        int NumSpids = 0;
        for (SI::Loop::Iterator it; pmt.streamLoop.getNext(stream, it); ) {
            bool ProcessCaDescriptors = false;
            int esPid = stream.getPid();
            switch (stream.getStreamType()) {
              case 1: // STREAMTYPE_11172_VIDEO
              case 2: // STREAMTYPE_13818_VIDEO
              case 0x1B: // MPEG4
                      Vpid = esPid;
                      Ppid = pmt.getPCRPid();
                      Vtype = stream.getStreamType();
                      ProcessCaDescriptors = true;
                      break;
              case 3: // STREAMTYPE_11172_AUDIO
              case 4: // STREAMTYPE_13818_AUDIO
              case 0x0F: // ISO/IEC 13818-7 Audio with ADTS transport syntax
              case 0x11: // ISO/IEC 14496-3 Audio with LATM transport syntax
                      {
                      if (NumApids < MAXAPIDS) {
                         Apids[NumApids] = esPid;
                         Atypes[NumApids] = stream.getStreamType();
                         SI::Descriptor *d;
                         for (SI::Loop::Iterator it; (d = stream.streamDescriptors.getNext(it)); ) {
                             switch (d->getDescriptorTag()) {
                               case SI::ISO639LanguageDescriptorTag: {
                                    SI::ISO639LanguageDescriptor *ld = (SI::ISO639LanguageDescriptor *)d;
                                    SI::ISO639LanguageDescriptor::Language l;
                                    char *s = ALangs[NumApids];
                                    int n = 0;
                                    for (SI::Loop::Iterator it; ld->languageLoop.getNext(l, it); ) {
                                        if (*ld->languageCode != '-') { // some use "---" to indicate "none"
                                           if (n > 0)
                                              *s++ = '+';
                                           strn0cpy(s, I18nNormalizeLanguageCode(l.languageCode), MAXLANGCODE1);
                                           s += strlen(s);
                                           if (n++ > 1)
                                              break;
                                           }
                                        }
                                    }
                                    break;
                               default: ;
                               }
                             delete d;
                             }
                         NumApids++;
                         }
                      ProcessCaDescriptors = true;
                      }
                      break;
              case 5: // STREAMTYPE_13818_PRIVATE
              case 6: // STREAMTYPE_13818_PES_PRIVATE
              //XXX case 8: // STREAMTYPE_13818_DSMCC
                      {
                      int dpid = 0;
                      int dtype = 0;
                      char lang[MAXLANGCODE1] = { 0 };
                      SI::Descriptor *d;
                      for (SI::Loop::Iterator it; (d = stream.streamDescriptors.getNext(it)); ) {
                          switch (d->getDescriptorTag()) {
                            case SI::AC3DescriptorTag:
                            case SI::EnhancedAC3DescriptorTag:
                                 dpid = esPid;
                                 dtype = d->getDescriptorTag();
                                 ProcessCaDescriptors = true;
                                 break;
                            case SI::SubtitlingDescriptorTag:
                                 if (NumSpids < MAXSPIDS) {
                                    Spids[NumSpids] = esPid;
                                    SI::SubtitlingDescriptor *sd = (SI::SubtitlingDescriptor *)d;
                                    SI::SubtitlingDescriptor::Subtitling sub;
                                    char *s = SLangs[NumSpids];
                                    int n = 0;
                                    for (SI::Loop::Iterator it; sd->subtitlingLoop.getNext(sub, it); ) {
                                        if (sub.languageCode[0]) {
                                           SubtitlingTypes[NumSpids] = sub.getSubtitlingType();
                                           CompositionPageIds[NumSpids] = sub.getCompositionPageId();
                                           AncillaryPageIds[NumSpids] = sub.getAncillaryPageId();
                                           if (n > 0)
                                              *s++ = '+';
                                           strn0cpy(s, I18nNormalizeLanguageCode(sub.languageCode), MAXLANGCODE1);
                                           s += strlen(s);
                                           if (n++ > 1)
                                              break;
                                           }
                                        }
                                    NumSpids++;
                                    }
                                 break;
                            case SI::TeletextDescriptorTag:
                                 Tpid = esPid;
                                 break;
                            case SI::ISO639LanguageDescriptorTag: {
                                 SI::ISO639LanguageDescriptor *ld = (SI::ISO639LanguageDescriptor *)d;
                                 strn0cpy(lang, I18nNormalizeLanguageCode(ld->languageCode), MAXLANGCODE1);
                                 }
                                 break;
                            default: ;
                            }
                          delete d;
                          }
                      if (dpid) {
                         if (NumDpids < MAXDPIDS) {
                            Dpids[NumDpids] = dpid;
                            Dtypes[NumDpids] = dtype;
                            strn0cpy(DLangs[NumDpids], lang, MAXLANGCODE1);
                            NumDpids++;
                            }
                         }
                      }
                      break;
              case 0x81: // STREAMTYPE_USER_PRIVATE
                      if (Channel->IsAtsc()) { // ATSC AC-3
                         char lang[MAXLANGCODE1] = { 0 };
                         SI::Descriptor *d;
                         for (SI::Loop::Iterator it; (d = stream.streamDescriptors.getNext(it)); ) {
                             switch (d->getDescriptorTag()) {
                               case SI::ISO639LanguageDescriptorTag: {
                                    SI::ISO639LanguageDescriptor *ld = (SI::ISO639LanguageDescriptor *)d;
                                    strn0cpy(lang, I18nNormalizeLanguageCode(ld->languageCode), MAXLANGCODE1);
                                    }
                                    break;
                               default: ;
                               }
                             delete d;
                             }
                         if (NumDpids < MAXDPIDS) {
                            Dpids[NumDpids] = esPid;
                            Dtypes[NumDpids] = SI::AC3DescriptorTag;
                            strn0cpy(DLangs[NumDpids], lang, MAXLANGCODE1);
                            NumDpids++;
                            }
                         ProcessCaDescriptors = true;
                         }
                      break;
              default: ;//printf("PID: %5d %5d %2d %3d %3d\n", pmt.getServiceId(), stream.getPid(), stream.getStreamType(), pmt.getVersionNumber(), Channel->Number());
              }
            if (ProcessCaDescriptors) {
               for (SI::Loop::Iterator it; (d = (SI::CaDescriptor*)stream.streamDescriptors.getNext(it, SI::CaDescriptorTag)); ) {
                   CaDescriptors->AddCaDescriptor(d, esPid);
                   delete d;
                   }
               }
            }
        //LO removed the check on Setup.UpdateChannels    
        Channel->SetPids(Vpid, Ppid, Vtype, Apids, Atypes, ALangs, Dpids, Dtypes, DLangs, Spids, SLangs, Tpid);
        Channel->SetCaIds(CaDescriptors->CaIds());
        Channel->SetSubtitlingDescriptors(SubtitlingTypes, CompositionPageIds, AncillaryPageIds);
        Channel->SetCaDescriptors(CaDescriptorHandler.AddCaDescriptors(CaDescriptors));
        FoundPidsForService(Channel->Sid());  //LO added
        }
     /**************************************************
      End of part copied from pat.c
      **************************************************/

  return true;
}

bool cChannelScanner::ParseSdt(const unsigned char *Data)
{	
  SI::SDT sdt(Data,false);
  if (!sdt.CheckCRCAndParse())
    return false;
          
 /**************************************************
  Following part copied from sdt.c
  Modified lines marked with //LO
  **************************************************/
  SI::SDT::Service SiSdtService;
  for (SI::Loop::Iterator it; sdt.serviceLoop.getNext(SiSdtService, it); ) {
      if (SiSdtService.getRunningStatus()!=SI::RunningStatusRunning) { //LO added
        warning("Service %d, running status %d (!=running), skipped\n",SiSdtService.getServiceId(),SiSdtService.getRunningStatus()); //LO added
        continue; //LO added
      }  //LO added
      //LO in the following 3 lines changed Source() to transponderData->Source()
      LOCK_CHANNELS_WRITE;
      cChannel *channel = (cChannel *) Channels->GetByChannelID(tChannelID(transponderData->Source(), sdt.getOriginalNetworkId(), sdt.getTransportStreamId(), SiSdtService.getServiceId()));
      if (!channel)
          channel = Channels->GetByChannelID(tChannelID(transponderData->Source(), 0, transponderData->Transponder(), SiSdtService.getServiceId()));
      cLinkChannels *LinkChannels = NULL;
      SI::Descriptor *d;
      for (SI::Loop::Iterator it2; (d = SiSdtService.serviceDescriptors.getNext(it2)); ) {
          switch (d->getDescriptorTag()) {
            case SI::ServiceDescriptorTag: {
                 SI::ServiceDescriptor *sd = (SI::ServiceDescriptor *)d;
                    char NameBufDeb[1024];
                    char ShortNameBufDeb[1024];
                 sd->serviceName.getText(NameBufDeb, ShortNameBufDeb, sizeof(NameBufDeb), sizeof(ShortNameBufDeb));
                  //printf(" %s --  ServiceType: %X: AddServiceType %d, Sid %i, running %i\n",
                  //NameBufDeb, sd->getServiceType(), AddServiceType, SiSdtService.getServiceId(), SiSdtService.getRunningStatus());
                 
                 switch (sd->getServiceType()) {
                   case 0x01: // digital television service
                   case 0x02: // digital radio sound service
                   case 0x04: // NVOD reference service
                   case 0x05: // NVOD time-shifted service
                   case 0x16: // digital SD television service
                   case 0x19: // digital HD television service
                        {
                        char NameBuf[Utf8BufSize(1024)];
                        char ShortNameBuf[Utf8BufSize(1024)];
                        char ProviderNameBuf[Utf8BufSize(1024)];
                        sd->serviceName.getText(NameBuf, ShortNameBuf, sizeof(NameBuf), sizeof(ShortNameBuf));
                        char *pn = compactspace(NameBuf);
                        char *ps = compactspace(ShortNameBuf);
                        //LO removed check for cable channels, this is satellite only
                        // Avoid ',' in short name (would cause trouble in channels.conf):
                        for (char *p = ShortNameBuf; *p; p++) {
                            if (*p == ',')
                               *p = '.';
                            }
                        sd->providerName.getText(ProviderNameBuf, sizeof(ProviderNameBuf));
                        char *pp = compactspace(ProviderNameBuf);
                        NewServiceInSdt(SiSdtService.getServiceId()); //LO added
                        if (channel) {
                           totalFound++; //LO added
                           info("OLD"); //LO added
                           //FIXME channel->SetId(sdt.getOriginalNetworkId(), sdt.getTransportStreamId(), SiSdtService.getServiceId(), channel->Rid());
                           //LO removed check on Setup.UpdateChannels
                           channel->SetName(pn, ps, pp);
                           // Using SiSdtService.getFreeCaMode() is no good, because some
                           // tv stations set this flag even for non-encrypted channels :-(
                           // The special value 0xFFFF was supposed to mean "unknown encryption"
                           // and would have been overwritten with real CA values later:
                           // channel->SetCa(SiSdtService.getFreeCaMode() ? 0xFFFF : 0);
                           }
                        else if (*pn) {
                           totalFound++; //LO added
                           info("NEW"); //LO added
                           newFound++; //LO added
                           channel = Channels->NewChannel(transponderData, pn, ps, pp, sdt.getOriginalNetworkId(), sdt.getTransportStreamId(), SiSdtService.getServiceId());
                           //LO removed patFilter->Trigger()
                           }
                        info(" channel found, sid: %d name: \"%s\" shortname: \"%s\" provider: \"%s\"\n", SiSdtService.getServiceId(), pn, ps, pp); //LO added
                        }
                   default: ;
                   }
                 }
                 break;
            // Using the CaIdentifierDescriptor is no good, because some tv stations
            // just don't use it. The actual CA values are collected in pat.c:
            /*
            case SI::CaIdentifierDescriptorTag: {
                 SI::CaIdentifierDescriptor *cid = (SI::CaIdentifierDescriptor *)d;
                 if (channel) {
                    for (SI::Loop::Iterator it; cid->identifiers.hasNext(it); )
                        channel->SetCa(cid->identifiers.getNext(it));
                    }
                 }
                 break;
            */
            case SI::NVODReferenceDescriptorTag: {
                 SI::NVODReferenceDescriptor *nrd = (SI::NVODReferenceDescriptor *)d;
                 SI::NVODReferenceDescriptor::Service Service;
                 for (SI::Loop::Iterator it; nrd->serviceLoop.getNext(Service, it); ) {
                     totalFound++;
                     NewServiceInSdt(Service.getServiceId());
                     //LO in the following line changed Source() to transponderData->Source()
                     cChannel *link = Channels->GetByChannelID(tChannelID(transponderData->Source(), Service.getOriginalNetworkId(), Service.getTransportStream(), Service.getServiceId()));
                     info("%s NVOD service found, sid: %d\n", link ? "OLD" : "NEW", Service.getServiceId()); //LO added
                     if (!link) { //LO removed check on Setup.UpdateChannels
                        newFound++; //LO added
                        //LO in the following line changed Source() to transponderData->Source()
                        link = Channels->NewChannel(transponderData, "NVOD", "", "", Service.getOriginalNetworkId(), Service.getTransportStream(), Service.getServiceId());
                        }
                     if (link) {
                        if (!LinkChannels)
                           LinkChannels = new cLinkChannels;
                        LinkChannels->Add(new cLinkChannel(link));
                        }
                     }
                 }
                 break;
            default: ;
            }
          delete d;
          }
      if (LinkChannels) {
         if (channel)
            channel->SetLinkChannels(LinkChannels);
         else
            delete LinkChannels;
         }
      }
 /**************************************************
  end of part copied from sdt.c
  **************************************************/
      
  return true;
}

/**
 *   returns 0 when more sections are expected
 *	   1 when all sections are read on this pid
 *	   -1 on invalid table id
 */
int cChannelScanner::ParseSection(cSectionBuf *s)
{
  const unsigned char *buf = s->buf;
  int table_id;
  int section_length;
  int table_id_ext;
  int section_version_number;
  int section_number;
  int last_section_number;
  int i;

  table_id = buf[0];

  if (s->table_id != table_id)
    return -1;

  section_length = ((buf[1] & 0x0f) << 8) | buf[2];

  table_id_ext = (buf[3] << 8) | buf[4];
  section_version_number = (buf[5] >> 1) & 0x1f;
  section_number = buf[6];
  last_section_number = buf[7];

  if (s->section_version_number != section_version_number || s->table_id_ext != table_id_ext) {
    if (s->section_version_number != -1 && s->table_id_ext != -1)
      debug("section version_number or table_id_ext changed "
            "%d -> %d / %04x -> %04x\n",
            s->section_version_number, section_version_number,
            s->table_id_ext, table_id_ext);
    s->table_id_ext = table_id_ext;
    s->section_version_number = section_version_number;
    s->sectionfilter_done = false;
    memset (s->section_done, 0, sizeof(s->section_done));
  }

  buf += 8;  /* past generic table header */
  section_length -= 5 + 4;  /* header + crc */
  if (section_length < 0) {
    warning("truncated section (PID %d, lenght %d)",
            s->pid, section_length + 9);
    return 0;
  }

  if (!get_bit(s->section_done, section_number)) {
    bool section_processed = false;

    debug("pid %d tid %d table_id_ext %d, "
          "%i/%i (version %i)\n",
          s->pid, table_id, table_id_ext, section_number,
          last_section_number, section_version_number);

    switch (table_id) {
      case 0x00:
        verbose("PAT\n");
        section_processed=ParsePat (buf, section_length, table_id_ext);
        break;

      case 0x02:
        verbose("PMT %d for service %d sect.num %d last %d\n", s->pid, table_id_ext,section_number, last_section_number);
        section_processed=ParsePmt (s->buf);
        break;

      case 0x42:
      case 0x46:
        verbose("SDT (%s TS) sect.num %d last %d\n", table_id == 0x42 ? "actual":"other",section_number, last_section_number);
        section_processed=ParseSdt (s->buf);
        break;

      default:
        verbosedebug("skip table_id %d\n", table_id);
        section_processed=true;
        ;
    };

    if (section_processed)
      set_bit (s->section_done, section_number);

    for (i = 0; i <= last_section_number; i++)
      if (get_bit (s->section_done, i) == 0)
        break;

    if (i > last_section_number)
      s->sectionfilter_done = true;
  }

  if (s->sectionfilter_done)
    return 1;

  return 0;
}


int cChannelScanner::ReadSections(cSectionBuf *s)
{
  int section_length, count;

  if (s->sectionfilter_done)
    return 1;

  /* the section filter API guarantess that we get one full section
   * per read(), provided that the buffer is large enough (it is)
   */
  if (((count = read (s->fd, s->buf, sizeof(s->buf))) < 0) && errno == EOVERFLOW) {
    warning("EOVERFLOW reading section\n");
    count = read (s->fd, s->buf, sizeof(s->buf));}
    if (count < 0) {
      errorn("ReadSections: read error");
      return -1;
    }

  if (count < 4) {
    warning("Section length <4\n");
    return -1;
  }

  section_length = ((s->buf[1] & 0x0f) << 8) | s->buf[2];

  if (count != section_length + 3) {
    warning("Read %d bytes but section length should be %d\n",count,section_length);
    return -1;
  }

  if (ParseSection(s) == 1)
    return 1;

  return 0;
}

cSectionBuf* cChannelScanner::SetupFilter(int pid, int tid, int tid_ext, int timeout)
{
  cSectionBuf *s = new cSectionBuf;

  s->fd = -1;
  s->pid = pid;
  s->table_id = tid;
  s->retries = 0;

  s->timeout = timeout;

  s->table_id_ext = tid_ext;
  s->section_version_number = -1;

  return s;
}

void cChannelScanner::UpdatePollFds(void)
{
  cSectionBuf* s;
  int i;

  memset(poll_section_bufs, 0, sizeof(poll_section_bufs));
  for (i = 0; i < MAX_RUNNING_FILTERS; i++)
    poll_fds[i].fd = -1;
  i = 0;
  for (s = running_filters.First(); s; s = running_filters.Next(s)) {
    if (i >= MAX_RUNNING_FILTERS)
      fatal("too many poll_fds\n");
    if (s->fd == -1)
      fatal("s->fd == -1 on running_filters\n");
    verbosedebug("poll fd %d\n", s->fd);
    poll_fds[i].fd = s->fd;
    poll_fds[i].events = POLLIN;
    poll_fds[i].revents = 0;
    poll_section_bufs[i] = s;
    i++;
  }
}

bool cChannelScanner::StartFilter(cSectionBuf* s)
{
  struct dmx_sct_filter_params f;

  if (running_filters.Count() >= MAX_RUNNING_FILTERS)
    goto err0;
  if ((s->fd = open (dmx_devname, O_RDWR | O_NONBLOCK)) < 0)
    goto err0;

  verbosedebug("start filter pid %d table_id %d\n", s->pid, s->table_id);

  memset(&f, 0, sizeof(f));

  f.pid = (uint16_t) s->pid;

  if (s->table_id < 0x100 && s->table_id > 0) {
    f.filter.filter[0] = (uint8_t) s->table_id;
    f.filter.mask[0]   = 0xff;
  }
  if (s->table_id_ext < 0x10000 && s->table_id_ext > 0) {
    f.filter.filter[1] = (uint8_t) ((s->table_id_ext >> 8) & 0xff);
    f.filter.filter[2] = (uint8_t) (s->table_id_ext & 0xff);
    f.filter.mask[1] = 0xff;
    f.filter.mask[2] = 0xff;
  }

  f.timeout = 0;
  f.flags = DMX_IMMEDIATE_START | DMX_CHECK_CRC;

  if (ioctl(s->fd, DMX_SET_FILTER, &f) == -1) {
    errorn ("ioctl DMX_SET_FILTER failed");
    goto err1;
  }

  s->sectionfilter_done = false;
  time(&s->start_time);

  for (cSectionBuf *s2 = waiting_filters.First(); s2; s2 = waiting_filters.Next(s2))
    if (s==s2) {
      waiting_filters.Del(s2,false);
      break;
    }
  running_filters.Add(s);

  UpdatePollFds();

  return true;

err1:
  ioctl (s->fd, DMX_STOP);
  close (s->fd);
err0:
  return false;
}


void cChannelScanner::StopFilter(cSectionBuf *s)
{
  verbosedebug("stop filter pid %d\n", s->pid);
  ioctl (s->fd, DMX_STOP);
  close (s->fd);
  s->fd = -1;
  running_filters.Del(s,false);
  s->running_time += time(NULL) - s->start_time;
  UpdatePollFds();
}


void cChannelScanner::AddFilter(cSectionBuf *s)
{
  verbosedebug("add filter pid %d\n", s->pid);
  if (!StartFilter (s))
    waiting_filters.Add(s);
}


void cChannelScanner::RemoveFilter(cSectionBuf *s)
{
  verbosedebug("remove filter pid %d\n", s->pid);
  StopFilter (s);

  for (s = waiting_filters.First(); s; s = waiting_filters.Next(s)) {
    if (!StartFilter (s))
      break;
  };
}

void cChannelScanner::ReadFilters(void)
{
  cSectionBuf *s;
  int i, n, done;

  n = poll(poll_fds, running_filters.Count(), 100);
  if (n == -1)
    errorn("poll");

  for (i = 0; i < running_filters.Count(); i++) {
    s = poll_section_bufs[i];
    if (!s)
      fatal("poll_section_bufs[%d] is NULL\n", i);
    if (poll_fds[i].revents)
      done = ReadSections (s) == 1;
    else
      done = 0; /* timeout */
    if (done || time(NULL) > s->start_time + s->timeout) {
      if (done)
        verbosedebug("filter done pid %d\n", s->pid);
      else
        warning("filter timeout pid %d\n", s->pid);
    RemoveFilter (s);
    if (!done && s->retries<1) {
      s->retries++;
      s->timeout = s->timeout + 2;
      warning("trying again pid %d try %d\n", s->pid, s->retries+1);
      AddFilter(s);
    }
    else
      delete s;
    }
  }
}


void cChannelScanner::Cleanup(void)
{
  cSectionBuf* s;

  for (s = waiting_filters.First(); s; s = waiting_filters.Next(s))
    waiting_filters.Del(s);
  for (s = running_filters.First(); s; s = running_filters.Next(s)) {
    StopFilter(s);
    delete s;
  }
}

void cChannelScanner::ScanTransponder(void)
{
 /**
  *  filter timeouts > min repetition rates specified in ETR211
  */

 /**
  * Scan completely the SDT first, so that it will create channels
  */
  AddFilter (SetupFilter (0x11, 0x42, -1, 5)); /* SDT */
  do {
    ReadFilters ();
  } while (Running() && running_filters.Count()+waiting_filters.Count()>0);
		   
  if (!Running())
    return;	   
		   
 /**
  * then scan the pat to update the pids
  */
  AddFilter (SetupFilter (0x00, 0x00, -1, 5)); /* PAT */
  do {
    ReadFilters ();
  } while (Running() && running_filters.Count()+waiting_filters.Count()>0);
}


//---- utility class to get the name of the demux device
class cCrackDvbDevice:public cDvbDevice {
public:
  cString DemuxDevname() { return DvbName(DEV_DVB_DEMUX, adapter, frontend);}
};

cChannelScanner::cChannelScanner(cDevice *ADevice, cChannel *TransponderData)
{
  cCrackDvbDevice *crack=static_cast<cCrackDvbDevice *>(ADevice);
  dmx_devname=crack->DemuxDevname();
  transponderData=TransponderData;
  totalFound=0;
  newFound=0;
  info("Channel scanner created %s\n",(const char *)dmx_devname);
  info("Transponder data %d:%s:%s:%d\n",transponderData->Frequency(),transponderData->Parameters(),*cSource::ToString(transponderData->Source()),transponderData->Srate());
}


cChannelScanner::~cChannelScanner(void)
{
  Cancel(5); //give enoug time for Action to end  
}

void cChannelScanner::Action(void)
{
  for (int i = 0; i < MAX_RUNNING_FILTERS; i++)
    poll_fds[i].fd = -1;
  //Channels.Lock(true);
  ScanTransponder ();
  if (running_filters.Count()+waiting_filters.Count()==0)
    info("Scan ended\n");
  else {   
    info("Scan interrupted, cleaning up\n");
    Cleanup ();
  }   
  //Channels.Unlock();
  CheckFoundPids();
}

