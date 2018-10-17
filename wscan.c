/*
 * wscan.c: integrate wirbelscan in the actuator plugin using its service interface 
 * 
 * adapted from the wirbelscancontrol plugin
 */


#include "wscan.h"
using namespace WIRBELSCAN_SERVICE;

SListItem * cbuf = NULL;
SListItem * sbuf = NULL;

cPreAllocBuffer CountryBuffer;
cPreAllocBuffer SatBuffer;

#define MIN_CMDVERSION     1    // command api 0001
#define MIN_STATUSVERSION  1    // query status
#define MIN_SETUPVERSION   1    // get/put setup, GetSetup#XXXX/SetSetup#XXXX
#define MIN_COUNTRYVERSION 1    // get list of country IDs and Names
#define MIN_SATVERSION     1    // get list of sat IDs and Names
#define MIN_USERVERSION    1    // scan user defined transponder

int smode;
int channelcount0;
int channelcount1;
int view;
int singlescan;
int progress;
time_t start;


//---- debugging -------------------------------------------------------------
//#define OSD_DBG 1

#ifdef OSD_DBG
int it;
#define _debug(format, args...) printf (format, ## args)
#else
#define _debug(format, args...)
#endif

//---- macro definitions. ----------------------------------------------------
#define CHECKVERSION(a,b,c) p=strchr((char *) info->a,'#') + 1; sscanf(p,"%d ",&version); if (version < b) c = true;
#define CHECKLIMITS(a,v,_min,_max,_def) a=v; if ((a<_min) || (a>_max)) a=_def;
#define freeAndNull(p)   if(p) { free(p);   p=NULL; }
#define deleteAndNull(p) if(p) { delete(p); p=NULL; }


// --- cWscanner --------------------------------------------------------------

cWscanner::cWscanner(void)
{
  int version=0;  
  char *p;
  bool unsupported = false;

  ok = false;
  
  memset(&systems,0,sizeof(systems));
  smode = singlescan = 0;
  Tp_unsupported = false;

  if (! (wirbelscan = cPluginManager::GetPlugin("wirbelscan"))) {
     //FIXME Add(new cOsdItem(tr("wirbelscan was not found - pls install.")));
     return;
     }

  info = new cWirbelscanInfo;
  asprintf(&p, "%s%s", SPlugin, SInfo);
  if (! wirbelscan->Service("wirbelscan_GetVersion", info)) {
     free(p);
     //FIXME Add(new cOsdItem(tr("Your wirbelscan version doesnt"    )));
     //FIXME Add(new cOsdItem(tr("support services - Please upgrade.")));
     return;
     }
  free(p);

  CHECKVERSION(CommandVersion,MIN_CMDVERSION,     unsupported);
  CHECKVERSION(StatusVersion, MIN_STATUSVERSION,  unsupported);
  CHECKVERSION(SetupVersion,  MIN_SETUPVERSION,   unsupported);
  CHECKVERSION(CountryVersion,MIN_COUNTRYVERSION, unsupported);
  CHECKVERSION(SatVersion,    MIN_SATVERSION,     unsupported);
  CHECKVERSION(UserVersion,   MIN_USERVERSION,    Tp_unsupported);

  if (unsupported) {
     //FIXME Add(new cOsdItem(tr("Your wirbelscan version is")));
     //FIXME Add(new cOsdItem(tr("too old - Please upgrade." )));
     return;
     }

  asprintf(&p, "%sGet%s", SPlugin, SSetup);
  wirbelscan->Service(p, &setup);
  free(p);

  if (!Tp_unsupported) {
     asprintf(&p, "%sGet%s", SPlugin, SUser);
     wirbelscan->Service(p, &userdata);
     free(p);
     }

  CHECKLIMITS(sat,      setup.SatId,           0 ,0xFFFF,0);
  CHECKLIMITS(country,  setup.CountryId       ,0 ,0xFFFF,0);
  CHECKLIMITS(source,   setup.DVB_Type        ,0 ,5     ,0);
  CHECKLIMITS(terrinv,  setup.DVBT_Inversion  ,0 ,1     ,0);
  CHECKLIMITS(cableinv, setup.DVBC_Inversion  ,0 ,1     ,0);
  CHECKLIMITS(srate,    setup.DVBC_Symbolrate ,0 ,16    ,0);
  CHECKLIMITS(qam,      setup.DVBC_QAM        ,0 ,4     ,0);
  CHECKLIMITS(atsc,     setup.ATSC_type       ,0 ,1     ,0);
  view = -1;

  CountryBuffer.size = 0;
  CountryBuffer.count = 0;
  CountryBuffer.buffer = NULL;
  asprintf(&p, "%sGet%s", SPlugin, SCountry);      
  wirbelscan->Service(p, &CountryBuffer);                             // query buffer size.
  cbuf = (SListItem*) malloc(CountryBuffer.size * sizeof(SListItem)); // now, allocate memory.
  CountryBuffer.buffer = cbuf;                                        // assign buffer
  wirbelscan->Service(p, &CountryBuffer);                             // fill buffer with values.
  free(p);

  SatBuffer.size = 0;
  SatBuffer.count = 0;
  SatBuffer.buffer = NULL;
  asprintf(&p, "%sGet%s", SPlugin, SSat);     
  wirbelscan->Service(p, &SatBuffer);                                 // query buffer size.
  cbuf = (SListItem*) malloc(SatBuffer.size * sizeof(SListItem));     // now, allocate memory.
  SatBuffer.buffer = cbuf;                                            // assign buffer
  wirbelscan->Service(p, &SatBuffer);                                 // fill buffer with values.
  free(p);
  
  ok = true;

}

