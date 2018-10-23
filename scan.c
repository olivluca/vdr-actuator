#include "actuator.h"
#include "scan.h"

int PolarizationCharToEnum(char p)
{
    switch(p) {
        case 'H':
            return POLARIZATION_H;
        case 'V':
            return POLARIZATION_V;
        case 'R':
            return POLARIZATION_R;
        case 'L':
            return POLARIZATION_L;
    }
    return POLARIZATION_OFF;
}

char PolarizationEnumToChar(int p)
{
    switch(p) {
        case POLARIZATION_H:
            return 'H';
        case POLARIZATION_V:
            return 'V';
        case POLARIZATION_R:
            return 'R';
        case POLARIZATION_L:
            return 'L';
    }
    return ' ';
}


dvbScanner::dvbScanner(cMainMenuActuator *parent, cDvbDevice *device)
{
    m_entries = NULL;
    m_parms = NULL;
    m_parent = parent;
    m_device = device;
}

dvbScanner::~dvbScanner()
{
}

void dvbScanner::StartScan(cSource *source, cTransponders *Transponders)
{
    dvb_file *entries=(dvb_file *)calloc(sizeof(dvb_file), 1);
    dvb_entry *entry=NULL;
    cTransponder *p = Transponders->First();
    while (p) {
        dvb_entry *newentry=(dvb_entry *)calloc(sizeof(dvb_entry), 1);
        dvb_store_entry_prop(newentry, DTV_DELIVERY_SYSTEM, p->System());
        dvb_store_entry_prop(newentry, DTV_FREQUENCY, p->Frequency()*1000);
        dvb_store_entry_prop(newentry, DTV_SYMBOL_RATE, p->Srate()*1000);
        dvb_store_entry_prop(newentry, DTV_MODULATION, p->Modulation());
        dvb_store_entry_prop(newentry, DTV_POLARIZATION, PolarizationCharToEnum(p->Polarization()));
        
        if (!entry) {
		entries->first_entry = newentry;
		entry = entries->first_entry;
	} else {
                entry->next = newentry;
                entry = entry->next;
        }
        p=Transponders->Next(p);
        
    }
    m_source = source;
    m_entries = entries;
    m_parms = NULL;
    m_channels = 0;
    m_newchannels = 0;
    m_progress = 0;
    Start();
}


void dvbScanner::StartScan(cSource *source, int delsys, int modulation, int freq, char pol, int sr)
{
    struct dvb_entry *entry = (dvb_entry *)calloc(sizeof(dvb_entry),1);
    dvb_file *entries=(dvb_file *)calloc(sizeof(dvb_file), 1);
    
    entries->first_entry = entry;    
    dvb_store_entry_prop(entry, DTV_DELIVERY_SYSTEM, delsys);
    dvb_store_entry_prop(entry, DTV_FREQUENCY, freq * 1000);
    dvb_store_entry_prop(entry, DTV_SYMBOL_RATE, sr * 1000);
    dvb_store_entry_prop(entry, DTV_MODULATION, modulation);
    dvb_store_entry_prop(entry, DTV_POLARIZATION, PolarizationCharToEnum(pol));
    m_source = source;
    m_entries = entries;
    m_parms = NULL;
    m_channels = 0;
    m_newchannels = 0;
    m_progress = 0;
    Start();
}


void dvbScanner::StopScan()
{
    if (m_parms)
        m_parms->abort=1;
}

void dvb_my_log(int level, const char *fmt, ...)
{
	//if(level > sizeof(loglevels) / sizeof(struct loglevel) - 2) // ignore LOG_COLOROFF as well
	//	level = LOG_INFO;
	va_list ap;
        char *s;
        
	va_start(ap, fmt);
        if (vasprintf(&s, fmt, ap)>0) {
            esyslog(s);
            free(s);
        }
	va_end(ap);
}

