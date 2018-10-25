/*
 * scan.c: actuator - A plugin for the Video Disk Recordes 
 *
 * adapted from statemachine.c of the wirbelscan plugin 
 * (License GPL, Written by  Winfried Koehler <w_scan AT gmx MINUS topmail DOT de>
 *  see  wirbel.htpc-forum.de/wirbelscan/index2.html)
 *
 */

#include <stdarg.h>
#include <stdlib.h>
#include <string>
#include <vdr/tools.h>
#include "scan.h"
#include "scanfilter.h"
#include "common.h"
//#include "menusetup.h"
#include "actuator.h"
#include "scan.h"
#include "si_ext.h"
using namespace SI_EXT;



dvbScanner::dvbScanner(cMainMenuActuator *parent, cDvbDevice *device)
{
    m_parent = parent;
    m_device = device;
}

dvbScanner::~dvbScanner()
{
}

void dvbScanner::ResetLists()
{
  NewChannels.Clear();
  NewTransponders.Clear();
  SdtData.services.Clear();
  NitData.frequency_list.Clear();
  NitData.cell_frequency_links.Clear();
  NitData.service_types.Clear();
  for(int i = 0; i < NitData.transport_streams.Count(); i++)
     delete NitData.transport_streams[i];
  NitData.transport_streams.Clear();

  NewChannels.Capacity(2500);
  NewTransponders.Capacity(500);
}

void dvbScanner::StartScan(cSource *source, cTransponders *Transponders)
{

    ResetLists();
    cTransponder *p = Transponders->First();
    NewTransponders.Clear();
    while (p) {
        TChannel * t = new TChannel;
        t->DelSys = p->System();
        t->Frequency = p->Frequency();
        t->Symbolrate = p->Srate();
        t->Modulation = p->Modulation();
        t->Polarization = p->Polarization();
        t->Source = cSource::ToString(source->Code());
        t->FEC = p->Fec();
        t->Rolloff = 999;
        NewTransponders.Add(t);
        p=Transponders->Next(p);
    }
    m_source = source;
    m_channels = 0;
    m_newchannels = 0;
    m_progress = 0;
    m_abort = false;
    Start();
}


void dvbScanner::StartScan(cSource *source, int delsys, int modulation, int freq, char pol, int sr, int fec)
{
    ResetLists();
    NewTransponders.Clear();
    TChannel * t = new TChannel;
    t->DelSys = delsys;
    t->Modulation = modulation;
    t->Frequency = freq;
    t->Polarization = pol;
    t->Symbolrate = sr;
    t->Source = cSource::ToString(source->Code());
    t->FEC = fec;
    t->Rolloff = 999;
    NewTransponders.Add(t);
    m_source = source;
    m_channels = 0;
    m_newchannels = 0;
    m_progress = 0;
    m_abort = false;
    Start();
}


void dvbScanner::StopScan()
{
    m_abort = true;
}


TSdtData SdtData;
TNitData NitData;