cWscanner::~cWscanner()
{
  freeAndNull(cbuf);
  freeAndNull(sbuf);
}

int cWscanner::GetSatId(int source)
{
   char buffer[16];

   int n=cSource::Position(source);
   
   if (n) {
        char *q = buffer;
        *q++ = (source & cSource::st_Mask) >> 24;
        snprintf(q, sizeof(buffer) - 2, "%u%c%u", abs(n) / 10,  (n < 0) ? 'W' : 'E', abs(n) % 10);
        for (int i=0; i<SatBuffer.count; i++) {
            if (strcmp(buffer,SatBuffer.buffer[i].short_name) == 0 )
                return SatBuffer.buffer[i].id;
        }
   }
   return -1;
}    

void cWscanner::StartScan(int id)
{
    setup.SatId=id;
    setup.DVB_Type=2; //satellite
    setup.scanflags=/* TV = */ (1 << 0) | /* RADIO = */ (1 << 1) | /* FTA = */ (1 << 2) | /* SCRAMBLED = */ (1 << 4) | /* HDTV = */ (1 << 5);

    char *s;
    asprintf(&s, "%sSet%s", SPlugin, SSetup);
    wirbelscan->Service(s, &setup);
    free(s);
    PutCommand(CmdStartScan);    
}

void cWscanner::StartScanSingle(cChannel * Channel, cDvbTransponderParameters * Transponder)
{
    setup.DVB_Type=999;

    setup.scanflags=/* TV = */ (1 << 0) | /* RADIO = */ (1 << 1) | /* FTA = */ (1 << 2) | /* SCRAMBLED = */ (1 << 4) | /* HDTV = */ (1 << 5);
  
    cUserTransponder * Utransponder;
  /* Transponder = new cUserTransponder(sat, satsystem, frequency, polarisation, symbolrate,
                                               modulation, fec_hp, 0, 0, rolloff, useNit); */
    Utransponder = new cUserTransponder(sat,
                                        Transponder->System() == SYS_DVBS ? 0 : 1,  /*0 DVB-S 1 DVB-S2 */
                                        Channel->Frequency(),
                                        Transponder->Polarization(),
                                        Channel->Srate(), //symbolrate
                                        Transponder->Modulation(),
                                        0, //fec_hp,
                                        0,
                                        0,
                                        Transponder->RollOff(),
                                        true); //useNit
    
    userdata[0] = *(Utransponder->Data() + 0);
    userdata[1] = *(Utransponder->Data() + 1);
    userdata[2] = *(Utransponder->Data() + 2);
    char *s;
    asprintf(&s, "%sSet%s", SPlugin, SUser);
    wirbelscan->Service(s, &userdata);
    free(s);         
    asprintf(&s, "%sSet%s", SPlugin, SSetup);
    wirbelscan->Service(s, &setup);
    free(s);
}