void dvbScanner::Action()
{
    struct dvb_file *dvb_file_new = NULL;
    struct dvb_device *dvb;
    cDvbDevice *dev=static_cast<cDvbDevice *>(m_device);
    uint32_t freq, pol, symbolrate, delsys, modulation;    
    int count = 0, shift;    
    
    dvb = dvb_dev_alloc();
    
    if (!dvb) {
      esyslog("******dvb_dev_alloc()");
      return;
    }
    //dvb_dev_set_log(dvb,1,dvb_my_log);
    dvb_dev_find(dvb, NULL, NULL);
    dvb_dev_list *dvb_dev = dvb_dev_seek_by_adapter(dvb, dev->Adapter(), dev->Frontend(), DVB_DEVICE_FRONTEND);
    if (!dvb_dev) {
      esyslog("******dvb_dev_seek()");
      dvb_dev_free(dvb);
      return;
    }
    if (!dvb_dev_open(dvb,dvb_dev->sysname,O_RDONLY)) {
      esyslog("******cannot open frontend");
      dvb_dev_free(dvb);
      return;
    }
    
    
    dvb_dev = dvb_dev_seek_by_adapter(dvb, dev->Adapter(), dev->Frontend(), DVB_DEVICE_DEMUX);
    if (!dvb_dev) {
		esyslog("Couldn't find demux device node\n");
		dvb_dev_free(dvb);
		return;
    }    
    struct dvb_open_descriptor *dmx_fd = dvb_dev_open(dvb, dvb_dev->sysname, O_RDWR);
    if (!dmx_fd) {
      esyslog("******** cannot open demux device");
      dvb_dev_free(dvb);
      return;
    }
    
    m_parms = dvb->fe_parms;    
    //ugly hack to get the fd of the demuxer, which is the first hidden field of dvb_open_descriptor
    int *fdfd = (int *)dmx_fd;
    
    m_parms->verbose=1;
    m_parms->abort=0;
    struct dvb_entry *entry;   
    for (entry = m_entries->first_entry; entry != NULL; entry = entry->next) {
        
                /* count entries to show progress*/
                int numentries = 0;
                for (dvb_entry * countentry = m_entries->first_entry; countentry != NULL; countentry = countentry->next)
                    numentries ++;
                count++;
                
                if (numentries>0) 
                    m_progress = count * 100 / numentries;
                else
                    m_progress = 100;
                struct dvb_v5_descriptors *dvb_scan_handler = NULL;
                uint32_t stream_id;
		/*
		 * If the channel file has duplicated frequencies, or some
		 * entries without any frequency at all, discard.
		 */
		if (dvb_retrieve_entry_prop(entry, DTV_FREQUENCY, &freq))
                    continue;
                
		shift = dvb_estimate_freq_shift(m_parms);
                
                if (dvb_retrieve_entry_prop(entry, DTV_POLARIZATION, &pol))
			pol = POLARIZATION_OFF;
                
                if (dvb_retrieve_entry_prop(entry, DTV_STREAM_ID, &stream_id))
			stream_id = NO_STREAM_ID_FILTER;
                
                if (!dvb_new_entry_is_needed(m_entries->first_entry, entry,
						  freq, shift, (dvb_sat_polarization)pol, stream_id))
                    continue;
                
                dvb_retrieve_entry_prop(entry, DTV_SYMBOL_RATE, &symbolrate);
                dvb_retrieve_entry_prop(entry, DTV_DELIVERY_SYSTEM, &delsys);
                dvb_retrieve_entry_prop(entry, DTV_MODULATION, &modulation);
                
                //dvb_log("Scanning frequency #%d %d", count, freq);
                
        
                m_parms->current_sys=(fe_delivery_system_t)delsys;
                dvb_fe_store_parm(m_parms, DTV_DELIVERY_SYSTEM, delsys);
                dvb_fe_store_parm(m_parms, DTV_MODULATION, modulation);
                dvb_fe_store_parm(m_parms, DTV_INVERSION, INVERSION_AUTO);
                dvb_fe_store_parm(m_parms, DTV_FREQUENCY, freq);
                dvb_fe_store_parm(m_parms, DTV_POLARIZATION, pol);
                dvb_fe_store_parm(m_parms, DTV_SYMBOL_RATE, symbolrate); 
                
                if (!m_parent->Tune(delsys,modulation,freq/1000,PolarizationEnumToChar(pol),symbolrate/1000))
                    continue;
                
                
                dvb_scan_handler = dvb_get_ts_tables(dvb->fe_parms, *fdfd,
                                                    SYS_DVBS,
                                                    0, //other_nit,
                                                    1); //timeout_multiply);
                
                m_device->SetOccupied(0);
                
		if (m_parms->abort) {
			dvb_scan_free_handler_table(dvb_scan_handler);
			break;
		}
		if (!dvb_scan_handler)
			continue;
			
		/*
		 * Store the service entry
		 */
		dvb_store_channel(&dvb_file_new, m_parms, dvb_scan_handler,
				  1 /*args->get_detected*/, 1 /* args->get_nit*/);
                
                if (dvb_file_new) {
                    AddChannels(dvb_file_new);
                    dvb_file_free(dvb_file_new);
                    dvb_file_new=NULL;
                }

		/*
		 * Add new transponders based on NIT table information
		 */
		dvb_add_scaned_transponders(m_parms, dvb_scan_handler,
					    m_entries->first_entry, entry);

		/*
		 * Free the scan handler associated with the transponder
		 */

		dvb_scan_free_handler_table(dvb_scan_handler);
	}
                
   dvb_file_free(m_entries);
   m_entries = NULL;
   m_parms = NULL;
   dvb_dev_close(dmx_fd);
   dvb_dev_free(dvb);
}

