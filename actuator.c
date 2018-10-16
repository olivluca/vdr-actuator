/*
 * actuator.c: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id$
 */

//#include <linux/dvb/frontend.h>
#include <linux/dvb/version.h>
#include <sys/ioctl.h>
#include <vdr/config.h>
#include <vdr/diseqc.h>
#include <vdr/plugin.h>
#include <vdr/sources.h>
#include <vdr/menuitems.h>
#include <vdr/dvbdevice.h>
#include <vdr/device.h>
#include <vdr/channels.h>
#include <vdr/status.h>
#include <vdr/positioner.h>
#include <vdr/interface.h>
#include <vdr/osd.h>
#include <vdr/pat.h>
#include <math.h>
#include <getopt.h>
#include "scanner.h"
#include "module/actuator.h"

static const char *VERSION        = "2.4.0";
static const char *DESCRIPTION    = trNOOP("Linear or h-h actuator control");
static const char *MAINMENUENTRY  = trNOOP("Actuator");

//Selectable buttons on the plugin menu: indexes
enum menuindex {
  MI_DRIVEEAST=0,  MI_HALT,                MI_DRIVEWEST,
  MI_RECALC,       MI_GOTO,                MI_STORE,
  MI_STEPSEAST,    MI_ENABLEDISABLELIMITS, MI_STEPSWEST,
  MI_SETEASTLIMIT, MI_SETZERO,             MI_SETWESTLIMIT,
                   MI_SATPOSITION,
                   MI_FREQUENCY,
  MI_SYMBOLRATE,   MI_SYSTEM,              MI_MODULATION,
                   MI_VPID,
                   MI_APID,
                   MI_SCANTRANSPONDER,
                   MI_SCANSATELLITE,
  MI_MARK,         MI_UNMARK,              MI_DELETE                 
  };


//Items per row
static const int itemsperrow[] = {
                                 3,
                                 3,
                                 3,
                                 3,
                                 1,
                                 1,
                                 3,
                                 1,
                                 1,
                                 1,
                                 1,
                                 3
                                 };

#define MAXMENUITEM MI_DELETE
#define MAXROW 11
#define MINROWSCANONLY 4
#define MINITEMSCANONLY MI_SATPOSITION


//Selectable buttons on the plugin menu: captions
static const char *menucaption[] = { 
                                   trNOOP("Drive East"), trNOOP("Halt"), trNOOP("Drive West"),
                                   trNOOP("Recalc"),trNOOP("Goto %d"),trNOOP("Store"),
                                   trNOOP("%d Steps East"),"*",trNOOP("%d Steps West"),
                                   trNOOP("Set East Limit"),trNOOP("Set Zero"), trNOOP("Set West Limit"),
                                   "", //Sat position
                                   trNOOP("Frequency:"),
                                   trNOOP("SR:"),trNOOP("Sys:"),trNOOP("Mod:"),
                                   trNOOP("Vpid:"),
                                   trNOOP("Apid:"),
                                   trNOOP("Scan Transponder"),
                                   trNOOP("Scan Satellite"),
                                   trNOOP("Mark channels"), trNOOP("Unmark channels"), trNOOP("Delete marked channels")
                                  };
#define ENABLELIMITS trNOOP("Enable Limits")
#define DISABLELIMITS trNOOP("Disable Limits")
                                  
//Number of allowed input digits for each menu item                                  
static const int menudigits[] = {
                                   0,0,0,
                                   0,4,0,
                                   2,0,2,
                                   0,0,0,
                                   0,
                                   5,
                                   5,0,0,
                                   4,
                                   4,
                                   0,
                                   0,
                                   0,0
                                   };                                  

//Dish messages
#define dishmoving    trNOOP("Dish target: %d, position: %d")
#define channelsfound trNOOP("Channels found: %d, new: %d")
#define dishreached   trNOOP("Position reached")
#define dishwait      trNOOP("Motor wait")
#define disherror     trNOOP("Motor error")
#define dishnopos     trNOOP("Position not set")
#define notpositioned trNOOP("Not positioned")
#define atlimits      trNOOP("Dish at limits")
#define outsidelimits trNOOP("Position outside limits")
#define scanning      trNOOP("Scanning, press any key to stop")

//Positioning tolerance
//(for transponder scanning and restore of Setup.UpdateChannels)
#define POSTOLERANCE 5

//Plugin parameters 
int WestLimit=0;
bool ScanOnly=false; //use the plugin only as channel scanner, not as a positioner
unsigned int MinRefresh=250;
int ShowChanges=1;
//EastLimit is always 0


//for telling MainThreadHook it's time to show the position
bool DishIsMoving=false;
//main menu used only to show the position, not to interact with the actuator
bool PositionDisplay=false;
//delay to show the position
cTimeMs PosDelay;

//how to access SetupStore inside the main menu without this kludge?
cPlugin *theplugin;

//------ Theme ----------------------------------------------------------------

static cTheme Theme;

THEME_CLR(Theme, Background, clrGray50);
THEME_CLR(Theme, HeaderBg, clrYellow);
THEME_CLR(Theme, HeaderText, clrBlack);
THEME_CLR(Theme, HeaderBgError, clrRed);
THEME_CLR(Theme, HeaderTextError, clrWhite);
THEME_CLR(Theme, RedBar, clrRed);
THEME_CLR(Theme, YellowBar, clrYellow);
THEME_CLR(Theme, GreenBar, clrGreen);
THEME_CLR(Theme, StatusText, clrWhite);
THEME_CLR(Theme, SignalOk, clrYellow);
THEME_CLR(Theme, SignalNo, clrBlack);
THEME_CLR(Theme, NormalText, clrYellow);
THEME_CLR(Theme, NormalBg, clrGray50);
THEME_CLR(Theme, SelectedText, clrBlack);
THEME_CLR(Theme, SelectedBg, clrYellow);
THEME_CLR(Theme, MessageText, clrWhite);
THEME_CLR(Theme, MessageBg, clrRed);
THEME_CLR(Theme, ProgressBar, clrBlue);
THEME_CLR(Theme, ProgressText, clrWhite);


// --- cSatPosition -----------------------------------------------------------
// associates one source with its position

class cSatPosition:public cListObject {
private:
  int source;
  int longitude;
  int position;
public:
  cSatPosition(void) {}  
  cSatPosition(int Source, int Position);
  ~cSatPosition();
  bool Parse(const char *s);
  bool Save(FILE *f);
  virtual int Compare(const cListObject &ListObject) const { return Position()-((const cSatPosition *)&ListObject)->Position(); }
  int Source(void) const { return source; }
  int Longitude(void) const { return longitude; }
  int Position(void) const { return position; }
  bool SetPosition(int Position);
  };

cSatPosition::cSatPosition(int Source, int Position)
{
  source = Source;
  position = Position;
}  

cSatPosition::~cSatPosition()
{
}  

bool cSatPosition::Parse(const char *s)
{
  bool result = false;
  char *sourcebuf = NULL;
  if (2 == sscanf(s, "%m[^ ] %d", &sourcebuf, &position)) { 
    source = cSource::FromString(sourcebuf);
    longitude = cSource::Position(source);
    if (Sources.Get(source)) result = true;
    else esyslog("ERROR: unknown source '%s'", sourcebuf);
    }
  free(sourcebuf);
  return result;     
}

bool cSatPosition::Save(FILE *f)
{
  return fprintf(f, "%s %d\n", *cSource::ToString(source), position) > 0; 
}

bool cSatPosition::SetPosition(int Position)
{
  position = Position;
  return true;
}

// --- cSatPositions --------------------------------------------------------
// All the positions known

class cSatPositions : public cConfig<cSatPosition> {
public:
  cSatPosition *Get(int Longitude);
  int Longitude(int Position);
  void Recalc(int offset);
  bool Load(const char * Filename);
  bool Save(void);
  };  

cSatPosition *cSatPositions::Get(int Longitude)
{
  for (cSatPosition *p = First(); p; p = Next(p)) {
    if (p->Longitude() == Longitude)
      return p;
    }
  return NULL;
}

int cSatPositions::Longitude(int Position)
{
  int pos1 = 0;
  int long1 = -1800;
  int pos2;
  int long2;
  for (cSatPosition *p = First(); p; p = Next(p)) {
    pos2=p->Position();
    long2=p->Longitude();
    if (pos2 == Position)
      return long2;
    if (Position >= pos1 && Position < pos2) {
      int retvalue=long1 + (pos2-Position) * (long2-long1) / (pos2-pos1);
      return retvalue;
    }  
    pos1 = pos2; 
    long1 = long2;
  }
  return 1800;   
}

void cSatPositions::Recalc(int offset)
{ 
  for (cSatPosition *p = First(); p; p = Next(p)) p->SetPosition(p->Position()+offset);
  Save();
}

bool cSatPositions::Load(const char * Filename)
{
  if (cConfig<cSatPosition>::Load(Filename)) {
     Sort();
     return true;
     }
  return false;
}

bool cSatPositions::Save(void)
{
  Sort();
  return cConfig<cSatPosition>::Save();
}