void cWscanner::StopScan(void)
{
    PutCommand(CmdStopScan);
}
    

static const char * DVB_Types[] = {"DVB-T - Terrestrisch","DVB-C - Cable","DVB-S/S2 - Satellite", "analog TV - pvrinput","analog Radio - pvrinput","ATSC (North America)"};
static const char * INV[]       = {"AUTO (fallback: OFF)", "AUTO (fallback: ON)"};
static const char * SR[]        = {"AUTO", "6900", "6875", "6111", "6250", "6790", "6811", "5900", "5000", "3450", "4000", "6950", "7000", "6952", "5156", "5483", "ALL" };
static const char * QAM[]       = {"AUTO", "QAM-64", "QAM-128", "QAM-256", "ALL"};
static const char * ATSC[]      = {"VSB (aerial DTV)", "QAM (cable DTV)"};
static const char * MODE[]      = {"AUTO", "single transponder"};
static const char * FEC[]       = {"OFF", "1/2", "2/3", "3/4", "4/5", "5/6", "6/7", "7/8", "8/9", "AUTO", "3/5", "9/10"};
static const char * BW[]        = {"8MHz", "7MHz", "6MHz", "5MHz" };
static const char * GUARD[]     = {"1/32", "1/16", "1/8", "1/4" };
static const char * HIERARCHY[] = {"OFF", "alpha = 1", "alpha = 2", "alpha = 4"};
static const char * TRANSM[]    = {"2k", "8k", "4k"};
static const char * SATSYS[]    = {"DVB-S", "DVB-S2"};
static const char * POL[]       = {"H", "V", "L", "R"};
static const char * MOD[]       = {"QPSK", "QAM16", "QAM32","QAM64","QAM128","QAM256","QAM-AUTO","VSB8","VSB16","PSK8"};
static const char * RO[]        = {"0.35", "0.25", "0.20"};

static const int    FEC_S[]     = { 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 };
static const int    FEC_T[]     = { 1, 1, 1, 1, 0, 1, 0, 1, 0, 0, 0, 0 }; 
static const int    BW_T[]      = { 1, 1, 1, 0  }; 
static const int    TRANSM_T[]  = { 1, 1, 0  };
static const int    MOD_A[]     = { 0, 0, 0, 1, 0, 1, 1, 1, 1, 0 };
static const int    MOD_C[]     = { 0, 0, 0, 1, 1, 1, 0, 0, 0, 0 };
static const int    MOD_S[]     = { 1, 0, 0, 0, 0, 0, 0, 0, 0, 1 };
static const int    MOD_T[]     = { 1, 1, 0, 1, 0, 0, 0, 0, 0, 0 };




void cWscanner::PutCommand(WIRBELSCAN_SERVICE::s_cmd command)
{
  cWirbelscanCmd cmd;
  char * request;
  asprintf(&request, "%s%s", SPlugin, SCommand);
 
  cmd.cmd = command;
  wirbelscan->Service(request, &cmd);
  free(request);
}

void cWscanner::GetStatus(cWirbelscanStatus *status)
{
  char * s;

  asprintf(&s, "%sGet%s", SPlugin, SStatus);
  wirbelscan->Service(s, status);
  free(s);

}




