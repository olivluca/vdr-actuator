#include "filter.h"
#include <malloc.h>
#include "../../../libsi/section.h"
#include "../../../libsi/descriptor.h"
#include <vdr/channels.h>

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
  cCaDescriptor *ca1 = caDescriptors.First();
  cCaDescriptor *ca2 = arg.caDescriptors.First();
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

// --- PatFilter ------------------------------------------------------------

PatFilter::PatFilter()
{
  sdtFilter=NULL;
  pmtIndex = 0;
  for (int i=0; i<MAXFILTERS; i++)
    pmtPid[i] = 0;
  numPmtEntries = 0;
  Set(0x00, 0x00);  // PAT
  endofScan=false;
  SetStatus(false);
  pit=pnum=0;
  sdtfinished=false;
}

void PatFilter::SetSdtFilter(SdtFilter *SdtFilter)
{
  sdtFilter=SdtFilter;
  SetStatus(true);
}

void PatFilter::SetStatus(bool On)
{
  cFilter::SetStatus(On);
  pmtIndex = 0;
  for (int i=0; i<MAXFILTERS; i++)
    pmtPid[i] = 0;
  numPmtEntries = 0;
  num=0;
  pit=pnum=0;
}

void PatFilter::Trigger(void)
{
  numPmtEntries = 0;
}

bool PatFilter::PmtVersionChanged(int PmtPid, int Sid, int Version)
{
  uint64_t v = Version;
  v <<= 32;
  uint64_t id = (PmtPid | (Sid << 16)) & 0x00000000FFFFFFFFLL;
  for (int i = 0; i < numPmtEntries; i++) {
      if ((pmtVersion[i] & 0x00000000FFFFFFFFLL) == id) {
         bool Changed = (pmtVersion[i] & 0x000000FF00000000LL) != v;
         if (Changed)
            pmtVersion[i] = id | v;
         return Changed;
         }
      }
  if (numPmtEntries < MAXPMTENTRIES)
     pmtVersion[numPmtEntries++] = id | v;
  return true;
}

bool PatFilter::SidinSdt(int Sid)
{
  for (int i=0; i<sdtFilter->numSid; i++)
    if (sdtFilter->sid[i]==Sid)
    {
      for (int j=0; j<MAXFILTERS; j++)
        if (Sids[j]==Sid)
          return false;
      return true;
    }
  return false;
}

void PatFilter::Process(u_short Pid, u_char Tid, const u_char *Data, int Length)
{
  if (Pid == 0x00) {
     if (Tid == 0x00) {
        for (int i=0; i<MAXFILTERS; i++)
          if (pmtPid[i] && time(NULL) - lastPmtScan[i] > FILTERTIMEOUT) {
             Del(pmtPid[i], 0x02);
             pmtPid[i] = 0;
             for (int k=0; i<sdtFilter->numSid; k++)
               if (sdtFilter->sid[k]==Sids[i])
               {
                 num++;
                 sdtFilter->sid[k]=0;
                 break;
               }
             Sids[i]=0;
             pmtIndex++;
            // lastPmtScan = time(NULL);
             }
           SI::PAT pat(Data, false);
           if (!pat.CheckCRCAndParse())
              return;
           SI::PAT::Association assoc;
           int Index = 0;
           while (pmtPid[Index] && Index<MAXFILTERS) Index++;
           int tnum=0,tSid[100];
           for (SI::Loop::Iterator it; pat.associationLoop.getNext(assoc, it); ) {
               tSid[tnum++]=assoc.getServiceId();
               if (!assoc.isNITPid() &&  SidinSdt(assoc.getServiceId())) {
                  if (Index<MAXFILTERS) 
                  {
                    pmtPid[Index] = assoc.getPid();
                    Sids[Index] = assoc.getServiceId();
                    lastPmtScan[Index] = time(NULL);
                    Add(pmtPid[Index], 0x02);
                    while (pmtPid[Index] && Index<MAXFILTERS) Index++;
                  }
                  if (Index==MAXFILTERS)
                    break;
                  }
               }
             for (int i=0; i<tnum; i++)
             {
               bool found=false;
               for (int k=0; k<pnum; k++)
               {
                 if (tSid[i]==pSid[k])
                   found=true;
               }
               if (!found)
                  pSid[pnum++]=tSid[i];
             }
             pit++;              
             for (int i=0; pit>4 && i<sdtFilter->numSid; i++)
             {
               bool found=false;
               for (int j=0; j<pnum; j++)
                 if (!sdtFilter->sid[i] || sdtFilter->sid[i]==pSid[j])
                   found=true;
               if (!found)
               {
                 sdtFilter->sid[i]=0;
                 num++;
               }
             }
        }
     }
  else if (Tid == SI::TableIdPMT && Source() && Transponder()) {
     int Index=0;
     for (int i=0; i<MAXFILTERS; i++)
       if (Pid==pmtPid[i])
          Index=i;
     SI::PMT pmt(Data, false);
     if (!pmt.CheckCRCAndParse())
        return;
     if (!Channels.Lock(true, 10)) {
        numPmtEntries = 0; // to make sure we try again
        return;
        }
     cChannel *Channel = Channels.GetByServiceID(Source(), Transponder(), pmt.getServiceId());
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
        int Ppid = 0;
        int Vtype = 0;
        int Apids[MAXAPIDS + 1] = { 0 }; // these lists are zero-terminated
        int Atypes[MAXDPIDS + 1] = { 0 };
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
              case 0x0F: // ISO/IEC 13818-7 Audio with ADTS transport sytax
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
        Channel->SetPids(Vpid, Ppid, Vtype, Apids, Atypes, ALangs, Dpids, Dtypes, DLangs, Spids, SLangs, Tpid);
        Channel->SetCaIds(CaDescriptors->CaIds());
        Channel->SetSubtitlingDescriptors(SubtitlingTypes, CompositionPageIds, AncillaryPageIds);
        Channel->SetCaDescriptors(CaDescriptorHandler.AddCaDescriptors(CaDescriptors));
        }
     lastPmtScan[Index] = 0; // this triggers the next scan
     Channels.Unlock();
     }
  if (sdtfinished && num>=sdtFilter->numSid) {
    endofScan=true;
    }
}