cSatPositions SatPositions;

//--- cPosTracker -----------------------------------------------------------
// Follows the dish when it's moving to store the position on disk

class cActuator;

class cPosTracker:private cThread {
private:
  cActuator * m_actuator;
  int LastPositionSaved;
  const char *positionfilename;
public:
  cPosTracker(cActuator *actuator);    
  ~cPosTracker(void);
  void SavePos(int newpos);
  void Track(void) { Start(); }
protected:
  void Action(void);
};

//------ declaration of cActuator (since it's used by the implementation of cPosTracker

class cActuator:public cPositioner {
private:
  cPosTracker * m_PosTracker;
  mutable cMutex mutex;
  bool m_Error;
  cString m_ErrorText;
  //actuator device
  int fd_actuator;
  int m_Target;
public:
  cActuator();
  ~cActuator();
  int Fd_Frontend(void) { return Frontend(); }
  bool Opened(void) { return fd_actuator > 0; }
  void Status(actuator_status * status) const;
  void SetPos(int position);
  void InternalGotoPosition(int position);
  void RestoreTarget(void);
  virtual cString Error(void) const;
  virtual void Drive(ePositionerDirection Direction);
  virtual void Step(ePositionerDirection Direction, uint Steps = 1);
  virtual void Halt(void);
  virtual void SetLimit(ePositionerDirection Direction);
  virtual void DisableLimits(void);
  virtual void EnableLimits(void);
  virtual void StorePosition(uint Number);
  virtual void RecalcPositions(uint Number);
  virtual void GotoPosition(uint Number, int Longitude);
  virtual void GotoAngle(int Longitude);
  virtual int CurrentLongitude(void) const;
  virtual bool IsMoving(void) const;

};

//--- cPosTracker implementation ----------------------------------------------------

cPosTracker::cPosTracker(cActuator * actuator):cThread("Position Tracker")
{
  m_actuator = actuator;
  positionfilename=strdup(AddDirectory(cPlugin::ConfigDirectory(), "actuator.pos"));
  actuator_status status;
  m_actuator->Status(&status);
  if (status.position==0) {
      //driver just loaded, try to get the position from the file
      FILE *f=fopen(positionfilename,"r");
      if(f) {
        int newpos;
        if (fscanf(f,"%d",&newpos)==1) { 
          isyslog("Read position %d from %s",newpos,positionfilename); 
          m_actuator->SetPos(newpos);
          LastPositionSaved=newpos;
        } else esyslog("couldn't read dish position from %s",positionfilename);
        fclose(f);
      } else esyslog("Couldn't open file %s: %s",positionfilename,strerror(errno));
  }  else {
      //driver already loaded, trust it
      LastPositionSaved=status.position+1; //force a save
      SavePos(status.position);
  }  
}

cPosTracker::~cPosTracker(void)
{
  m_actuator->Halt();
  sleep(1);
  actuator_status status;
  m_actuator->Status(&status);
  SavePos(status.position);
    printf("actuator: saved dish position\n");
}

void cPosTracker::SavePos(int NewPos)
{
  if(NewPos!=LastPositionSaved) {
    cSafeFile f(positionfilename);
    if (f.Open()) {
      fprintf(f,"%d",NewPos);
      f.Close();
      LastPositionSaved=NewPos;
    } else LOG_ERROR;
  }    
}

void cPosTracker::Action(void)
{
  actuator_status status;
  while(Running()) {
    m_actuator->Status(&status);
    SavePos(status.position);
    if (status.state==ACM_IDLE) {
      DishIsMoving=false;
      return;
    } else if (status.state==ACM_WEST || status.state==ACM_EAST) DishIsMoving=true;  
    usleep(50000);
  }  
}

// --- cActuator -------------------------------------------------------

cActuator::cActuator() : cPositioner()
{
  m_Target=0;
  m_Error=false;
  m_PosTracker=NULL;
  SetCapabilities(pcCanDrive |
                  pcCanStep |
                  pcCanHalt |
                  pcCanSetLimits |
                  pcCanDisableLimits |
                  pcCanEnableLimits |
                  pcCanStorePosition |
                  pcCanRecalcPositions |
                  pcCanGotoPosition |
                  pcCanGotoAngle
                  );
  fd_actuator = open("/dev/actuator",0);
  if (fd_actuator<0) {
    esyslog("cannot open /dev/actuator");
    return;
  }
  m_PosTracker=new cPosTracker(this);
}

cActuator::~cActuator()
{
  if (m_PosTracker) delete m_PosTracker;
  close(fd_actuator);
}

void cActuator::Status(actuator_status * status) const
{
  cMutexLock MutexLock(&mutex);
  CHECK(ioctl(fd_actuator, AC_RSTATUS, status));
}

void cActuator::SetPos(int position)
{
  cMutexLock MutexLock(&mutex);
  int newpos=position;
  CHECK(ioctl(fd_actuator, AC_WPOS, &newpos));
  m_PosTracker->Track();
}


void cActuator::InternalGotoPosition(int position)
{
  cMutexLock MutexLock(&mutex);
  int newpos=position;
  CHECK(ioctl(fd_actuator, AC_WTARGET, &newpos));
  m_PosTracker->Track();
}

void cActuator::RestoreTarget(void)
{
  cMutexLock MutexLock(&mutex);
  GotoAngle(m_Target);
}


cString cActuator::Error(void) const
{
  cMutexLock MutexLock(&mutex);
  if (m_Error)
    return m_ErrorText;
  return NULL;
}

void cActuator::Drive(ePositionerDirection Direction)
{
  cMutexLock MutexLock(&mutex);
  if (Direction==cPositioner::pdRight)
  {
    CHECK(ioctl(fd_actuator, AC_MWEST));
  } else {
    CHECK(ioctl(fd_actuator, AC_MEAST));
  }  
  m_PosTracker->Track();    
}

void cActuator::Step(ePositionerDirection Direction, uint Steps)
{
  cMutexLock MutexLock(&mutex);

  actuator_status status;
  int newpos;
  
  Status(&status);
  if (Direction==cPositioner::pdRight)
  {
    newpos=status.position+Steps;
  } else {
    newpos=status.position-Steps;
  }  
  CHECK(ioctl(fd_actuator, AC_WTARGET, &newpos));
  m_PosTracker->Track();    
}

void cActuator::Halt(void)
{
  cMutexLock MutexLock(&mutex);
  CHECK(ioctl(fd_actuator, AC_MSTOP));
}

void cActuator::SetLimit(ePositionerDirection Direction)
{
 //FIXME
}

void cActuator::DisableLimits(void)
{
  //FIXME
}

void cActuator::EnableLimits(void)
{
  //FIXME
}

void cActuator::StorePosition(uint Number)
{
  //FIXME
}

void cActuator::RecalcPositions(uint Number)
{
  //FIXME
}

void cActuator::GotoPosition(uint Number, int Longitude)
{
  //FIXME
}

void cActuator::GotoAngle(int Longitude)
{
  cMutexLock MutexLock(&mutex);
  actuator_status status;
  cSatPosition *p=SatPositions.Get(Longitude);
  cPositioner::GotoAngle(Longitude); //to update the target shown in skins...the positioner interface isn't well thought out
  int target=-1;  //in case there's no target stop the motor
  m_Target=Longitude;
  if (p) {
    target=p->Position();
    //dsyslog("New sat position: %i",target);
    if (target>=0 && target<=WestLimit) {
      Status(&status);
      if (status.position!=target || status.target!=target)  {
        //dsyslog("Satellite changed");
        InternalGotoPosition(target);
      } else {  
        m_PosTracker->Track();
      }  
      m_Error=false;
    } else {
      Skins.Message(mtError, tr(outsidelimits));
      m_Error=true;
      m_ErrorText=tr(outsidelimits);
      target=-1;
    }  
  } else {
     Skins.Message(mtError, tr(dishnopos));
     m_Error=true;
     m_ErrorText=tr(dishnopos);
  }   
  if (target==-1) Halt();
}

int cActuator::CurrentLongitude(void) const
{
  cMutexLock MutexLock(&mutex);
  actuator_status status;
  Status(&status);
  return SatPositions.Longitude(status.position); 
}

bool cActuator::IsMoving(void) const
{
  cMutexLock MutexLock(&mutex);
  actuator_status status;
  Status(&status);
  if (status.state != ACM_IDLE)
    return true;
  return SatPositions.Longitude(status.position) != m_Target;
}

cActuator * Actuator = NULL;

// --- cMenuSetupActuator ------------------------------------------------------

class cMenuSetupActuator : public cMenuSetupPage {
private:
  int newMinRefresh;
  int newShowChanges;
  cThemes themes;
  int themeIndex;
protected:
  virtual void Store(void);
public:
  cMenuSetupActuator(void);
  };