void dvbScanner::Action()
{
    cPatScanner* PatScanner = NULL;
    cNitScanner* NitScanner = NULL;
    cSdtScanner* SdtScanner = NULL;
    TList<cPmtScanner*> PmtScanners;
    struct TPatData PatData;
    TList<TPmtData*> PmtData;
    std::string s;
    
    int count = 0;
    for (int t=0 ; t<NewTransponders.Count() ; t++) {
        
                /* count entries to show progress*/
                int numentries = 0;
                count++;
                if (NewTransponders.Count()>0) 
                    m_progress = count * 100 / NewTransponders.Count();
                else
                    m_progress = 100;
                
                TChannel *Transponder=NewTransponders[t];
                
                if (!m_parent->Tune(Transponder->DelSys, Transponder->Modulation, Transponder->Frequency, Transponder->Polarization, Transponder->Symbolrate))
                    continue;
                dsyslog("Starting PatScanner");
                PatScanner = new cPatScanner(m_device, PatData);
                while (PatScanner->Active()) {
                    //dsyslog("PatScanner active");
                    if (m_abort) //FIXME free patscanner 
                        return;
                    cCondWait::SleepMs(100);
                }    
                dsyslog("PatScanner end");
                PatScanner = NULL;
                PmtScanners.Clear();
                PmtData.Clear();
                for(int i = 0; i < PatData.services.Count(); i++) {
                  TPmtData* d = new TPmtData;
                  d->program_map_PID = PatData.services[i].program_map_PID;
                  PmtData.Add(d);
                  cPmtScanner* p = new cPmtScanner(m_device, PmtData[i]);
                  PmtScanners.Add(p);
                 }
                // run up to 16 filters in parallel; up to 32 should be safe.
                int activePmts = 0;
                int finished = 0;
                while (finished < PmtScanners.Count()) {
                    if (m_abort)
                        return; //FIXME free free free
                    //dsyslog("Finished PMTs %d",finished);
                    cCondWait::SleepMs(100);
                    for(int i = 0; i < PmtScanners.Count(); i++) {
                        cPmtScanner* p = (cPmtScanner*) PmtScanners[i];
                        if (p->Finished()) {
                          finished++;
                          continue;
                        }
                        if (p->Active()) {
                          if (++activePmts > 16)
                          break;
                        }
                        else {
                          p->Start();
                        if (++activePmts > 16)
                          break;
                        }
                     }
                }
                dsyslog("Pmt Finished");

                PmtScanners.Clear();
                static int tm;
                tm = time(0);
                // some stupid cable providers use non-standard PID for NIT; sometimes called 'Setup-PID'.
                /*
                if (wSetup.DVBC_Network_PID != 0x10)
                   PatData.network_PID = wSetup.DVBC_Network_PID; */
                SdtData.original_network_id = 0;
                dsyslog("Starting NitScanner/SDTScanner");
                NitScanner = new cNitScanner(m_device, PatData.network_PID, NitData, /*FIXME dvbtype */ SCAN_SATELLITE);
                SdtScanner = new cSdtScanner(m_device, SdtData);
                while (NitScanner->Active() || SdtScanner->Active()) {
                    if (m_abort)
                        return; //FIXME free structures
                    //if (NitScanner->Active())
                    //    dsyslog("NitScanner still active");
                    //if (SdtScanner->Active())
                    //    dsyslog("SdtScanner still active");
                    cCondWait::SleepMs(100);

                }        

                dsyslog("NitScanner/SDTScanner end");
                m_device->SetOccupied(0);

                
           if (/*wSetup.verbosity > 4*/ true) {
              for(int i = 0; i < PmtData.Count(); i++)
                 dlog(0, "PMT %d: program_number = %d; Vpid = %d (%d); Tpid = %d Apid = %d Dpid = %d",
                         PmtData[i]->program_map_PID,
                         PmtData[i]->program_number,
                         PmtData[i]->Vpid.PID,PmtData[i]->PCR_PID,
                         PmtData[i]->Tpid,
                         PmtData[i]->Apids.Count()?PmtData[i]->Apids[0].PID:0,
                         PmtData[i]->Dpids.Count()?PmtData[i]->Dpids[0].PID:0);

              for(int i = 0; i < SdtData.services.Count(); i++) {
                 if (SdtData.services[i].reported)
                    continue;
                 SdtData.services[i].reported = true;
                 dlog(0, "SDT: ONID = %d, TID = %d, SID = %d, FreeCA = %d, Name = '%s'",
                      SdtData.services[i].original_network_id,
                      SdtData.services[i].transport_stream_id,
                      SdtData.services[i].service_id,
                      SdtData.services[i].free_CA_mode,
                      SdtData.services[i].Name.c_str());
                 }

              for(int i = 0; i < NitData.transport_streams.Count(); i++) {
                 if (NitData.transport_streams[i]->reported)
                    continue;
                 NitData.transport_streams[i]->reported = true;
                 NitData.transport_streams[i]->PrintTransponder(s);
                 dlog(0, "NIT: %s'%s', NID = %d, ONID = %d, TID = %d",
                    abs(NitData.transport_streams[i]->OrbitalPos - m_source->Position()) > 5?"WRONG SATELLITE: ":"",
                    s.c_str(), NitData.transport_streams[i]->NID, NitData.transport_streams[i]->ONID, NitData.transport_streams[i]->TID);
                 if (NitData.transport_streams[i]->Source == "T" and NitData.transport_streams[i]->DelSys == 1) {
                    for(int c = 0; c < NitData.transport_streams[i]->cells.Count(); c++) {
                       for(int cf = 0; cf < NitData.transport_streams[i]->cells[c].num_center_frequencies; cf++)
                          dlog(0, "   center%d = %u (cell_id %u)", c+1, NitData.transport_streams[i]->cells[c].center_frequencies[cf], NitData.transport_streams[i]->cells[c].cell_id);
                       for(int tf = 0; tf < NitData.transport_streams[i]->cells[c].num_transposers; tf++)
                          dlog(0, "      transposer%d = %u (cell_id_extension %u)", tf+1, NitData.transport_streams[i]->cells[c].transposers[tf].transposer_frequency, NitData.transport_streams[i]->cells[c].transposers[tf].cell_id_extension);
                       }
                    }
                 }
              }

           // PAT: transport_stream_id, network_pid (0x10), program_map_PIDs
           // PMT: program_number(service_id), PCR_PID, [stream_type, elementary_PID]
           // NIT: network_id, [transport_stream_id, original_network_id]
           // SDT: transport_stream_id, original_network_id, [service_id, free_CA_mode]
           
           Transponder->TID = PatData.services[0].transport_stream_id;
           if (SdtData.original_network_id) // update onid, if sdt found. 
              Transponder->ONID = SdtData.original_network_id;

           for(int i = 0; i < NitData.transport_streams.Count(); i++) {
              if ((NitData.transport_streams[i]->NID == Transponder->NID or
                  NitData.transport_streams[i]->ONID == Transponder->ONID) and
                  NitData.transport_streams[i]->TID == Transponder->TID) {
                 uint32_t f = Transponder->Frequency;
                 uint32_t center_freq = NitData.transport_streams[i]->Frequency;

                 Transponder->CopyTransponderData(NitData.transport_streams[i]);

                 if ((center_freq < 100000000) or (center_freq > 858000000) or (abs((int)center_freq - (int)f) > 2000000))
                    Transponder->Frequency = f;
                 break;
                 }
              }
             
           /* THIS WILL CAUSE AN ENDLESS LOOP  
           if (!known_transponder(Transponder, false, &NewTransponders)) {
              TChannel* tp = new TChannel;
              tp->CopyTransponderData(Transponder);
              tp->NID = Transponder->NID;
              tp->ONID = Transponder->ONID;
              tp->TID = Transponder->TID;
              tp->Tested = true;
              tp->Tunable = true;
              tp->PrintTransponder(s);
              dlog(5, "NewTransponders.Add: '%s', NID = %d, ONID = %d, TID = %d",
                   s.c_str(), tp->NID, tp->ONID, tp->TID);
              NewTransponders.Add(tp);
              } */

           for(int i = 0; i < PmtData.Count(); i++) {
              TChannel* n = new TChannel;
              n->CopyTransponderData(Transponder);
              n->NID = Transponder->NID;
              n->ONID = Transponder->ONID;
              n->TID = Transponder->TID;
              n->SID = PmtData[i]->program_number;
              n->VPID.PID  = PmtData[i]->Vpid.PID;
              n->VPID.Type = PmtData[i]->Vpid.Type;
              n->VPID.Lang = PmtData[i]->Vpid.Lang;
              n->PCR   = PmtData[i]->PCR_PID;
              n->TPID  = PmtData[i]->Tpid;
              n->APIDs = PmtData[i]->Apids;
              n->DPIDs = PmtData[i]->Dpids;
              n->SPIDs = PmtData[i]->Spids;
              n->CAIDs = PmtData[i]->Caids;

              if (!n->VPID.PID and !n->APIDs.Count() and !n->DPIDs.Count()) {
                 delete n;
                 continue;
                 }

              for(int j = 0; j < SdtData.services.Count(); j++) {
                 if (n->TID == SdtData.services[j].transport_stream_id and
                     n->SID == SdtData.services[j].service_id) {
                    n->Name         = SdtData.services[j].Name;
                    n->Shortname    = SdtData.services[j].Shortname;
                    n->Provider     = SdtData.services[j].Provider;
                    n->free_CA_mode = SdtData.services[j].free_CA_mode;
                    n->service_type = SdtData.services[j].service_type;
                    n->ONID         = SdtData.services[j].original_network_id;
                    break;
                    }
                 }

              if (n->service_type == Teletext_service or
                  n->service_type == DVB_SRM_service or
                  n->service_type == mosaic_service or
                  n->service_type == data_broadcast_service or
                  n->service_type == reserved_Common_Interface_Usage_EN50221 or
                  n->service_type == RCS_Map_EN301790 or
                  n->service_type == RCS_FLS_EN301790 or
                  n->service_type == DVB_MHP_service or
                  n->service_type == H264_AVC_codec_mosaic_service) {
                 dlog(5, "skip service %d '%s' (no Audio/Video)", n->SID, n->Name.c_str());
                 continue;
                 }

              #define PMT_ALL (SCAN_TV | SCAN_RADIO | SCAN_SCRAMBLED | SCAN_FTA)

              if (/*(wSetup.scanflags & PMT_ALL) != PMT_ALL and n->service_type < 0xFFFF*/false) {
                 if (/*(wSetup.scanflags & SCAN_SCRAMBLED) != SCAN_SCRAMBLED and n->free_CA_mode*/false) {
                    dlog(5, "skip service %d '%s' (encrypted)", n->SID, n->Name.c_str());
                    continue;
                    }
                 if (/*(wSetup.scanflags & SCAN_FTA) != SCAN_FTA and !n->free_CA_mode*/ false) {
                    dlog(5, "skip service %d '%s' (FTA)", n->SID, n->Name.c_str());
                    continue;
                    }

                 if (/*(wSetup.scanflags & SCAN_TV) != SCAN_TV*/ false) {
                    if (n->service_type == digital_television_service or
                        n->service_type == digital_television_NVOD_reference_service or
                        n->service_type == digital_television_NVOD_timeshifted_service or
                        n->service_type == MPEG2_HD_digital_television_service or
                        n->service_type == H264_AVC_SD_digital_television_service or
                        n->service_type == H264_AVC_SD_NVOD_timeshifted_service or
                        n->service_type == H264_AVC_SD_NVOD_reference_service or
                        n->service_type == H264_AVC_HD_digital_television_service or
                        n->service_type == H264_AVC_HD_NVOD_timeshifted_service or
                        n->service_type == H264_AVC_HD_NVOD_reference_service or
                        n->service_type == H264_AVC_frame_compat_plano_stereoscopic_HD_digital_television_service or
                        n->service_type == H264_AVC_frame_compat_plano_stereoscopic_HD_NVOD_timeshifted_service or
                        n->service_type == H264_AVC_frame_compat_plano_stereoscopic_HD_NVOD_reference_service or
                        n->service_type == HEVC_digital_television_service) {
                       dlog(5, "skip service %d '%s' (tv)", n->SID, n->Name.c_str());
                       continue;
                       }
                    }
                 if (/*(wSetup.scanflags & SCAN_RADIO) != SCAN_RADIO*/ false) {
                    if (n->service_type == digital_radio_sound_service or
                        n->service_type == FM_radio_service or
                        n->service_type == advanced_codec_digital_radio_sound_service) {
                       dlog(5, "skip service %d '%s' (radio)", n->SID, n->Name.c_str());
                       continue;
                       }
                    }
                 }
              if (/*wSetup.verbosity > 4*/ true) {
                 n->Print(s);
                 dlog(4, "NewChannels.Add: '%s'", s.c_str());
                 }
              else {
                 if (n->Name != "???") dlog(0, "%s", n->Name.c_str());
                 }
              NewChannels.Add(n);
              /* FIXME
              if (MenuScanning)
                 MenuScanning->SetChan(NewChannels.Count()); 
              */
              }

           for(int i = 0; i < NewChannels.Count(); i++) {
              if (NewChannels[i]->Name != "???")
                 continue;
              for(int j = 0; j < SdtData.services.Count(); j++) {
                 if (NewChannels[i]->TID == SdtData.services[j].transport_stream_id and
                   /*NewChannels[i]->NID == SdtData.services[j].original_network_id and*/
                     NewChannels[i]->SID == SdtData.services[j].service_id) {
                    NewChannels[i]->Name         = SdtData.services[j].Name;
                    NewChannels[i]->Shortname    = SdtData.services[j].Shortname;
                    NewChannels[i]->Provider     = SdtData.services[j].Provider;
                    NewChannels[i]->free_CA_mode = SdtData.services[j].free_CA_mode;
                    NewChannels[i]->Print(s);
                    dlog(5, "Update: '%s'", s.c_str());
                    break;
                    }
                 }
              }

           for(int i = 0; i < NitData.transport_streams.Count(); i++) {
              if (abs(NitData.transport_streams[i]->OrbitalPos - m_source->Position()) > 5)
                 continue;
              //dlog(4,"===================before checking is_known");
              //   for (int ii=0; ii<NewTransponders.Count(); ii++){
              //     NewTransponders[ii]->PrintTransponder(s);
              //     dlog(4,"  %d  --[%d]-------> %s",ii,NewTransponders[ii],s.c_str());
              //     }
              if (!known_transponder(NitData.transport_streams[i], true, &NewTransponders)) {
                 TChannel* tp = new TChannel;
                 tp->CopyTransponderData(NitData.transport_streams[i]);
                 tp->NID = NitData.transport_streams[i]->NID;
                 tp->ONID = NitData.transport_streams[i]->ONID;
                 tp->TID = NitData.transport_streams[i]->TID;
                 tp->PrintTransponder(s);
                 dlog(4, "NewTransponders.Add: '%s', NID = %d, ONID = %d, TID = %d",
                      s.c_str(), tp->NID, tp->ONID, tp->TID);
              //dlog(4,"===================before add");
              //   for (int ii=0; ii<NewTransponders.Count(); ii++){
              //     NewTransponders[ii]->PrintTransponder(s);
              //     dlog(4,"  %d  --[%d]-------> %s",ii,NewTransponders[ii],s.c_str());
              //     }
                 NewTransponders.Add(tp);
              //dlog(4,"===================after add");
              //   for (int ii=0; ii<NewTransponders.Count(); ii++){
              //     NewTransponders[ii]->PrintTransponder(s);
              //     dlog(4,"  %d  --[%d]-------> %s",ii,NewTransponders[ii],s.c_str());
              //     }
                 }
                 
              if (NitData.transport_streams[i]->Source == "T" and NitData.transport_streams[i]->DelSys == 1) {
                 for(int c = 0; c < NitData.transport_streams[i]->cells.Count(); c++) {
                    for(int cf = 0; cf < NitData.transport_streams[i]->cells[c].num_center_frequencies; cf++) {
                       TChannel* tp = new TChannel;
                       tp->CopyTransponderData(NitData.transport_streams[i]);
                       tp->NID = NitData.transport_streams[i]->NID;
                       tp->TID = NitData.transport_streams[i]->TID;
                       tp->Frequency = NitData.transport_streams[i]->cells[c].center_frequencies[cf];
                       if (!known_transponder(tp, true, &NewTransponders)) {
                          tp->PrintTransponder(s);
                          dlog(4, "NewTransponders.Add: '%s', NID = %d, ONID = %d, TID = %d", s.c_str(), tp->NID, tp->ONID, tp->TID);
                          NewTransponders.Add(tp);
                          }
                       else
                          delete tp;
                       }
                    for(int tf = 0; tf < NitData.transport_streams[i]->cells[c].num_transposers; tf++) {
                       TChannel* tp = new TChannel;
                       tp->CopyTransponderData(NitData.transport_streams[i]);
                       tp->NID = NitData.transport_streams[i]->NID;
                       tp->TID = NitData.transport_streams[i]->TID;
                       tp->Frequency = NitData.transport_streams[i]->cells[c].transposers[tf].transposer_frequency;
                       if (!known_transponder(tp, true, &NewTransponders)) {
                          tp->PrintTransponder(s);
                          dlog(5, "NewTransponders.Add: '%s', NID = %d, ONID = %d, TID = %d", s.c_str(), tp->NID, tp->ONID, tp->TID);
                          NewTransponders.Add(tp);
                          }
                       else
                          delete tp;
                       }
                    }
                 }
              }

           for(int i = 0; i < NitData.cell_frequency_links.Count(); i++) {
              TChannel t;

              if (/*wSetup.verbosity > 5*/ true)
                 dlog(0, "NIT: cell_id %u, frequency %7.3fMHz network_id %d",
                       NitData.cell_frequency_links[i].cell_id,
                       NitData.cell_frequency_links[i].frequency/1e6,
                       NitData.cell_frequency_links[i].network_id);
              t.Source       = 'T';
              t.Frequency    = NitData.cell_frequency_links[i].frequency;
              t.Bandwidth    = t.Frequency <= 226500000 ? 7 : 8;
              t.Inversion    = 999;
              t.FEC          = 999;
              t.FEC_low      = 999;
              t.Modulation   = 999;
              t.Transmission = 999;
              t.Guard        = 999;
              t.Hierarchy    = 999;
              t.DelSys       = 0;

              if (!known_transponder(&t, true, &NewTransponders)) {
                 TChannel* n = new TChannel;
                 n->CopyTransponderData(&t);
                 n->PrintTransponder(s);
                 dlog(4, "NewTransponders.Add: '%s', NID = %d, TID = %d", s.c_str(), n->NID, n->TID);
                 NewTransponders.Add(n);
                 }

              t.DelSys = 1;
              if (!known_transponder(&t, true, &NewTransponders)) {
                 TChannel* n = new TChannel;
                 n->CopyTransponderData(&t);
                 n->PrintTransponder(s);
                 dlog(4, "NewTransponders.Add: '%s', NID = %d, TID = %d", s.c_str(), n->NID, n->TID);
                 NewTransponders.Add(n);
                 }

              for(int j = 0; j < NitData.cell_frequency_links[i].subcellcount; j++) {
                 dlog(5, "NIT:    cell_id_extension %u, frequency %7.3fMHz",
                      NitData.cell_frequency_links[i].subcells[j].cell_id_extension,
                      NitData.cell_frequency_links[i].subcells[j].transposer_frequency/1e6);
                 t.Frequency = NitData.cell_frequency_links[i].subcells[j].transposer_frequency;
                 t.Bandwidth = t.Frequency <= 226500000 ? 7 : 8;
                 t.DelSys    = 0;
                 
                 if (!known_transponder(&t, true, &NewTransponders)) {
                    TChannel* tp = new TChannel;
                    tp->CopyTransponderData(&t);
                    tp->PrintTransponder(s);
                    dlog(5, "NewTransponders.Add: '%s', NID = %d, TID = %d", s.c_str(), tp->NID, tp->TID);
                    NewTransponders.Add(tp);
                    }
                 
                 t.DelSys = 1;
                 if (!known_transponder(&t, true, &NewTransponders)) {
                    TChannel* tp = new TChannel;
                    tp->CopyTransponderData(&t);
                    tp->PrintTransponder(s);
                    dlog(4, "NewTransponders.Add: '%s', NID = %d, TID = %d", s.c_str(), tp->NID, tp->TID);
                    NewTransponders.Add(tp);
                    }
                 }
              }
              
              AddChannels();
              NewChannels.Clear();


//           if ((count = AddChannels()))
//              dlog(4, "added %d channels", count);
//
//           if (MenuScanning)
//              MenuScanning->SetChan(count); 


           // delete data from current tp
           PatData.network_PID = 0x10;
           PatData.services.Clear();
           for(int i = 0; i < PmtData.Count(); i++)
              delete PmtData[i];
           PmtData.Clear();
           NitData.frequency_list.Clear();
           NitData.cell_frequency_links.Clear();
	}
                
}