// --- cSdtFilter ------------------------------------------------------------

int SdtFilter::channelsFound = 0;
int SdtFilter::newFound = 0;

SdtFilter::SdtFilter(PatFilter *PatFilter)
{
  patFilter = PatFilter;
  numSid=0;
  Set(0x11, 0x42);  // SDT
}

void SdtFilter::SetStatus(bool On)
{
  cFilter::SetStatus(On);
  sectionSyncer.Reset();
}

void SdtFilter::Process(u_short Pid, u_char Tid, const u_char *Data, int Length)
{
  if (!(Source() && Transponder()))
     return;
  SI::SDT sdt(Data, false);
  if (!sdt.CheckCRCAndParse())
     return;
  if (!sectionSyncer.Sync(sdt.getVersionNumber(), sdt.getSectionNumber(), sdt.getLastSectionNumber()))
     return;
  if (!Channels.Lock(true, 10))
     return;
  SI::SDT::Service SiSdtService;
  for (SI::Loop::Iterator it; sdt.serviceLoop.getNext(SiSdtService, it); ) {
     cChannel *channel = Channels.GetByChannelID(tChannelID(Source(), sdt.getOriginalNetworkId(), sdt.getTransportStreamId(), SiSdtService.getServiceId()));
      if (!channel)
         channel = Channels.GetByChannelID(tChannelID(Source(), 0, Transponder(), SiSdtService.getServiceId()));
      cLinkChannels *LinkChannels = NULL;
      SI::Descriptor *d;
      for (SI::Loop::Iterator it2; (d = SiSdtService.serviceDescriptors.getNext(it2)); ) {
          switch (d->getDescriptorTag()) {
            case SI::ServiceDescriptorTag: {
                 SI::ServiceDescriptor *sd = (SI::ServiceDescriptor *)d;
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
                        if (!*ps && cSource::IsCable(Source())) {
                           // Some cable providers don't mark short channel names according to the
                           // standard, but rather go their own way and use "name>short name":
                           char *p = strchr(pn, '>'); // fix for UPC Wien
                           if (p && p > pn) {
                              *p++ = 0;
                              strcpy(ShortNameBuf, skipspace(p));
                              }
                           }
                        // Avoid ',' in short name (would cause trouble in channels.conf):
                        for (char *p = ShortNameBuf; *p; p++) {
                            if (*p == ',')
                               *p = '.';
                            }
                        sd->providerName.getText(ProviderNameBuf, sizeof(ProviderNameBuf));
                        char *pp = compactspace(ProviderNameBuf);
                        sid[numSid++]=SiSdtService.getServiceId();
                        if (channel) {
                           channelsFound++;
                           channel->SetId(sdt.getOriginalNetworkId(), sdt.getTransportStreamId(), SiSdtService.getServiceId());
                           channel->SetName(pn, ps, pp);
                           // Using SiSdtService.getFreeCaMode() is no good, because some
                           // tv stations set this flag even for non-encrypted channels :-(
                           // The special value 0xFFFF was supposed to mean "unknown encryption"
                           // and would have been overwritten with real CA values later:
                           // channel->SetCa(SiSdtService.getFreeCaMode() ? 0xFFFF : 0);
                           }
                        else if (*pn) {
                           channelsFound++;
                           newFound++;
                           channel = Channels.NewChannel(Channel(), pn, ps, pp, sdt.getOriginalNetworkId(), sdt.getTransportStreamId(), SiSdtService.getServiceId());
                           patFilter->Trigger();
                           }
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
                 channelsFound++;
                 SI::NVODReferenceDescriptor *nrd = (SI::NVODReferenceDescriptor *)d;
                 SI::NVODReferenceDescriptor::Service Service;
                 for (SI::Loop::Iterator it; nrd->serviceLoop.getNext(Service, it); ) {
                     cChannel *link = Channels.GetByChannelID(tChannelID(Source(), Service.getOriginalNetworkId(), Service.getTransportStream(), Service.getServiceId()));
                     sid[numSid++]=SiSdtService.getServiceId();
                     if (!link) {
                        newFound++;
                        link = Channels.NewChannel(Channel(), "NVOD", "", "", Service.getOriginalNetworkId(), Service.getTransportStream(), Service.getServiceId());
                        patFilter->Trigger();
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
  Channels.Unlock();
 if (sdt.getSectionNumber() == sdt.getLastSectionNumber()) { 
  patFilter->SdtFinished();
  SetStatus(false);
    }
}

void SdtFilter::ResetFound(void)
{
  channelsFound=0;
  newFound=0;
}

int SdtFilter::ChannelsFound(void)
{
  return channelsFound;
}

int SdtFilter::NewFound(void)
{
  return newFound;
}