cMenuSetupActuator::cMenuSetupActuator(void)
{
  newMinRefresh=MinRefresh;
  newShowChanges=ShowChanges;
  themes.Load("actuator");
  themeIndex=themes.GetThemeIndex(Theme.Description());
  Add(new cMenuEditIntItem( tr("Min. screen refresh time (ms)"), &newMinRefresh,10,1000));
  Add(new cMenuEditBoolItem( tr("Show dish moving"), &newShowChanges));
  if (themes.NumThemes())
  Add(new cMenuEditStraItem(tr("Setup.OSD$Theme"),&themeIndex, themes.NumThemes(), themes.Descriptions()));
}

void cMenuSetupActuator::Store(void)
{
  SetupStore("MinRefresh", MinRefresh=newMinRefresh);
  SetupStore("ShowChanges", ShowChanges=newShowChanges);
  SetupStore("Theme", themes.Name(themeIndex));
  themes.Load("actuator",themes.Name(themeIndex), &Theme);
}


// --- cTransponder -----------------------------------------------------------
//Transponder data for satellite scan

class cTransponder:public cListObject {
private:
  int frequency;
  int srate;
  int system;
  int modulation;
  char polarization;
  
public:
  cTransponder(void) { frequency=0; }  
  ~cTransponder();
  bool Parse(const char *s);
  int Frequency(void) const { return frequency; }
  int Srate(void) const { return srate; }
  int System(void) const { return system; }
  int Modulation(void) const { return modulation; }
  char Polarization(void) const { return polarization; }
  };


cTransponder::~cTransponder()
{
}  

//I'm too lazy to write my own file reading function, so I use the standard one and
//will trim later invalid "entries"
bool cTransponder::Parse(const char *s)
{
  int dummy;
  int locsystem;
  int locmodulation;
  int fec;
  char modstring[10];
  char sysstring[3];
  bool parsed=false;
  
  if (7 == sscanf(s,"%d =%d , %c , %d , %d , %2s ; %9s", &dummy, &frequency, &polarization, &srate, &fec, sysstring, modstring)) { //DVBS2
    if (strcasecmp(sysstring,"S2")==0) {
      locsystem=SYS_DVBS2;
      if (strcasecmp(modstring,"8PSK")==0)
        locmodulation=PSK_8;
      else if (strcasecmp(modstring,"16APSK")==0)
        locmodulation=APSK_16;
      else if (strcasecmp(modstring,"32APSK")==0)
        locmodulation=APSK_32;
      else if (strcasecmp(modstring,"DIRECTV")==0)
        locmodulation=DQPSK;
      else        
        locmodulation=QPSK;
      parsed=true;
      }
  }    
  
  if (!parsed && 4 == sscanf(s, "%d =%d , %c ,%d ", &dummy, &frequency, &polarization, &srate)) { //DVBS
    locsystem=SYS_DVBS;
    locmodulation=QPSK;
    parsed=true;
    }
  
  if (!parsed) {
    frequency=0; //hack 
    } else {
    system=locsystem;
    modulation=locmodulation;
    if (polarization=='v') polarization='V';
    if (polarization=='h') polarization='H';
    if (polarization=='l') polarization='L';
    if (polarization=='r') polarization='R';
    if (polarization!='V' && polarization!='H' && polarization!='L' && polarization!='R') frequency=0; //hack
    }
  return true; //always, will trim invalid entries later on  
}


// --- cTransponders --------------------------------------------------------
//All the transponders of the current satellite

class cTransponders : public cConfig<cTransponder> {
public:
  void LoadTransponders(int source);
  };  

void cTransponders::LoadTransponders(int source)
{
  cList<cTransponder>::Clear();
  int satlong = source;
  if (satlong < 0 ) {
    satlong = 3600 - satlong;
  }    
  char buffer[100];
  snprintf(buffer,sizeof(buffer),"transponders/%04d.ini",satlong);
  printf("============================ %s\n",buffer);
  Load(AddDirectory(cPlugin::ConfigDirectory(),buffer));
  cTransponder *p = First();
  while(p) {
    cTransponder *p2=Next(p);
    if (p->Frequency()==0) Del(p);
    p=p2;
  }
  //*********
  //p=First();
  //while(p) {
  //  printf("freq: %d  pol: %c   sr: %d\n", p->Frequency(), p->Polarization(), p->Srate());
  //  p=Next(p);
  //  }
  //printf("transponder file : %s   count: %d\n", buffer, Count());
}  

// -- cMainMenuActuator --------------------------------------------------------

class cMainMenuActuator : public cOsdObject {
private:
  cDvbDevice *ActuatorDevice;
  int digits,menucolumn,menuline,conf,repeat,HasSwitched,fd_frontend;
  enum em {
    EM_NONE,
    EM_OUTSIDELIMITS,
    EM_ATLIMITS,
    EM_NOTPOSITIONED
    } errormessage;
  bool LimitsDisabled;
  cSource *curSource;
  cSatPosition *curPosition;
  cTransponders *Transponders;
  cTransponder *curtransponder;
  int transponderindex; 
  int menuvalue[MAXMENUITEM+1];
  char Pol;
  cChannel *OldChannel;
  cChannel *SChannel;
  cOsd *osd;
  const cFont *textfont;
  tColor color;
  fe_status_t fe_status;
  uint16_t fe_ss;
  uint16_t fe_snr;
  uint32_t fe_ber;
  uint32_t fe_unc;
  cTimeMs *scantime;
  cTimeMs *refresh;
  cTimeMs *lockstable;
  bool HideOsd;
  cChannelScanner *Scanner;
  enum sm {
    SM_NONE,
    SM_TRANSPONDER,
    SM_SATELLITE
    } scanmode;
  enum ss {
    SS_WAITING_FOR_LOCK,
    SS_NO_LOCK,
    SS_SCANNING
  } scanstatus;
  bool showScanResult;
  int TotalFound,NewFound;  
  void DisplayOsd(void);
  void GetSignalInfo(void);
  void Tune(bool live=true);
  void StartScan(bool live=true);
  void StopScan(void);
  void MarkChannels(void);
  void UnmarkChannels(void);
  void DeleteMarkedChannels(void);
  int Selected(void);
  tColor clrBackground;
  tColor clrHeaderBg;
  tColor clrHeaderText;
  tColor clrHeaderBgError;
  tColor clrHeaderTextError;
  tColor clrRedBar;
  tColor clrYellowBar;
  tColor clrGreenBar;
  tColor clrStatusText;
  tColor clrSignalOk;
  tColor clrSignalNo;
  tColor clrNormalText;
  tColor clrNormalBg;
  tColor clrSelectedText;
  tColor clrSelectedBg;
  tColor clrMessageText;
  tColor clrMessageBg;
  tColor clrProgressBar;
  tColor clrProgressText;
  int oldupdate;
public:
  cMainMenuActuator(void);
  ~cMainMenuActuator();
  void Refresh(void);
  virtual void Show(void);
  virtual eOSState ProcessKey(eKeys Key);
};


cMainMenuActuator::cMainMenuActuator(void)
{
  ActuatorDevice=NULL;
  oldupdate=-1;
  for (int i=0; i<cDevice::NumDevices() && ActuatorDevice==NULL; i++) {
    cDvbDevice * LocDvbDevice=dynamic_cast<cDvbDevice *>(cDevice::GetDevice(i));
    if (LocDvbDevice != nullptr)
      ActuatorDevice=LocDvbDevice;
  }   
  if (ActuatorDevice==NULL) return;
  //prevent auto update of channels while the menu is shown
  if (!PositionDisplay) {
    oldupdate=Setup.UpdateChannels;
    Setup.UpdateChannels=0;
  }  
  LOCK_CHANNELS_READ;
  OldChannel=(cChannel *)Channels->GetByNumber(ActuatorDevice->CurrentChannel());
  SChannel=new cChannel();
  repeat=HasSwitched=false;
  conf=0;
  errormessage=EM_NONE;
  menucolumn=menuvalue[MI_STEPSEAST]=menuvalue[MI_STEPSWEST]=1;
  menuline=1;
  if (ScanOnly) {
    menuline=MINROWSCANONLY;
    menucolumn=0;
  }
  osd=NULL;
  digits=0;
  LimitsDisabled=false;
  HideOsd=false;
  //if we're not going to interact with the actuator, don't save the value of UpdateChannels
  // FIXME if (!ScanOnly && !PositionDisplay) PosTracker->SaveUpdate();
  fd_frontend = Actuator->Fd_Frontend();
  curSource=Sources.Get(OldChannel->Source());
  curPosition=SatPositions.Get(curSource->Position());
  Transponders=new cTransponders();
  Transponders->LoadTransponders(curSource->Position());
  menuvalue[MI_SCANSATELLITE]=Transponders->Count();
  curtransponder=Transponders->First();
  transponderindex=1;
  if (curPosition) menuvalue[MI_GOTO]=curPosition->Position();
  else menuvalue[MI_GOTO]=0;
  menuvalue[MI_FREQUENCY]=OldChannel->Frequency();
  cDvbTransponderParameters dtp(OldChannel->Parameters());
  Pol=dtp.Polarization();
  if (Pol=='v') Pol='V';
  if (Pol=='h') Pol='H';
  if (Pol=='l') Pol='L';
  if (Pol=='r') Pol='R';
  menuvalue[MI_SYMBOLRATE]=OldChannel->Srate();
  menuvalue[MI_SYSTEM]=dtp.System();
  menuvalue[MI_MODULATION]=dtp.Modulation();
  menuvalue[MI_VPID]=OldChannel->Vpid();
  menuvalue[MI_APID]=OldChannel->Apid(0);
  //fast refresh to show signal information in a timely way
  SetNeedsFastResponse(true);
  if (Setup.UseSmallFont == 0) {
    // Dirty hack to force the small fonts...
    Setup.UseSmallFont = 1;
    textfont = cFont::GetFont(fontSml);
    Setup.UseSmallFont = 0;
  }
  else
    textfont = cFont::GetFont(fontSml);
  scanmode=SM_NONE;
  showScanResult=false;
  scantime=new cTimeMs();
  lockstable=new cTimeMs();
  refresh=new cTimeMs();
  refresh->Set(-MinRefresh);
  
  clrBackground=Theme.Color(Background);
  clrHeaderBg=Theme.Color(HeaderBg);
  clrHeaderText=Theme.Color(HeaderText);
  clrHeaderBgError=Theme.Color(HeaderBgError);
  clrHeaderTextError=Theme.Color(HeaderTextError);
  clrRedBar=Theme.Color(RedBar);
  clrYellowBar=Theme.Color(YellowBar);
  clrGreenBar=Theme.Color(GreenBar);
  clrStatusText=Theme.Color(StatusText);
  clrSignalOk=Theme.Color(SignalOk);
  clrSignalNo=Theme.Color(SignalNo);
  clrNormalText=Theme.Color(NormalText);
  clrNormalBg=Theme.Color(NormalBg);
  clrSelectedText=Theme.Color(SelectedText);
  clrSelectedBg=Theme.Color(SelectedBg);
  clrMessageText=Theme.Color(MessageText);
  clrMessageBg=Theme.Color(MessageBg);
  clrProgressBar=Theme.Color(ProgressBar);
  clrProgressText=Theme.Color(ProgressText);

}