/* Here i cannot avoid anymore dealing with vdrs lists and
 * channel classes. So the real complicated stuff is here..
 */
#include <vdr/channels.h>

void dvbScanner::AddChannels(void) {
  cStateKey WriteState;
  cChannels* WChannels = (cChannels*) cChannels::GetChannelsWrite(WriteState, 30000);

  if (!WChannels)
     return;

  m_channels += NewChannels.Count();
  
  for(int i = 0; i < WChannels->Count(); i++) {
      
     
     const cChannel* ch = WChannels->Get(i);
     TChannel* newCh = NULL;
     int source = 0;

     // is 'ch' known in NewChannels?
     for(int idx = 0; idx < NewChannels.Count(); idx++) {
        if (!source)
            source = cSource::FromString(NewChannels[idx]->Source.c_str());

        if (ch->Nid() != NewChannels[idx]->ONID or
            ch->Tid() != NewChannels[idx]->TID or
            ch->Sid() != NewChannels[idx]->SID or
            ch->Source() != cSource::FromString(NewChannels[idx]->Source.c_str()))
           continue;

        // this channel is already known.
        newCh = NewChannels[idx];
        break;
        }

     //dlog(5, "%s channel '%s'", newCh?"known":"unknown", *ch->ToText());

     // existing channel not found by IDs
     /* FIXME 
     if (wSetup.scan_remove_invalid and !newCh and ch->Source() == source) {
        dlog(4, "remove invalid channel '%s'", *ch->ToText());
        WChannels->Del((cChannel*) ch);
        i--;
        continue;
        }
      */
     // update existing
     if (/*wSetup.scan_update_existing and */newCh) {
        std::string s;
        newCh->Print(s);
        if (s != *ch->ToText()) {
           ((cChannel*) ch)->Parse(s.c_str());
           dlog(4, "updated channel '%s'", *ch->ToText());
           }
        }
     }

  if (/*wSetup.scan_append_new*/true)
  for(int i = 0; (i < NewChannels.Count()); i++) {
     TChannel* n = NewChannels[i];
     const cChannel* old = NULL;

     for(old = WChannels->First(); old; old = WChannels->Next(old)) {
        if (old->Nid() == n->ONID and
            old->Tid() == n->TID and
            old->Sid() == n->SID and
            *cSource::ToString(old->Source()) == n->Source) {
           break;
           }
        }

     if (!old) {
        std::string s;
        cChannel* c = new cChannel;
        n->Print(s);
        c->Parse(s.c_str());
        dlog(4, "Add channel '%s'", s.c_str());
        WChannels->Add(c);
        m_newchannels++;
        }           
     }
  WChannels->ReNumber();
  WriteState.Remove();
}