void dvbScanner::ChanFromEntry(cChannels* channels, cChannel *channel, dvb_entry *entry)
{
    uint32_t freq, polarization, symbolrate, delsys, modulation, inversion, fec, pilot, rolloff, streamid;
    std::string params;    
    char tmp[8];
    params=""; 
    
    dvb_retrieve_entry_prop(entry, DTV_FREQUENCY, &freq);
    dvb_retrieve_entry_prop(entry, DTV_SYMBOL_RATE, &symbolrate);
    dvb_retrieve_entry_prop(entry, DTV_DELIVERY_SYSTEM, &delsys);
    dvb_retrieve_entry_prop(entry, DTV_INVERSION, &inversion);
    dvb_retrieve_entry_prop(entry, DTV_MODULATION, &modulation);
    dvb_retrieve_entry_prop(entry, DTV_POLARIZATION, &polarization);
    dvb_retrieve_entry_prop(entry, DTV_INNER_FEC, &fec);
    if (delsys == SYS_DVBS2) {
      dvb_retrieve_entry_prop(entry, DTV_PILOT, &pilot);
      dvb_retrieve_entry_prop(entry, DTV_ROLLOFF, &rolloff);
      dvb_retrieve_entry_prop(entry, DTV_STREAM_ID, &streamid);
    }  
    
    
    params+=PolarizationEnumToChar(polarization);
    switch(fec) {
        case FEC_NONE: params+="C0"; break;
        case FEC_1_2: params+= "C12"; break;
        case FEC_2_3: params+= "C23"; break;
        case FEC_3_4: params+= "C34"; break;
        case FEC_3_5: params+= "C35"; break;
        case FEC_4_5: params+= "C45"; break;
        case FEC_5_6: params+= "C56"; break;
        case FEC_6_7: params+= "C67"; break;
        case FEC_7_8: params+= "C78"; break;
        case FEC_8_9: params+= "C89"; break;
        case FEC_9_10: params+="C910"; break;
    }
    
    switch(inversion) {
      case INVERSION_OFF: params+="I0" ; break;
      case INVERSION_ON: params+="I1" ; break;
    }
    
    switch(modulation) {
        case QAM_16: params+=   "M16";  break;
        case QAM_32: params+=   "M32";  break;
        case QAM_64: params+=   "M64";  break;
        case QAM_128: params+=  "M128"; break;
        case QAM_256: params+=  "M256"; break;
        case QPSK: params+=     "M2";   break;
        case PSK_8: params+=    "M5";   break;
        case APSK_16: params+=  "M6";   break;
        case APSK_32: params+=  "M7";   break;
        case VSB_8: params+=    "M10";  break;
        case VSB_16: params+=   "M11";  break;
        case DQPSK: params+=    "M12";  break;
    }
    
    if (delsys == SYS_DVBS2) {
      switch(pilot) {
        case PILOT_OFF: params+= "N0"; break;
        case PILOT_ON:  params+= "N1"; break;
      }
      switch(rolloff) {
        case ROLLOFF_AUTO: params+= "O0"; break;
        case ROLLOFF_20:   params+= "O20"; break;
        case ROLLOFF_25:   params+= "O25"; break;
        case ROLLOFF_35:   params+= "O35"; break;
      }
      snprintf(tmp,8,"P%d",streamid);
      params+=tmp;
      params+="S1";
    } else {
      params+="S0";
    }

    channel->SetId(channels, entry->network_id, entry->transport_id, entry->service_id);
    channel->SetTransponderData(m_source->Code(), freq, symbolrate/1000, params.c_str());
    channel->SetName(entry->channel,"","");
    //channel->SetPids(entry->video_pid_len > 0 ? entry->video_pids[0] : 0, 
    dsyslog("ChanFromEntry %s",channel->ToText());
}

void dvbScanner::AddChannels(dvb_file *dvb_file_new)
{   
   //add/update found channels to vdr channels
#include <vdr/channels.h>
   //FIXME
                    
   cChannel *newCh;
   if (dvb_file_new) {
       cStateKey WriteState;
       cChannels* WChannels = (cChannels*) cChannels::GetChannelsWrite(WriteState, 30000);
       if (WChannels) {
           int line=0;
           for (dvb_entry *entry=dvb_file_new->first_entry; entry != NULL && WChannels != NULL ; entry = entry->next) {
               line++;
               if (!entry->channel) {
                   esyslog("missing channel name, skipping entry %d",line);
               }
               if (entry->video_pid_len == 0 && entry->audio_pid_len == 0) {
                   esyslog("skip entry %s, no audio or video pids", entry->channel);
                   continue;
               }
               dsyslog("examining entry %d channel %s vchannel %s",line,entry->channel,entry->vchannel);
               m_channels++;
               newCh=NULL;
               for(int i = 0; i < WChannels->Count(); i++) {
                   cChannel* ch = WChannels->Get(i);
                   if (ch->Nid() != entry->network_id ||
                       ch->Tid() != entry->transport_id ||
                       ch->Sid() != entry->service_id )
                       continue;
                   newCh=ch;
                   break;
               }
               
               if (newCh) {
                 dsyslog("updating channel");
                 ChanFromEntry(WChannels,newCh,entry);
                   
               } else {
                 m_newchannels++;
                 newCh = new cChannel;
                 ChanFromEntry(WChannels,newCh,entry);
                 //WChannels->Add(c);
                 dsyslog("added channel");
               }
           }
       WriteState.Remove();    
       } else {
           esyslog("Could not get Channels for writing!");
       }
   }
}