cMainMenuActuator::~cMainMenuActuator()
{
  if (ActuatorDevice == NULL) return;
  delete osd;
  delete SChannel;
  delete Transponders;
  delete scantime;
  delete lockstable;
  delete refresh;
  if (oldupdate>=0)
    Setup.UpdateChannels=oldupdate;
  //Don't change channel if we're only showing the position
  if (PositionDisplay) {
    PositionDisplay=false;
    return;
  }  
  /* FIXME
  if (!ScanOnly) {
    Actuator->Halt();
    PosTracker->RestoreUpdate();
  }  
  */
  if (OldChannel) {
    if (HasSwitched) {
      if (cDevice::GetDevice(OldChannel,0,true)==ActuatorDevice) {
        cDevice::PrimaryDevice()->SwitchChannel(OldChannel, HasSwitched);
        return;
      }  
    }
    ActuatorDevice->SwitchChannel(OldChannel,true);
  }
  Actuator->RestoreTarget();  
}


void cMainMenuActuator::Refresh(void)
{
   if (refresh->Elapsed()>=MinRefresh) {
        refresh->Set(); 
        GetSignalInfo();
        DisplayOsd();
   }     
}

eOSState cMainMenuActuator::ProcessKey(eKeys Key)
{
  actuator_status status;
  if (ActuatorDevice == NULL) {
    Skins.Message(mtError,tr("Card not available"));
    return osEnd;
  }  
  int selected=Selected();
  int newpos;
  eOSState state = osContinue;
  if (!ScanOnly) {
    Actuator->Status(&status);
    if ((status.position<=0 && status.state==ACM_EAST || 
         status.position>=WestLimit && status.state==ACM_WEST) && !LimitsDisabled) {
      Actuator->Halt();
      errormessage=EM_ATLIMITS;
    }
  }  
  //Skip normal key processing if we're just showing the position
  if (PositionDisplay) {
    Refresh();
    if (Key != kNone) {
      cRemote::Put(Key);
      return osEnd;
    }
    if (status.state==ACM_IDLE) return osEnd;
    return osContinue;
  }
  
  //Hiding osd, show it again
  if (HideOsd && Key!=kNone) {
    HideOsd=false;
    Refresh();
    return osContinue;
  }
  //needsFastResponse=(status.state != ACM_IDLE);
  int oldcolumn,oldline;
  oldcolumn=menucolumn;
  oldline=menuline;
  
  //force a redraw
  if (Key!=kNone) refresh->Set(-MinRefresh);
  
  //-------------------------------------------
  // Hide scan result
  //-------------------------------------------
  if(Key!=kNone && scanmode==SM_NONE) showScanResult=false;
  
  //-------------------------------------------
  // Scanning: waiting for lock
  //-------------------------------------------
  
  if (scanmode!=SM_NONE && scanstatus==SS_WAITING_FOR_LOCK) {
      GetSignalInfo();
      if (fe_status & FE_HAS_LOCK) {
        if (lockstable->Elapsed()>=500) {
          Scanner=new cChannelScanner(ActuatorDevice,SChannel);
          Scanner->Start();
          scanstatus=SS_SCANNING;
          }
      } else {
        lockstable->Set();
        if (scantime->Elapsed()>=4000) 
          scanstatus=SS_NO_LOCK;
      }     
  }
  
  //-------------------------------------------
  //Scanning transponder
  //-------------------------------------------
  if(scanmode==SM_TRANSPONDER) {
      if (Key!=kNone || scanstatus==SS_NO_LOCK || (scanstatus==SS_SCANNING && !Scanner->Active())) {
        scanmode=SM_NONE;
        StopScan();
        if (scanstatus==SS_SCANNING) {
          LOCK_CHANNELS_WRITE;
          Channels->Save();
        }
      }
      Refresh();
      return state;
  }
  
  
  //-------------------------------------------
  //Scanning satellite
  //-------------------------------------------
  if(scanmode==SM_SATELLITE) {
      if (scanstatus==SS_NO_LOCK || (scanstatus==SS_SCANNING && !Scanner->Active())) {
        StopScan();
        curtransponder=Transponders->Next(curtransponder);
        if(curtransponder) {
          transponderindex++;
          menuvalue[MI_FREQUENCY]=curtransponder->Frequency();
          menuvalue[MI_SYMBOLRATE]=curtransponder->Srate();
          menuvalue[MI_SYSTEM]=curtransponder->System();
          menuvalue[MI_MODULATION]=curtransponder->Modulation();
          menuvalue[MI_VPID]=0;  //FIXME
          menuvalue[MI_APID]=0;  //FIXME
          Pol=curtransponder->Polarization();
          StartScan(false);
        } else {
          scanmode=SM_NONE;
          transponderindex=1;
          curtransponder=Transponders->First();
          LOCK_CHANNELS_WRITE;
          Channels->Save();
        }
      }
      if (Key!=kNone) {
        StopScan();
        scanmode=SM_NONE;
        LOCK_CHANNELS_WRITE;
        Channels->Save();
      }
      Refresh();
      return state;     
  }      
  
  //-------------------------------------------
  //No scanning, normal key processing
  //-------------------------------------------

  if (Key!=kNone) errormessage=EM_NONE;
  switch(Key) {
    case kLeft:
    case kLeft|k_Repeat:
             switch(selected) {
                
                  case MI_FREQUENCY:
                     switch(Pol) {
                       case 'R': Pol='L'; break;            
                       case 'L': Pol='V'; break;            
                       case 'V': Pol='H'; break;
                       default : Pol='R';
                       }
                     break;
                   
                  case MI_SATPOSITION:
                    {               
                      cSource *newsource=(cSource *)curSource->Prev();
                      while(newsource && (newsource->Code() & cSource::st_Mask) != cSource::stSat) newsource=(cSource *)newsource->Prev(); 
                      if(newsource) {
                        curSource=newsource;
                        curPosition=SatPositions.Get(curSource->Position());
                        Transponders->LoadTransponders(curSource->Position());
                        menuvalue[MI_SCANSATELLITE]=Transponders->Count();
                        curtransponder=Transponders->First();
                        transponderindex=1;
                      }
                    }
                    break;
                  
                  case MI_SCANSATELLITE:
                    {
                      cTransponder *newtransponder=Transponders->Prev(curtransponder);
                      if (newtransponder) {
                        curtransponder=newtransponder;
                        transponderindex--;
                      }
                      if (curtransponder) {
                        menuvalue[MI_FREQUENCY]=curtransponder->Frequency();
                        menuvalue[MI_SYMBOLRATE]=curtransponder->Srate();
                        menuvalue[MI_SYSTEM]=curtransponder->System();
                        menuvalue[MI_MODULATION]=curtransponder->Modulation();
                        Pol=curtransponder->Polarization();
                      }
                    }
                    break;
                  
                  default:  
                    if (menucolumn>0) menucolumn--;
                      else menucolumn=itemsperrow[menuline]-1;
             } //switch(selected)  
             break;
                
    case kRight:
    case kRight|k_Repeat:
             switch(selected) {
                
                  case MI_FREQUENCY:
                     switch(Pol) {
                       case 'H': Pol='V'; break;            
                       case 'V': Pol='L'; break;            
                       case 'L': Pol='R'; break;
                       default : Pol='H';
                       }            
                     break;  
                   
                  case MI_SATPOSITION:
                    {
                      cSource *newsource=(cSource *)curSource->Next();
                      while(newsource && (newsource->Code() & cSource::st_Mask) != cSource::stSat) newsource=(cSource *)newsource->Next(); 
                      if(newsource) {
                        curSource=newsource;
                        curPosition=SatPositions.Get(curSource->Position());
                        Transponders->LoadTransponders(curSource->Position());
                        menuvalue[MI_SCANSATELLITE]=Transponders->Count();
                        curtransponder=Transponders->First();
                        transponderindex=1;
                      }
                    }
                    break;
                    
                  case MI_SCANSATELLITE:
                    {
                      cTransponder *newtransponder=Transponders->Next(curtransponder);
                      if (newtransponder) {
                        curtransponder=newtransponder;
                        transponderindex++;
                      }
                      if (curtransponder) {
                        menuvalue[MI_FREQUENCY]=curtransponder->Frequency();
                        menuvalue[MI_SYMBOLRATE]=curtransponder->Srate();
                        menuvalue[MI_SYSTEM]=curtransponder->System();
                        menuvalue[MI_MODULATION]=curtransponder->Modulation();
                        Pol=curtransponder->Polarization();
                      }
                    }
                    break;
                
                  default:
                    if (menucolumn<itemsperrow[menuline]-1) menucolumn++;
                      else menucolumn=0;
             } //switch(selected)    
             break;
    case kUp:
    case kUp|k_Repeat:
                menuline--;
                if (menuline<0 || (ScanOnly && menuline<MINROWSCANONLY)) menuline=MAXROW;
                if (menuvalue[MI_SCANSATELLITE]==0 && Selected()==MI_SCANSATELLITE) { 
                  menuline--;
                  if (menuline<0 || (ScanOnly && menuline<MINROWSCANONLY)) menuline=MAXROW;
                }  
                if (menucolumn>=itemsperrow[menuline]) menucolumn=itemsperrow[menuline]-1;
                break;
    case kDown: 
    case kDown|k_Repeat:
                menuline++;
                if (menuline>MAXROW) menuline=ScanOnly ? MINROWSCANONLY : 0;
                if (menuvalue[MI_SCANSATELLITE]==0 && Selected()==MI_SCANSATELLITE) {
                  menuline++;
                  if (menuline>MAXROW) menuline=ScanOnly ? MINROWSCANONLY : 0;
                }  
                if (menucolumn>=itemsperrow[menuline]) menucolumn=itemsperrow[menuline]-1;
                break;
    case k0 ... k9:  
                if (selected<=MAXMENUITEM) {
                        if (menudigits[selected]>0) {
                            if (digits==0) {
                                    menuvalue[selected] = Key - 11;
                                    digits++;
                            }
                            else if (digits<menudigits[selected]) {
                                    menuvalue[selected] = menuvalue[selected]*10 + Key - 11;
                                    digits++;
                            }
                        }    
                }
                break;
    case kOk:
             switch(selected) {
                case MI_DRIVEEAST:
                   if (status.position<=0 && !LimitsDisabled) errormessage=EM_ATLIMITS;
                   else Actuator->Drive(cPositioner::pdLeft);   
                   break;
                case MI_HALT:   
                   Actuator->Halt();
                   break;
                case MI_DRIVEWEST:   
                   if (status.position>=WestLimit && !LimitsDisabled) errormessage=EM_ATLIMITS;
                   else Actuator->Drive(cPositioner::pdRight);  
                   break;
                case MI_RECALC:
                   if (curPosition) {
                     if (conf==0) conf=1;
                     else {
                       Actuator->SetPos(curPosition->Position());
                       conf=0;
                     }  
                   }
                   break;
                case MI_GOTO:
                   if (LimitsDisabled || (menuvalue[MI_GOTO]>=0 && menuvalue[MI_GOTO]<=WestLimit)) {
                     Actuator->InternalGotoPosition(menuvalue[MI_GOTO]);
                   } else errormessage=EM_OUTSIDELIMITS;
                   break;
                case MI_STORE:
                   if (conf==0) conf=1;
                   else {
                     if (curPosition) curPosition->SetPosition(status.position);
                     else {
                       curPosition=new cSatPosition(curSource->Code(),status.position);
                       SatPositions.Add(curPosition);
                     }
                     SatPositions.Save();
                     conf=0;
                   }
                   break;
                case MI_STEPSEAST:
                   Actuator->Step(cPositioner::pdLeft,menuvalue[MI_STEPSEAST]);
                   break;
                case MI_STEPSWEST:
                   Actuator->Step(cPositioner::pdRight,menuvalue[MI_STEPSWEST]); 
                   break;
                case MI_ENABLEDISABLELIMITS:
                   LimitsDisabled=!LimitsDisabled;
                   break;
                case MI_SETEASTLIMIT:
                   if (conf==0) conf=1;
                   else {
                     WestLimit-=status.position;
                     theplugin->SetupStore("WestLimit",WestLimit);
                     SatPositions.Recalc(-status.position);
                     Actuator->SetPos(0);
                     conf=0;
                   }  
                   break;
                case MI_SETZERO:
                   if (conf==0) conf=1;
                   else {
                     Actuator->SetPos(0);
                     conf=0;
                   }  
                   break;
                case MI_SETWESTLIMIT:
                   if (conf==0) conf=1;
                   else {
                     if (status.position>=0) {
                       WestLimit=status.position;
                       theplugin->SetupStore("WestLimit", WestLimit);
                     }
                     conf=0;
                   }  
                   break;
                case MI_FREQUENCY:
                case MI_SYMBOLRATE:
                case MI_VPID:
                case MI_APID:
                   Tune(); 
                   break;
                case MI_SYSTEM:
                  if (menuvalue[MI_SYSTEM]==SYS_DVBS)
                     menuvalue[MI_SYSTEM]=SYS_DVBS2;
                  else
                     menuvalue[MI_SYSTEM]=SYS_DVBS;
                  break;    
                case MI_MODULATION:
                  switch (menuvalue[MI_MODULATION]) {
                        case QPSK:
                          menuvalue[MI_MODULATION]=PSK_8;
                          break;
                        case PSK_8:
                          menuvalue[MI_MODULATION]=APSK_16;
                          break;
                        case APSK_16:
                          menuvalue[MI_MODULATION]=APSK_32;
                          break;
                        case APSK_32:
                          menuvalue[MI_MODULATION]=DQPSK;
                          break;
                        default:
                          menuvalue[MI_MODULATION]=QPSK;  
                  }
                  break;       
                case MI_SCANTRANSPONDER:
                   if (!ScanOnly && (!curPosition || curPosition->Position() != status.target || abs(status.target-status.position)>POSTOLERANCE)) {
                     errormessage=EM_NOTPOSITIONED;
                     break;
                   }     
                   if (conf==0) conf=1;
                   else {
                     scanmode=SM_TRANSPONDER;
                     TotalFound=0;
                     NewFound=0;
                     StartScan();
                     conf=0;
                   }
                   break;
                case MI_SCANSATELLITE:
                   if(curtransponder) { 
                     if (!ScanOnly && (!curPosition || curPosition->Position() != status.target || abs(status.target-status.position)>POSTOLERANCE)) {
                       errormessage=EM_NOTPOSITIONED;
                       break;
                     }     
                     if (conf==0) conf=1;
                     else {
                       menuvalue[MI_FREQUENCY]=curtransponder->Frequency();
                       menuvalue[MI_SYMBOLRATE]=curtransponder->Srate();
                       menuvalue[MI_SYSTEM]=curtransponder->System();
                       menuvalue[MI_MODULATION]=curtransponder->Modulation();
                       menuvalue[MI_VPID]=0;  //FIXME
                       menuvalue[MI_APID]=0;  //FIXME
                       Pol=curtransponder->Polarization();
                       scanmode=SM_SATELLITE;
                       TotalFound=0;
                       NewFound=0;
                       StartScan();
                       conf=0;
                     }
                   }  
                   break;
                 case MI_SATPOSITION:
                   if (curPosition) {
                     newpos=curPosition->Position();
                     if (LimitsDisabled || (newpos>=0 && newpos<=WestLimit)) {
                       Actuator->InternalGotoPosition(newpos);
                     } else errormessage=EM_OUTSIDELIMITS; 
                   } 
                   break;
                case MI_MARK:
                   if (conf==0) conf=1;
                   else {
                     MarkChannels();
                     conf=0;
                   }  
                   break;
                case MI_UNMARK:
                   if (conf==0) conf=1;
                   else {
                     UnmarkChannels();
                     conf=0;
                   }  
                   break;
                case MI_DELETE:
                   if (conf==0) conf=1;
                   else {
                     DeleteMarkedChannels();
                     conf=0;
                   }  
                   break;
             } //switch(selected)
             digits=0;
             repeat=false;
             break;
    case kOk|k_Release:
                if ((repeat) && ((selected==MI_DRIVEEAST) || (selected==MI_DRIVEWEST)))
                  Actuator->Halt();
                repeat=false;
                break;
    case kOk|k_Repeat: 
                repeat=true;
                break;
    case kRed:
                HideOsd=true;
                Refresh();
                return state;            
    case kBack: 
                if (conf==1)
                  conf=0;
                else
                return osEnd;
    default:
                Refresh();
                return state;
    }
  if ((oldcolumn!=menucolumn) || (menuline!=oldline)) {
    digits=0;
    conf=0;
  }
  Refresh();
  return state;
}

