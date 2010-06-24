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
#include <vdr/interface.h>
#include <vdr/osd.h>
#include <vdr/pat.h>
#include <math.h>
#include "filter.h"
#include "module/actuator.h"

#define DEV_DVB_FRONTEND "frontend"

static const char *VERSION        = "1.1.1";
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
                   MI_SYMBOLRATE,
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
                                 1,
                                 1,
                                 1,
                                 1,
                                 1,
                                 3
                                 };

#define MAXMENUITEM MI_DELETE
#define MAXROW 11


//Selectable buttons on the plugin menu: captions
static const char *menucaption[] = { 
                                   trNOOP("Drive East"), trNOOP("Halt"), trNOOP("Drive West"),
                                   trNOOP("Recalc"),trNOOP("Goto %d"),trNOOP("Store"),
                                   trNOOP("%d Steps East"),"*",trNOOP("%d Steps West"),
                                   trNOOP("Set East Limit"),trNOOP("Set Zero"), trNOOP("Set West Limit"),
                                   "", //Sat position
                                   trNOOP("Frequency:"),
                                   trNOOP("Symbolrate:"),
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
                                   5,
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
int DvbKarte=0;
int WestLimit=0;
unsigned int MinRefresh=250;
int ShowChanges=1;
//EastLimit is always 0

//actuator device
int fd_actuator;

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
  int position;
public:
  cSatPosition(void) {}  
  cSatPosition(int Source, int Position);
  ~cSatPosition();
  bool Parse(const char *s);
  bool Save(FILE *f);
  int Source(void) const { return source; }
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
  if (2 == sscanf(s, "%a[^ ] %d", &sourcebuf, &position)) { 
    source = cSource::FromString(sourcebuf);
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
  cSatPosition *Get(int Source);
  void Recalc(int offset);
  };  

cSatPosition *cSatPositions::Get(int Source)
{
  for (cSatPosition *p = First(); p; p = Next(p)) {
    if (p->Source() == Source)
      return p;
    }
  return NULL;
}

void cSatPositions::Recalc(int offset)
{ 
  for (cSatPosition *p = First(); p; p = Next(p)) p->SetPosition(p->Position()+offset);
  Save();
}

cSatPositions SatPositions;

//--- cPosTracker -----------------------------------------------------------
// Follows the dish when it's moving to store the position on disk
// It also saves/restore Setup.UpdateChannels while the dish is
// moving to avoid picking up updates from the wrong satellite

class cPosTracker:private cThread {
private:
  int LastPositionSaved;
  int fd_actuator;
  bool restoreupdate;
  const char *positionfilename;
  int update;
  int target;
  cMutex updatemutex;
public:
  cPosTracker(void);    
  ~cPosTracker(void);
  void Track(int NewTarget=-1);
  void SavePos(int newpos);
  void SaveUpdate(void);
  void RestoreUpdate(void);
protected:
  void Action(void);
};

cPosTracker::cPosTracker(void):cThread("Position Tracker")
{
  fd_actuator = open("/dev/actuator",0);
  positionfilename=strdup(AddDirectory(cPlugin::ConfigDirectory(), "actuator.pos"));
  restoreupdate=true;
  update=-1;
  if (fd_actuator) {
    actuator_status status;
    CHECK(ioctl(fd_actuator, AC_RSTATUS, &status));
    if (status.position==0) {
      //driver just loaded, try to get the position from the file
      FILE *f=fopen(positionfilename,"r");
      if(f) {
        int newpos;
        if (fscanf(f,"%d",&newpos)==1) { 
          isyslog("Read position %d from %s",newpos,positionfilename); 
          CHECK(ioctl(fd_actuator, AC_WPOS, &newpos));
          LastPositionSaved=newpos;
        } else esyslog("couldn't read dish position from %s",positionfilename);
        fclose(f);
      } else esyslog("Couldn't open file %s: %s",positionfilename,strerror(errno));
    }  else {
      //driver already loaded, trust it
      LastPositionSaved=status.position+1; //force a save
      SavePos(status.position);
    }  
  } else { //it should never take this branch
    esyslog("PosTracker cannot open /dev/actuator: %s", strerror(errno));
    exit(1);
  } 
}

cPosTracker::~cPosTracker(void)
{
  updatemutex.Lock();
  target=-1;
  restoreupdate=false;
  if (update>=0) { 
    Setup.UpdateChannels=update;  
    Setup.Save();
  }  
  updatemutex.Unlock();
  if (fd_actuator) {
    Cancel(3);
    CHECK(ioctl(fd_actuator, AC_MSTOP));
    sleep(1);
    actuator_status status;
    CHECK(ioctl(fd_actuator, AC_RSTATUS, &status));
    SavePos(status.position);
    close(fd_actuator);
    printf("actuator: saved dish position\n");
  }
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

void cPosTracker::Track(int NewTarget)
{
  updatemutex.Lock();
  if (restoreupdate) { 
    target=NewTarget;
    if (update==-1) update=Setup.UpdateChannels;
  }  
  Setup.UpdateChannels=0;

  updatemutex.Unlock();
  if (fd_actuator) Start(); 
}

void cPosTracker::SaveUpdate(void)
{
  updatemutex.Lock();
  restoreupdate=false;
  if (update==-1) update=Setup.UpdateChannels;
  Setup.UpdateChannels=0;
  target=-1;
  updatemutex.Unlock();
}

void cPosTracker::RestoreUpdate(void)
{
  //actually the restore will be made in Action() after Track() has set a new target
  updatemutex.Lock();
  restoreupdate=true;
  updatemutex.Unlock();
}



void cPosTracker::Action(void)
{
  actuator_status status;
  while(Running()) {
    CHECK(ioctl(fd_actuator, AC_RSTATUS, &status));
    SavePos(status.position);
    if (status.state==ACM_IDLE) {
      updatemutex.Lock();
      if (update!=-1 and target!=-1 and abs(status.position-target)<POSTOLERANCE) {
        Setup.UpdateChannels=update;
        update=-1;
        target=-1;
      }
      updatemutex.Unlock();
      DishIsMoving=false;
      return;
    } else if (status.state==ACM_WEST || status.state==ACM_EAST) DishIsMoving=true;  
    usleep(50000);
  }  
}

cPosTracker *PosTracker;

// --- cStatusMonitor -------------------------------------------------------

class cStatusMonitor:public cStatus {
private:
  unsigned int last_state_shown;
  int last_position_shown;
  bool transfer;
protected:
  virtual void ChannelSwitch(const cDevice *Device, int ChannelNumber);
public:
  cStatusMonitor();
};

cStatusMonitor::cStatusMonitor()
{
  last_state_shown=ACM_IDLE;
  transfer=false;
}

void cStatusMonitor::ChannelSwitch(const cDevice *Device, int ChannelNumber)
{
  actuator_status status;
  if (ChannelNumber) {
    //
    //vdr will call ChannelSwitch for the primary device even if it isn't
    //tuning the new channel (in transfer mode, the channel will be tuned by
    //another card, which will also call this method).  So:
    // - if the device is not the primary device it has really tuned to the
    //   channel
    // - if the primary device is also the actual device, it also
    //   tuned to the channel
    // - now the difficult part: HasProgramme() is true when the primary
    //   device is in transfer mode and it is switched. Unfortunately is also
    //   true when the card first enter transfer mode, so the "transfer" 
    //   variable is used to remember that the device is already in transfer 
    //   mode (i.e. we ignore the first call when it is entering transfer mode)
    //
    esyslog("actuator: switch to channel %d",ChannelNumber);
    if ((Device!=cDevice::PrimaryDevice()) || 
        (cDevice::ActualDevice()==cDevice::PrimaryDevice()) || 
        (cDevice::PrimaryDevice()->HasProgramme()) && transfer) {
      if (DvbKarte==Device->CardIndex()) { 
        cChannel channel = *Channels.GetByNumber(ChannelNumber);
        //dsyslog("Checking satellite");
        cSatPosition *p=SatPositions.Get(channel.Source());
        int target=-1;  //in case there's no target stop the motor and force Setup.UpdateChannels to 0
        if (p) {
          target=p->Position();
          //dsyslog("New sat position: %i",target);
          if (target>=0 && target<=WestLimit) {
            CHECK(ioctl(fd_actuator, AC_RSTATUS, &status));
            if (status.position!=target || status.target!=target)  {
              //dsyslog("Satellite changed");
              CHECK(ioctl(fd_actuator, AC_WTARGET, &target));
            }  
            PosTracker->Track(target);
          } else {
            Skins.Message(mtError, tr(outsidelimits));
            target=-1;
          }  
        } else Skins.Message(mtError, tr(dishnopos));
        if (target==-1) { 
          CHECK(ioctl(fd_actuator,AC_MSTOP));
          PosTracker->Track(target);
        }  
      }  
    }
    //here we remember if the primary device is already in transfer mode
    if (Device==cDevice::PrimaryDevice())
      transfer=cDevice::ActualDevice()!=cDevice::PrimaryDevice();
  }
}

// --- cMenuSetupActuator ------------------------------------------------------

class cMenuSetupActuator : public cMenuSetupPage {
private:
  int newDvbKarte;
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
  newDvbKarte=DvbKarte+1;
  newMinRefresh=MinRefresh;
  newShowChanges=ShowChanges;
  themes.Load("actuator");
  themeIndex=themes.GetThemeIndex(Theme.Description());
  Add(new cMenuEditIntItem( tr("Card connected with motor"), &newDvbKarte,1,cDevice::NumDevices()));
  Add(new cMenuEditIntItem( tr("Min. screen refresh time (ms)"), &newMinRefresh,10,1000));
  Add(new cMenuEditBoolItem( tr("Show dish moving"), &newShowChanges));
  if (themes.NumThemes())
  Add(new cMenuEditStraItem(tr("Setup.OSD$Theme"),&themeIndex, themes.NumThemes(), themes.Descriptions()));
}

void cMenuSetupActuator::Store(void)
{
  SetupStore("DVB-Karte", DvbKarte=newDvbKarte-1);
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
  char polarization;
  
public:
  cTransponder(void) { frequency=0; }  
  ~cTransponder();
  bool Parse(const char *s);
  int Frequency(void) const { return frequency; }
  int Srate(void) const { return srate; }
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
  if (4 != sscanf(s, "%d =%d , %c ,%d ", &dummy, &frequency, &polarization, &srate)) {
    frequency=0; //hack 
    } else {
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
  int satlong = source & cSource::st_Pos;
  if (satlong > 0x00007FFF) {
    satlong |= 0xFFFF0000;
    satlong = - satlong;
  }  else {   
    satlong = 3600 - satlong;
  }    
  char buffer[100];
  snprintf(buffer,sizeof(buffer),"transponders/%04d.ini",satlong);
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
  cDevice *ActuatorDevice;
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
  SdtFilter *SFilter;
  PatFilter *PFilter;
  enum sm {
    SM_NONE,
    SM_TRANSPONDER,
    SM_SATELLITE
    } scanmode;
  bool showScanResult;  
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
  for (int i=0; i<cDevice::NumDevices() && ActuatorDevice==NULL; i++)
    if (cDevice::GetDevice(i)->CardIndex()==DvbKarte) 
      ActuatorDevice=cDevice::GetDevice(i);
  if (ActuatorDevice==NULL) return;
  OldChannel=Channels.GetByNumber(ActuatorDevice->CurrentChannel());
  SChannel=new cChannel();
  repeat=HasSwitched=false;
  conf=0;
  errormessage=EM_NONE;
  menucolumn=menuvalue[MI_STEPSEAST]=menuvalue[MI_STEPSWEST]=1;
  menuline=1;
  osd=NULL;
  digits=0;
  LimitsDisabled=false;
  //if we're not going to interact with the actuator, don't save the value of UpdateChannels
  if (!PositionDisplay) PosTracker->SaveUpdate();
  static char buffer[PATH_MAX];
  snprintf(buffer, sizeof(buffer), "%s%d/%s%d", "/dev/dvb/adapter",DvbKarte,"frontend",0);
  fd_frontend = open(buffer,O_RDONLY);
  curSource=Sources.Get(OldChannel->Source());
  curPosition=SatPositions.Get(OldChannel->Source());
  Transponders=new cTransponders();
  Transponders->LoadTransponders(curSource->Code());
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
  menuvalue[MI_VPID]=OldChannel->Vpid();
  menuvalue[MI_APID]=OldChannel->Apid(0);
  actuator_status status;
  CHECK(ioctl(fd_actuator, AC_RSTATUS, &status));
  //fast refresh only when the dish is moving
  //(now always, to show signal information more timely)
  SetNeedsFastResponse(true); //((status.state == ACM_EAST) || (status.state == ACM_WEST));
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
  delete refresh;
  close(fd_frontend);
  //Don't change channel if we're only showing the position
  if (PositionDisplay) {
    PositionDisplay=false;
    return;
  }  
  CHECK(ioctl(fd_actuator, AC_MSTOP));
  PosTracker->RestoreUpdate();
  if (OldChannel) {
    if (HasSwitched) {
      if (cDevice::GetDevice(OldChannel,0,true)==ActuatorDevice) {
        cDevice::PrimaryDevice()->SwitchChannel(OldChannel, HasSwitched);
        return;
      }  
    }
    ActuatorDevice->SwitchChannel(OldChannel,true);
  }  
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
  if (ActuatorDevice == NULL) {
    Skins.Message(mtError,tr("Card not available"));
    return osEnd;
  }  
  int selected=Selected();
  int newpos;
  eOSState state = osContinue;
  actuator_status status;
  CHECK(ioctl(fd_actuator, AC_RSTATUS, &status));
  if ((status.position<=0 && status.state==ACM_EAST || 
       status.position>=WestLimit && status.state==ACM_WEST) && !LimitsDisabled) {
    CHECK(ioctl(fd_actuator, AC_MSTOP));
    errormessage=EM_ATLIMITS;
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
  //Scanning transponder
  //-------------------------------------------
  if(scanmode==SM_TRANSPONDER) {
      if (Key!=kNone || scantime->Elapsed()>=10000 || PFilter->EndOfScan()) {
        scanmode=SM_NONE;
        StopScan();
        Channels.Save();
      }
      Refresh();
      return state;
  }
  
  
  //-------------------------------------------
  //Scanning satellite
  //-------------------------------------------
  if(scanmode==SM_SATELLITE) {
      bool haslock=true;
      if (scantime->Elapsed()>=1000 && scantime->Elapsed()<2000) {
        GetSignalInfo();
        haslock=fe_status & FE_HAS_LOCK;
      }     
      if (!haslock || scantime->Elapsed()>=10000 || PFilter->EndOfScan()) {
        StopScan();
        curtransponder=Transponders->Next(curtransponder);
        if(curtransponder) {
          transponderindex++;
          menuvalue[MI_FREQUENCY]=curtransponder->Frequency();
          menuvalue[MI_SYMBOLRATE]=curtransponder->Srate();
          menuvalue[MI_VPID]=0;  //FIXME
          menuvalue[MI_APID]=0;  //FIXME
          Pol=curtransponder->Polarization();
          StartScan(false);
          scantime->Set();
        } else {
          scanmode=SM_NONE;
          transponderindex=1;
          curtransponder=Transponders->First();
          Channels.Save();
        }
      }
      if (Key!=kNone && scanmode!=SM_NONE) {
        StopScan();
        scanmode=SM_NONE;
        Channels.Save();
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
                        curPosition=SatPositions.Get(curSource->Code());
                        Transponders->LoadTransponders(curSource->Code());
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
                        curPosition=SatPositions.Get(curSource->Code());
                        Transponders->LoadTransponders(curSource->Code());
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
                if (menuline<0) menuline=MAXROW;
                if (menuvalue[MI_SCANSATELLITE]==0 && Selected()==MI_SCANSATELLITE) { 
                  menuline--;
                  if (menuline<0) menuline=MAXROW;
                }  
                if (menucolumn>=itemsperrow[menuline]) menucolumn=itemsperrow[menuline]-1;
                break;
    case kDown: 
    case kDown|k_Repeat:
                menuline++;
                if (menuline>MAXROW) menuline=0;
                if (menuvalue[MI_SCANSATELLITE]==0 && Selected()==MI_SCANSATELLITE) {
                  menuline++;
                  if (menuline>MAXROW) menuline=0;
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
                   else {   
                     CHECK(ioctl(fd_actuator, AC_MEAST));
                     PosTracker->Track();
                     //needsFastResponse=true;
                   }  
                   break;
                case MI_HALT:   
                   CHECK(ioctl(fd_actuator, AC_MSTOP));
                   break;
                case MI_DRIVEWEST:   
                   if (status.position>=WestLimit && !LimitsDisabled) errormessage=EM_ATLIMITS;
                   else {   
                     CHECK(ioctl(fd_actuator, AC_MWEST));
                     PosTracker->Track();
                     //needsFastResponse=true;
                   }  
                   break;
                case MI_RECALC:
                   if (curPosition) {
                     if (conf==0) conf=1;
                     else {
                       newpos=curPosition->Position();
                       CHECK(ioctl(fd_actuator, AC_WPOS, &newpos));
                       PosTracker->Track();
                       conf=0;
                     }  
                   }
                   break;
                case MI_GOTO:
                   if (LimitsDisabled || (menuvalue[MI_GOTO]>=0 && menuvalue[MI_GOTO]<=WestLimit)) {
                     CHECK(ioctl(fd_actuator, AC_WTARGET, &menuvalue[MI_GOTO]));
                     PosTracker->Track();
                     //needsFastResponse=true;
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
                case MI_STEPSWEST:
                   {
                     if (selected==MI_STEPSEAST) newpos=status.position-menuvalue[MI_STEPSEAST];
                       else newpos=status.position+menuvalue[MI_STEPSWEST];
                     CHECK(ioctl(fd_actuator, AC_WTARGET, &newpos));
                     PosTracker->Track();
                     //needsFastResponse=true;
                   }    
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
                     newpos=0;
                     CHECK(ioctl(fd_actuator, AC_WPOS, &newpos));
                     PosTracker->Track();
                     conf=0;
                   }  
                   break;
                case MI_SETZERO:
                   if (conf==0) conf=1;
                   else {
                     newpos=0;
                     CHECK(ioctl(fd_actuator, AC_WPOS, &newpos));
                     PosTracker->Track();
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
                case MI_SCANTRANSPONDER:
                   if (!curPosition || curPosition->Position() != status.target || abs(status.target-status.position)>POSTOLERANCE) {
                     errormessage=EM_NOTPOSITIONED;
                     break;
                   }     
                   if (conf==0) conf=1;
                   else {
                     scanmode=SM_TRANSPONDER;
                     StartScan();
                     conf=0;
                   }
                   break;
                case MI_SCANSATELLITE:
                   if(curtransponder) { 
                     if (!curPosition || curPosition->Position() != status.target || abs(status.target-status.position)>POSTOLERANCE) {
                       errormessage=EM_NOTPOSITIONED;
                       break;
                     }     
                     if (conf==0) conf=1;
                     else {
                       menuvalue[MI_FREQUENCY]=curtransponder->Frequency();
                       menuvalue[MI_SYMBOLRATE]=curtransponder->Srate();
                       menuvalue[MI_VPID]=0;  //FIXME
                       menuvalue[MI_APID]=0;  //FIXME
                       Pol=curtransponder->Polarization();
                       scanmode=SM_SATELLITE;
                       StartScan();
                       conf=0;
                     }
                   }  
                   break;
                 case MI_SATPOSITION:
                   if (curPosition) {
                     newpos=curPosition->Position();
                     if (LimitsDisabled || (newpos>=0 && newpos<=WestLimit)) {
                       CHECK(ioctl(fd_actuator,AC_WTARGET,&newpos));
                       PosTracker->Track();
                       //needsFastResponse=true;
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
                  CHECK(ioctl(fd_actuator, AC_MSTOP));
                repeat=false;
                break;
    case kOk|k_Repeat: 
                repeat=true;
                break;
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
    tArea Area[] = 
      {{ 0, 0,           Setup.OSDWidth-1, rowheight*6-1,  4},  //rows 0..5  signal info
       { 0, rowheight*7, Setup.OSDWidth-1, rowheight*19-1, 2},  //rows 7..18 menu
       { 0, rowheight*19,Setup.OSDWidth-1, rowheight*20-1, 4}}; //row  19    prompt/error
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
         //only the top area if we're just showing the position
         osd->DrawRectangle(0,0,Setup.OSDWidth,rowheight*(PositionDisplay ? 6 : 13)-1,clrBackground);
         char buf[512];
         char buf2[512];
         
         int y=0;
         
         actuator_status status;
         tColor text, background;
         CHECK(ioctl(fd_actuator, AC_RSTATUS, &status));
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
             if (showScanResult) snprintf(buf,sizeof(buf),tr(channelsfound), SdtFilter::ChannelsFound(), SdtFilter::NewFound());
             else snprintf(buf,sizeof(buf),tr(dishmoving),status.target, status.position);
             background=clrHeaderBg;
             text=clrHeaderText;
         }    
         osd->DrawRectangle(0,y,CORNER-1,y+rowheight-1,background);  
         snprintf(buf2,sizeof(buf2),"%s - %s", tr(MAINMENUENTRY), buf);
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
         int currow=0;
         
         y+=rowheight; //enter second display area
         TopCorners;
         
         int selected=Selected();
           
         for (int itemindex=0; itemindex<=MAXMENUITEM; itemindex++) {
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
               snprintf(buf, sizeof(buf), tr(menucaption[itemindex]));
               break;
             case MI_SYMBOLRATE:
             case MI_VPID:
             case MI_APID:
               curwidth-=colwidth;
               snprintf(buf, sizeof(buf),"%d ", menuvalue[itemindex]);
               osd->DrawText(x+curwidth,y,buf,text,background,textfont,colwidth,rowheight,taRight);
               snprintf(buf, sizeof(buf),tr(menucaption[itemindex]));
               break;
             case MI_SATPOSITION:
               curwidth-=colwidth;
               if (curPosition) {
                 snprintf(buf, sizeof(buf),"%d",curPosition->Position());
                 osd->DrawText(x+curwidth,y,buf,text,background,textfont,colwidth,rowheight,taRight);                 
               } else osd->DrawText(x+curwidth,y,tr(dishnopos),text,background,textfont,colwidth,rowheight,taRight);
               snprintf(buf,sizeof(buf), "%s %s:",*cSource::ToString(curSource->Code()),curSource->Description()); 
               break;  
             case MI_SCANSATELLITE:
               if(menuvalue[MI_SCANSATELLITE]==0) { //hide the option
                 background=clrBackground;
                 text=clrBackground;
               }
               curwidth-=colwidth;
               snprintf(buf, sizeof(buf),"(%d/%d)", transponderindex,menuvalue[MI_SCANSATELLITE]);
               osd->DrawText(x+curwidth,y,buf,text,background,textfont,colwidth,rowheight,taRight);
               snprintf(buf, sizeof(buf),tr(menucaption[itemindex]));
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
      Atypes[0]=4; //FIXME add menu option
      SChannel->SetPids(menuvalue[MI_VPID], //Vpid
                         0, //Ppid
                         2, //Vtype MPEG-2 - FIXME add menu
                         Apids,Atypes,ALangs,Dpids,Dtypes,DLangs,Spids, SLangs, 
                         0); //Tpid
      cDvbTransponderParameters dtp;
      dtp.SetPolarization(Pol);
      dtp.SetModulation(QPSK);
      dtp.SetSystem(SYS_DVBS);
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
      PFilter=new PatFilter();
      SFilter=new SdtFilter(PFilter);
      if (live) SdtFilter::ResetFound();
      PFilter->SetSdtFilter(SFilter);
      ActuatorDevice->AttachFilter(SFilter);
      ActuatorDevice->AttachFilter(PFilter);
  
}

void cMainMenuActuator::StopScan(void)
{
     printf("     Scanned in %lld ms, found %d channels (new %d)\n", scantime->Elapsed(), SdtFilter::ChannelsFound(), SdtFilter::NewFound());
     ActuatorDevice->Detach(PFilter);
     ActuatorDevice->Detach(SFilter);
     delete PFilter;
     delete SFilter;
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
      Channels.Lock(true);
      for (cChannel* ch=Channels.First(); ch ; ch = Channels.Next(ch)) {
        if(ch->Source()==curSource->Code()) {
          strncpy(buffer+1,ch->Name(),1023);
          if(buffer[1]!='+') ch->SetName((const char *)buffer, ch->ShortName(), ch->Provider());
        }          
      }
      Channels.Unlock();
      Channels.Save();
}

void cMainMenuActuator::UnmarkChannels(void)
{
      char buffer[1024];
      Channels.Lock(true);
      for (cChannel* ch=Channels.First(); ch ; ch = Channels.Next(ch)) {
        if(ch->Name() && (ch->Name()[0]=='+'||ch->Name()[0]=='·')) {
          strncpy(buffer,ch->Name()+1,1023);
          ch->SetName((const char *)buffer, ch->ShortName(), ch->Provider());
        }          
      }
      Channels.Unlock();
      Channels.Save();
}

void cMainMenuActuator::DeleteMarkedChannels(void)
{
      Channels.Lock(true);
      cChannel* ch=Channels.First();
      while(ch) {
         cChannel* cnext=Channels.Next(ch);
         if (ch->Name() && ch->Name()[0]=='+') { 
           if (OldChannel && OldChannel==ch) OldChannel=NULL;
           Channels.Del(ch);
         }  
         ch=cnext;
      }
      Channels.Unlock();
      Channels.ReNumber();
      Channels.Save();
}

// --- cPluginActuator ---------------------------------------------------------

class cPluginActuator : public cPlugin {
private:
  // Add any member variables or functions you may need here.
  cStatusMonitor *statusMonitor;
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
  statusMonitor = NULL;
  PosTracker = NULL;
  fd_actuator = open("/dev/actuator",0);
  if (fd_actuator<0) {
    esyslog("cannot open /dev/actuator");
    exit(1);
  }
  cThemes::Save("actuator", &Theme);
}

cPluginActuator::~cPluginActuator()
{
  // Clean up after yourself!
}

void cPluginActuator::Stop(void)
{
  delete statusMonitor;
  delete PosTracker;
  close(fd_actuator);
}

void cPluginActuator::MainThreadHook(void)
{
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
  return NULL;
}

bool cPluginActuator::ProcessArgs(int argc, char *argv[])
{
  // Implement command line argument processing here if applicable.
  return true;
}

bool cPluginActuator::Initialize(void)
{
  // Initialize any background activities the plugin shall perform.
  PosTracker=new cPosTracker();
  return true;
}

bool cPluginActuator::Start(void)
{
  // Start any background activities the plugin shall perform.
  SatPositions.Load(AddDirectory(ConfigDirectory(), "actuator.conf"));
  statusMonitor = new cStatusMonitor;
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
  if      (!strcasecmp(Name, "DVB-Karte"))      DvbKarte = atoi(Value);
  else if (!strcasecmp(Name, "MinRefresh"))     MinRefresh = atoi(Value);
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
  return HelpPages;  
}

cString cPluginActuator::SVDRPCommand(const char *Command, const char *Option, int &ReplyCode)
{
  if (strcasecmp(Command, "STATUS") == 0) {
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
    CHECK(ioctl(fd_actuator, AC_RSTATUS, &status));
    snprintf(buf,sizeof(buf),"%d, %d, %d (%s)",status.target, status.position, status.state, (status.state >= ACM_IDLE && status.state <= ACM_ERROR) ? states[status.state] : "UNKWNOW"); 
    return buf;
  }
  return NULL;
}
  
VDRPLUGINCREATOR(cPluginActuator); // Don't touch this!