#define OSDWIDTH 600
#define CORNER 10

void cMainMenuActuator::Show(void)
{
  osd = NULL;
  if (ActuatorDevice == NULL) return;
  osd = cOsdProvider::NewOsd(Setup.OSDLeft, Setup.OSDTop);
  if (osd) {
    int rowheight=textfont->Height();
    int maxrowmenu = 19;
    if (ScanOnly)
     maxrowmenu = maxrowmenu - MINROWSCANONLY;
    tArea Area[] = 
      {{ 0, 0,           Setup.OSDWidth-1, rowheight*6-1,  4},  //rows 0..5  signal info
       { 0, rowheight*7, Setup.OSDWidth-1, rowheight*maxrowmenu-1, 4},  //rows 7..18 menu
       { 0, rowheight*maxrowmenu,Setup.OSDWidth-1, rowheight*maxrowmenu+rowheight-1, 4}}; //row  19    prompt/error
    //only the top area if we're just showing the position   
    osd->SetAreas(Area, PositionDisplay ? 1 : 3);
  } else esyslog("Darn! Couldn't create osd");
  Refresh();  
}

void cMainMenuActuator::DisplayOsd(void)
{
      int ShowSNR = fe_snr/655;
      int ShowSS =  fe_ss/655;
      int rowheight=textfont->Height();
      
#define BARWIDTH(x)  (Setup.OSDWidth * x / 100)
#define REDLIMIT 33
#define YELLOWLIMIT 66

#define TopCorners    osd->DrawEllipse(0, y, CORNER, y+CORNER, clrTransparent, -2); \
                      osd->DrawEllipse((Setup.OSDWidth-CORNER),y,Setup.OSDWidth, y+CORNER, clrTransparent, -1); 

#define BottomCorners  osd->DrawEllipse(0, y-CORNER, CORNER, y, clrTransparent, -3); \
                       osd->DrawEllipse((Setup.OSDWidth-CORNER),y-CORNER, Setup.OSDWidth, y, clrTransparent, -4); 

      if(osd)
      {
         //osd momentarily hidden
         if (HideOsd) {
           osd->DrawRectangle(0,0,Setup.OSDWidth,Setup.OSDHeight,clrTransparent);
           osd->Flush();
           return;
         }
         //background of the top area
         osd->DrawRectangle(0,0,Setup.OSDWidth,rowheight*6-1,clrBackground);
         //background of the main menu area
         if (!PositionDisplay)
           osd->DrawRectangle(0,rowheight*7,Setup.OSDWidth,rowheight*20 - (ScanOnly ? MINROWSCANONLY: 0)-1,clrNormalBg);
         char buf[512];
         char buf2[512];
         
         int y=0;
         
         tColor text, background;
         if (ScanOnly) {
           if (showScanResult) snprintf(buf,sizeof(buf),tr(channelsfound), TotalFound, NewFound);
           else buf[0]=0;
           background=clrHeaderBg;
           text=clrHeaderText;
         } else { 
           actuator_status status;
           Actuator->Status(&status);
           switch(status.state) {
             case ACM_STOPPED:
             case ACM_CHANGE:
               snprintf(buf,sizeof(buf), tr(dishwait));
               background=clrHeaderBg;
               text=clrHeaderText;
               break;
             case ACM_REACHED:
               snprintf(buf,sizeof(buf), tr(dishreached));
               background=clrHeaderBg;
               text=clrHeaderText;
               break;
             case ACM_ERROR:
               snprintf(buf,sizeof(buf), tr(disherror));
               background=clrHeaderBgError;
               text=clrHeaderTextError;
               break;
            default:
               if (showScanResult) snprintf(buf,sizeof(buf),tr(channelsfound), TotalFound, NewFound);
               else snprintf(buf,sizeof(buf),tr(dishmoving),status.target, status.position);
               background=clrHeaderBg;
               text=clrHeaderText;
           }
         }      
         osd->DrawRectangle(0,y,CORNER-1,y+rowheight-1,background);  
         snprintf(buf2,sizeof(buf2),"%s %s", tr(MAINMENUENTRY), buf);
         osd->DrawText(CORNER,y,buf2,text,background,textfont,Setup.OSDWidth,y+rowheight-1,taLeft);
         TopCorners;
         
         y+=rowheight;
         if (ShowSS > 0) {
           ShowSS=BARWIDTH(ShowSS);
           osd->DrawRectangle(0, y+3, min(BARWIDTH(REDLIMIT),ShowSS), y+rowheight-3, clrRedBar);
           if (ShowSS > BARWIDTH(REDLIMIT)) osd->DrawRectangle(BARWIDTH(REDLIMIT), y+3, min(BARWIDTH(YELLOWLIMIT),ShowSS), y+rowheight-3, clrYellowBar);
           if (ShowSS > BARWIDTH(YELLOWLIMIT)) osd->DrawRectangle(BARWIDTH(YELLOWLIMIT), y+3, ShowSS, y+rowheight-3, clrGreenBar);
         }
         y+=rowheight;
         if (ShowSNR > 0) {
           ShowSNR=BARWIDTH(ShowSNR);
           osd->DrawRectangle(0, y+3, min(BARWIDTH(REDLIMIT),ShowSNR), y+rowheight-3, clrRedBar);
           if (ShowSNR > BARWIDTH(REDLIMIT)) osd->DrawRectangle(BARWIDTH(REDLIMIT), y+3, min(BARWIDTH(YELLOWLIMIT),ShowSNR), y+rowheight-3, clrYellowBar);
           if (ShowSNR > BARWIDTH(YELLOWLIMIT)) osd->DrawRectangle(BARWIDTH(YELLOWLIMIT), y+3, ShowSNR, y+rowheight-3, clrGreenBar);
         }
         
#define OSDSTATUSWIN_X(col)      ((col == 7) ? 475 : (col == 6) ? 410 : (col == 5) ? 275 : (col == 4) ? 220 : (col == 3) ? 125 : (col == 2) ? 70 : 15)
#define OSDSTATUSWIN_XC(col,txt) (((col - 1) * Setup.OSDWidth / 5) + ((Setup.OSDWidth / 5 - textfont->Width(txt)) / 2))

         y+=rowheight;
         osd->DrawText(OSDSTATUSWIN_X(1), y, "STR:", clrStatusText, clrBackground, textfont);
         snprintf(buf, sizeof(buf), "%04x", fe_ss);
         osd->DrawText(OSDSTATUSWIN_X(2), y, buf, clrStatusText, clrBackground, textfont);
         snprintf(buf, sizeof(buf), "(%2d%%)", fe_ss / 655);
         osd->DrawText(OSDSTATUSWIN_X(3), y, buf, clrStatusText, clrBackground, textfont);
         osd->DrawText(OSDSTATUSWIN_X(4), y, "BER:", clrStatusText, clrBackground, textfont);
         snprintf(buf, sizeof(buf), "%08x", fe_ber);
         osd->DrawText(OSDSTATUSWIN_X(5), y, buf, clrStatusText, clrBackground, textfont);
         
         y+=rowheight;
         osd->DrawText(OSDSTATUSWIN_X(1), y, "SNR:", clrStatusText, clrBackground, textfont);
         snprintf(buf, sizeof(buf), "%04x", fe_snr);
         osd->DrawText(OSDSTATUSWIN_X(2), y, buf, clrStatusText, clrBackground, textfont);
         snprintf(buf, sizeof(buf), "(%2d%%)", fe_snr / 655);
         osd->DrawText(OSDSTATUSWIN_X(3), y, buf, clrStatusText, clrBackground, textfont);
         osd->DrawText(OSDSTATUSWIN_X(4), y, "UNC:", clrStatusText, clrBackground, textfont);
         snprintf(buf, sizeof(buf), "%08x", fe_unc);
         osd->DrawText(OSDSTATUSWIN_X(5), y, buf, clrStatusText, clrBackground, textfont);
         
         y+=rowheight;
         osd->DrawText(OSDSTATUSWIN_XC(1,"LOCK"),    y, "LOCK",    (fe_status & FE_HAS_LOCK)    ? clrSignalOk : clrSignalNo, clrBackground, textfont);
         osd->DrawText(OSDSTATUSWIN_XC(2,"SIGNAL"),  y, "SIGNAL",  (fe_status & FE_HAS_SIGNAL)  ? clrSignalOk : clrSignalNo, clrBackground, textfont);
         osd->DrawText(OSDSTATUSWIN_XC(3,"CARRIER"), y, "CARRIER", (fe_status & FE_HAS_CARRIER) ? clrSignalOk : clrSignalNo, clrBackground, textfont);
         osd->DrawText(OSDSTATUSWIN_XC(4,"VITERBI"), y, "VITERBI", (fe_status & FE_HAS_VITERBI) ? clrSignalOk : clrSignalNo, clrBackground, textfont);
         osd->DrawText(OSDSTATUSWIN_XC(5,"SYNC"),    y, "SYNC",    (fe_status & FE_HAS_SYNC)    ? clrSignalOk : clrSignalNo, clrBackground, textfont);
         
         y+=rowheight;
         BottomCorners;

         //end after drawing the top area if we're just showing the position
         if (PositionDisplay)  {
           osd->Flush();
           return;
         }
         
         int left=CORNER;
         int pagewidth=Setup.OSDWidth-left*2;
         int colspace=6;
         int colwidth=(pagewidth+colspace)/3;
         int curcol=0;
         int currow=ScanOnly ? MINROWSCANONLY : 0;
         
         //enter second display area
         y+=rowheight;
         TopCorners;
         
         int selected=Selected();
           
         for (int itemindex=ScanOnly ? MINITEMSCANONLY : 0; itemindex<=MAXMENUITEM; itemindex++) {
           int ipr=itemsperrow[currow];
           int x=left+curcol*(pagewidth/ipr);
           if (curcol>0) x+=colspace/2;
           int curwidth=(pagewidth-(ipr-1)*colspace)/ipr;
           if (itemindex==selected) {
             background=clrSelectedBg;
             text=clrSelectedText;
           } else {
             background=clrNormalBg;
             text=clrNormalText;
           }  
           switch(itemindex) {
             case MI_ENABLEDISABLELIMITS:
               snprintf(buf,sizeof(buf), LimitsDisabled ? tr(ENABLELIMITS) : tr(DISABLELIMITS));  
               break;
             case MI_FREQUENCY:
               curwidth-=colwidth;
               snprintf(buf, sizeof(buf),"%d%c ", menuvalue[itemindex], Pol);
               osd->DrawText(x+curwidth,y,buf,text,background,textfont,colwidth,rowheight,taRight);
               snprintf(buf, sizeof(buf), "%s", tr(menucaption[itemindex]));
               break;
             case MI_SYMBOLRATE:
               snprintf(buf, sizeof(buf),"%s %d", tr(menucaption[itemindex]), menuvalue[itemindex]);
               break;  
             case MI_SYSTEM:
               snprintf(buf, sizeof(buf),"%s %s", tr(menucaption[itemindex]), MapToUserString(menuvalue[itemindex], SystemValuesSat));
               break;
             case MI_MODULATION:
               snprintf(buf, sizeof(buf),"%s %s ", tr(menucaption[itemindex]), MapToUserString(menuvalue[itemindex], ModulationValues));
               break;  
             case MI_VPID:
             case MI_APID:
               curwidth-=colwidth;
               snprintf(buf, sizeof(buf),"%d ", menuvalue[itemindex]);
               osd->DrawText(x+curwidth,y,buf,text,background,textfont,colwidth,rowheight,taRight);
               snprintf(buf, sizeof(buf), "%s", tr(menucaption[itemindex]));
               break;
             case MI_SATPOSITION:
               if (!ScanOnly) {
                 curwidth-=colwidth;
                 if (curPosition) {
                   snprintf(buf, sizeof(buf),"%d",curPosition->Position());
                   osd->DrawText(x+curwidth,y,buf,text,background,textfont,colwidth,rowheight,taRight);                 
                 } else osd->DrawText(x+curwidth,y,tr(dishnopos),text,background,textfont,colwidth,rowheight,taRight);
               }  
               snprintf(buf,sizeof(buf), "%s %s",*cSource::ToString(curSource->Code()),curSource->Description()); 
               break;  
             case MI_SCANSATELLITE:
               if(menuvalue[MI_SCANSATELLITE]==0) { //hide the option
                 background=clrBackground;
                 text=clrBackground;
               }
               curwidth-=colwidth;
               snprintf(buf, sizeof(buf),"(%d/%d)", transponderindex,menuvalue[MI_SCANSATELLITE]);
               osd->DrawText(x+curwidth,y,buf,text,background,textfont,colwidth,rowheight,taRight);
               snprintf(buf, sizeof(buf),"%s",tr(menucaption[itemindex]));
               break;
             default:
               snprintf(buf,sizeof(buf),tr(menucaption[itemindex]),menuvalue[itemindex]);
           }
           osd->DrawText(x,y,buf,text,background,textfont,curwidth,rowheight,
               curcol==0 ? taDefault : (curcol==1 && ipr>2 ? taCenter : taRight));
           curcol++;
           if (curcol>=itemsperrow[currow]) {
             currow++;
             y+=rowheight;
             curcol=0;
           }
         }
         
        left=0; 
        if (conf==1) {
          snprintf(buf,sizeof(buf),"%s - %s", tr(menucaption[selected]), tr("Are you sure?")); 
          osd->DrawText(left,y,buf,clrMessageText,clrMessageBg,textfont,Setup.OSDWidth-left-1,rowheight,taCenter);
        } else {
          switch(errormessage) {
            case EM_OUTSIDELIMITS:
              snprintf(buf, sizeof(buf),"%s (0..%d)", tr(outsidelimits), WestLimit);
              osd->DrawText(left,y,buf,clrMessageText,clrMessageBg,textfont,Setup.OSDWidth-left-1,rowheight,taCenter);
              break;
            case EM_ATLIMITS:  
              snprintf(buf, sizeof(buf), "%s (0..%d)", tr(atlimits), WestLimit);
              osd->DrawText(left,y,buf,clrMessageText,clrMessageBg,textfont,Setup.OSDWidth-left-1,rowheight,taCenter);
              break;
            case EM_NOTPOSITIONED:
              osd->DrawText(left,y,tr(notpositioned),clrMessageText,clrMessageBg,textfont,Setup.OSDWidth-left-1,rowheight,taCenter);
              break;
            default:  
              osd->DrawRectangle(left,y,Setup.OSDWidth,y+rowheight-1,clrBackground);
              if(scanmode!=SM_NONE) {
                int barwidth;
                if (scanmode==SM_TRANSPONDER) barwidth=Setup.OSDWidth*scantime->Elapsed()/10000;
                else barwidth=Setup.OSDWidth*(10000*(transponderindex-1)+scantime->Elapsed())/10000/menuvalue[MI_SCANSATELLITE];
                osd->DrawRectangle(left,y,barwidth,y+rowheight-1,clrProgressBar);
                osd->DrawText(left,y,tr(scanning),clrProgressText,clrTransparent,textfont,Setup.OSDWidth-left-1,rowheight,taCenter);
              }
          }  
        }
        y+=rowheight;
        BottomCorners;
        osd->Flush();
      }
}

void cMainMenuActuator::GetSignalInfo(void)
{
      if (fd_frontend<=0)
      {
        fe_status=(fe_status_t)0;
        fe_ber=0;
        fe_ss=0;
        fe_snr=0;
        fe_unc=0;
        return;
      }  
      fe_status = fe_status_t(0);
      usleep(15);
      CHECK(ioctl(fd_frontend, FE_READ_STATUS, &fe_status));
      usleep(15);                                                 //µsleep necessary else returned values may be junk
      fe_ber=0;                                    //set variables to zero before ioctl, else FE_call sometimes returns junk
      CHECK(ioctl(fd_frontend, FE_READ_BER, &fe_ber));
      usleep(15);
      fe_ss=0;
      CHECK(ioctl(fd_frontend, FE_READ_SIGNAL_STRENGTH, &fe_ss));
      usleep(15);
      fe_snr=0;
      CHECK(ioctl(fd_frontend, FE_READ_SNR, &fe_snr));
      usleep(15);
      fe_unc=0;
      CHECK(ioctl(fd_frontend, FE_READ_UNCORRECTED_BLOCKS, &fe_unc));
}

void cMainMenuActuator::Tune(bool live)
{
      int Apids[MAXAPIDS + 1] = { 0 };
      int Atypes[MAXAPIDS + 1] = { 0 };
      int Dpids[MAXDPIDS + 1] = { 0 };
      int Dtypes[MAXDPIDS + 1] = { 0 };
      int Spids[MAXSPIDS + 1] = { 0 };
      char ALangs[MAXAPIDS+1][MAXLANGCODE2]={ "" };
      char DLangs[MAXDPIDS+1][MAXLANGCODE2]={ "" };
      char SLangs[MAXSPIDS+1][MAXLANGCODE2] = { "" };
      Apids[0]=menuvalue[MI_APID];
      Atypes[0]=3; //Default audio
      SChannel->SetPids(menuvalue[MI_VPID], //Vpid
                         0, //Ppid
                         2, //Vtype - default
                         Apids,Atypes,ALangs,Dpids,Dtypes,DLangs,Spids, SLangs, 
                         0); //Tpid
      cDvbTransponderParameters dtp;
      dtp.SetPolarization(Pol);
      dtp.SetModulation(menuvalue[MI_MODULATION]);
      dtp.SetSystem(menuvalue[MI_SYSTEM]);
      dtp.SetRollOff(ROLLOFF_AUTO);  
      SChannel->cChannel::SetTransponderData(curSource->Code(),menuvalue[MI_FREQUENCY],menuvalue[MI_SYMBOLRATE],dtp.ToString('S'));
      if (ActuatorDevice==cDevice::ActualDevice()) HasSwitched=true;
      if (HasSwitched && live) {
        if (cDevice::GetDevice(SChannel,0,true)==ActuatorDevice) {
          cDevice::PrimaryDevice()->SwitchChannel(SChannel, HasSwitched);
          return;
        }  
      }  
      ActuatorDevice->SwitchChannel(SChannel, HasSwitched);
}


void cMainMenuActuator::StartScan(bool live)
{
      printf("**** Scanning %d%c\n",menuvalue[MI_FREQUENCY], Pol);
      showScanResult=true;
      Tune(live);
      scantime->Set();
      lockstable->Set();
      scanstatus=SS_WAITING_FOR_LOCK;
}

void cMainMenuActuator::StopScan(void)
{
     if (scanstatus==SS_SCANNING) {
       printf("     Scanned in %lld ms, found %d channels (new %d)\n", scantime->Elapsed(), Scanner->TotalFound(), Scanner->NewFound());
       TotalFound+=Scanner->TotalFound();
       NewFound+=Scanner->NewFound();
       delete Scanner;
     }
}

int cMainMenuActuator::Selected(void)
{
     int s=menucolumn;
     for (int i=0; i<menuline; i++) s+=itemsperrow[i];
     //printf("selected %d\n", s);
     return s;
}

void cMainMenuActuator::MarkChannels(void)
{
      char buffer[1024];
      buffer[0]='+';
      LOCK_CHANNELS_WRITE;
      for (cChannel* ch=Channels->First(); ch ; ch = Channels->Next(ch)) {
        if(ch->Source()==curSource->Code()) {
          strncpy(buffer+1,ch->Name(),1023);
          if(buffer[1]!='+') ch->SetName((const char *)buffer, ch->ShortName(), ch->Provider());
        }          
      }
      //Channels.Unlock();
      Channels->Save();
}

void cMainMenuActuator::UnmarkChannels(void)
{
      char buffer[1024];
      LOCK_CHANNELS_WRITE;
      for (cChannel* ch=Channels->First(); ch ; ch = Channels->Next(ch)) {
        if(ch->Name() && (ch->Name()[0]=='+'||ch->Name()[0]=='·')) {
          strncpy(buffer,ch->Name()+1,1023);
          ch->SetName((const char *)buffer, ch->ShortName(), ch->Provider());
        }          
      }
      //Channels.Unlock();
      Channels->Save();
}

void cMainMenuActuator::DeleteMarkedChannels(void)
{
      LOCK_CHANNELS_WRITE;
      cChannel* ch=Channels->First();
      while(ch) {
         cChannel* cnext=Channels->Next(ch);
         if (ch->Name() && ch->Name()[0]=='+') { 
           if (OldChannel && OldChannel==ch) OldChannel=NULL;
           Channels->Del(ch);
         }  
         ch=cnext;
      }
      //Channels.Unlock();
      Channels->ReNumber();
      Channels->Save();
}

// --- cPluginActuator ---------------------------------------------------------

class cPluginActuator : public cPlugin {
private:
  // Add any member variables or functions you may need here.
public:
  cPluginActuator(void);
  virtual ~cPluginActuator();
  virtual const char *Version(void) { return VERSION; }
  virtual const char *Description(void) { return tr(DESCRIPTION); }
  virtual const char *CommandLineHelp(void);
  virtual bool ProcessArgs(int argc, char *argv[]);
  virtual bool Initialize(void);
  virtual bool Start(void);
  virtual void Stop(void);
  virtual void Housekeeping(void);
  virtual void MainThreadHook(void);
  virtual const char *MainMenuEntry(void) { return tr(MAINMENUENTRY); }
  virtual cOsdObject *MainMenuAction(void);
  virtual cMenuSetupPage *SetupMenu(void);
  virtual bool SetupParse(const char *Name, const char *Value);
  virtual const char **SVDRPHelpPages(void);
  virtual cString SVDRPCommand(const char *Command, const char *Option, int &ReplyCode);
};

cPluginActuator::cPluginActuator(void)
{
  // Initialize any member variables here.
  // DON'T DO ANYTHING ELSE THAT MAY HAVE SIDE EFFECTS, REQUIRE GLOBAL
  // VDR OBJECTS TO EXIST OR PRODUCE ANY OUTPUT!
  Actuator = NULL;
  cThemes::Save("actuator", &Theme);
}

cPluginActuator::~cPluginActuator()
{
  // Clean up after yourself!
}

void cPluginActuator::Stop(void)
{
  if (!ScanOnly) {
    if (Actuator) delete Actuator;
  }  
}

void cPluginActuator::MainThreadHook(void)
{
  if (ScanOnly)
    return;
  //wait if it's not time to show the position
  if (!ShowChanges || !DishIsMoving || Skins.IsOpen() || cOsd::IsOpen()) {
    PosDelay.Set();
    return;
  }  
  //after the delay has passed try to open the display
  if (PosDelay.Elapsed()>=500) if (cRemote::CallPlugin("actuator")) {
     //it succeded, tell the main menu to show only the position
     PositionDisplay=true;
  }   
}

const char *cPluginActuator::CommandLineHelp(void)
{
  // Return a string that describes all known command line options.
  return "  -s,  --scanonly  use the plugin as a channel scanner, not as a positioner\n";
}

bool cPluginActuator::ProcessArgs(int argc, char *argv[])
{
  static struct option long_options[] = {
    { "scanonly", no_argument, NULL, 's' },
    {NULL}
  };
  
  int c;
  while ((c = getopt_long(argc, argv, "s", long_options, NULL)) != -1) {
    switch (c) {
      case 's' : ScanOnly = true;
                 break;
      default  : ;           
    }
  }  
  return true;
}

bool cPluginActuator::Initialize(void)
{
  // Initialize any background activities the plugin shall perform.
  if (!ScanOnly) {
    Actuator = new cActuator;
    if (!Actuator->Opened())
      return false;
  }  
  return true;
}

bool cPluginActuator::Start(void)
{
  // Start any background activities the plugin shall perform.
  SatPositions.Load(AddDirectory(ConfigDirectory(), "actuator.conf"));
  return true;
}

void cPluginActuator::Housekeeping(void)
{
  // Perform any cleanup or other regular tasks.
}

cOsdObject *cPluginActuator::MainMenuAction(void)
{
  // Perform the action when selected from the main VDR menu.
  theplugin=this;
  return new cMainMenuActuator;
}

cMenuSetupPage *cPluginActuator::SetupMenu(void)
{
  // Return a setup menu in case the plugin supports one.
  return new cMenuSetupActuator;
}

bool cPluginActuator::SetupParse(const char *Name, const char *Value)
{
  cThemes::Save("actuator", &Theme);
  // Parse your own setup parameters and store their values.
  if      (!strcasecmp(Name, "MinRefresh"))     MinRefresh = atoi(Value);
  else if (!strcasecmp(Name, "WestLimit"))      WestLimit = atoi(Value);
  else if (!strcasecmp(Name, "ShowChanges"))    ShowChanges = atoi(Value);
  else if (!strcasecmp(Name, "Theme"))          cThemes::Load("actuator",Value, &Theme);
  else
    return false;
  return true;
}

const char **cPluginActuator::SVDRPHelpPages(void)
{
  static const char *HelpPages[] = {
    "STATUS\n"
    "    Prints the current status of the positioner.",
    NULL
    };
  if (ScanOnly)
    return NULL;  
  return HelpPages;  
}

cString cPluginActuator::SVDRPCommand(const char *Command, const char *Option, int &ReplyCode)
{
  if (!ScanOnly && strcasecmp(Command, "STATUS") == 0) {
    actuator_status status;
    char buf[500];
    const char *states[] = {
     "IDLE",
     "WEST",
     "EAST",
     "REACHED",
     "STOPPED",
     "CHANGE",
     "ERROR"
    }; 
    Actuator->Status(&status);
    snprintf(buf,sizeof(buf),"%d, %d, %d (%s)",status.target, status.position, status.state, (status.state >= ACM_IDLE && status.state <= ACM_ERROR) ? states[status.state] : "UNKWNOW"); 
    return buf;
  }
  return NULL;
}
  
VDRPLUGINCREATOR(cPluginActuator); // Don't touch this!

