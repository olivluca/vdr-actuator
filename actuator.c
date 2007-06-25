/*
 * actuator.c: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id$
 */

#include <linux/dvb/frontend.h>
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
#include "i18n.h"
#include "module/actuator.h"
#define DEV_DVB_FRONTEND "frontend"

static const char *VERSION        = "0.0.3";
static const char *DESCRIPTION    = "Linear or h-h actuator control";
static const char *MAINMENUENTRY  = "Actuator";

//Selectable buttons on the plugin menu: indexes
enum menuindex {
  MI_DRIVEEAST=0,
  MI_HALT,
  MI_DRIVEWEST,
  MI_RECALC,
  MI_GOTO,
  MI_STORE,
  MI_STEPSEAST,
  MI_ENABLEDISABLELIMITS,
  MI_STEPSWEST,
  MI_SETEASTLIMIT,
  MI_SETZERO,
  MI_SETWESTLIMIT,
  MI_SATPOSITION,
  MI_FREQUENCY,
  MI_SYMBOLRATE,
  MI_SCANTRANSPONDER
  };

#define MAXMENUITEM MI_SCANTRANSPONDER


//Selectable buttons on the plugin menu: captions
static const char *menucaption[] = { 
                                   "Drive East", "Halt", "Drive West",
                                   "Recalc","Goto %d","Store",
                                   "%d Steps East","*","%d Steps West",
                                   "Set East Limit","Set Zero", "Set West Limit",
                                   "", //Sat position
                                   "Frequency:",
                                   "Symbolrate:",
                                   "Scan Transponder"
                                  };
#define ENABLELIMITS "Enable Limits"
#define DISABLELIMITS "Disable Limits"
                                  
                                  
//Rows and columns of the menu buttons                                  
#define IROW 0
#define ICOL 1
#define IWIDTH 2
static const int menurowcol[][3] = { 
                                       {0,0,1}, {0,1,1}, {0,2,1},
                                       {1,0,1}, {1,1,1}, {1,2,1},
                                       {2,0,1}, {2,1,1}, {2,2,1},
                                       {3,0,1}, {3,1,1}, {3,2,1},
                                       {4,0,3},
                                       {5,0,3},
                                       {6,0,3},
                                       {7,0,3}
                                      };                                   
                                  
//Number of allowed input digits for each menu item                                  
static const int menudigits[] = {
                                   0,0,0,
                                   0,4,0,
                                   2,0,2,
                                   0,0,0,
                                   0,
                                   5,
                                   5,
                                   0
                                   };                                  

//Dish messages
#define dishmoving    "Dish target: %d, position: %d"
#define dishreached   "Position reached"
#define dishwait      "Motor wait"
#define disherror     "Motor error"
#define dishnopos     "Position not set"
#define notpositioned "Not positioned"
#define atlimits      "Dish at limits"
#define outsidelimits "Position outside limits"

//Positioning tolerance (for transponder scanning)
#define POSTOLERANCE 5

//Plugin parameters 
int DvbKarte=0;
int WestLimit=0;
//EastLimit is always 0

//actuator device
int fd_actuator;

//how to access SetupStore inside the main menu?
cPlugin *theplugin;

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
  return fprintf(f, "%s %d\n", cSource::ToString(source), position) > 0; 
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
  bool stopit;
  bool restoreupdate;
  const char *positionfilename;
  cCondVar *trackwait;
  cMutex mutex;
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
  stopit=false;
  restoreupdate=true;
  update=-1;
  trackwait=new cCondVar();
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
        } else esyslog("couldn't read dish position from %s",positionfilename);
        fclose(f);
      } else esyslog("Couldn't open file %s: %s",positionfilename,strerror(errno));
    }  else {
      //driver already loaded, trust it
      LastPositionSaved=status.position+1; //force a save
      SavePos(status.position);
    }  
    Start();
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
    stopit=true;
    trackwait->Broadcast();
    //Cancel(5);
    sleep(5);
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
    if (update==-1) {
      update=Setup.UpdateChannels;
      Setup.UpdateChannels=0;
    }
  }  
  updatemutex.Unlock();
  if (fd_actuator) trackwait->Broadcast();
}

void cPosTracker::SaveUpdate(void)
{
  updatemutex.Lock();
  restoreupdate=false;
  if (update==-1) {
    update=Setup.UpdateChannels;
    Setup.UpdateChannels=0;
  }  
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
  while(true) {
    cMutexLock MutexLock(&mutex);
    trackwait->Wait(mutex);
    if (stopit) {
      CHECK(ioctl(fd_actuator, AC_MSTOP));
      sleep(1);
      CHECK(ioctl(fd_actuator, AC_RSTATUS, &status));
      SavePos(status.position);
      close(fd_actuator);
      return; 
    }
    while (true) {
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
        break;
      }  
      usleep(50000);
    }
  }  
}


cPosTracker *PosTracker;

// --- cStatusMonitor -------------------------------------------------------

class cStatusMonitor:public cStatus {
private:
  bool transfer;
  unsigned int last_state_shown;
  int last_position_shown;
protected:
  virtual void ChannelSwitch(const cDevice *Device, int ChannelNumber);
  virtual bool AlterDisplayChannel(cSkinDisplayChannel *displayChannel);
public:
  cStatusMonitor();
};

cStatusMonitor::cStatusMonitor()
{
  transfer=false;
  last_state_shown=ACM_IDLE;
}

void cStatusMonitor::ChannelSwitch(const cDevice *Device, int ChannelNumber)
{
  actuator_status status;
  if (ChannelNumber) {
     if ((Device!=cDevice::PrimaryDevice()) || (cDevice::ActualDevice()==cDevice::PrimaryDevice()) || (cDevice::PrimaryDevice()->cDevice::HasProgramme()) && transfer) {
     cChannel channel = *Channels.GetByNumber(ChannelNumber);
     dsyslog("Checking satellite");
     //if (DvbKarte==Device->CardIndex())
     cSatPosition *p=SatPositions.Get(channel.Source());
     int target=-1;  //in case there's no target stop the motor and force Setup.UpdateChannels to 0
     if (p) {
       target=p->Position();
       dsyslog("New sat position: %i",target);
       if (target>=0 && target<=WestLimit) {
         CHECK(ioctl(fd_actuator, AC_RSTATUS, &status));
         if (status.position!=target || status.target!=target)  {
           dsyslog("Satellite changed");
           CHECK(ioctl(fd_actuator, AC_WTARGET, &target));
         }  
       } else {
         Skins.Message(mtError, tr(outsidelimits));
         target=-1;
       }  
     } else Skins.Message(mtError, tr(dishnopos));
     if (target==-1) CHECK(ioctl(fd_actuator,AC_MSTOP));
     if (PosTracker) PosTracker->Track(target);
          
     }
       if ((cDevice::ActualDevice()!=cDevice::PrimaryDevice()) && (Device==cDevice::PrimaryDevice()) )
        transfer=true;
       if ((cDevice::ActualDevice()==cDevice::PrimaryDevice()) && (Device==cDevice::PrimaryDevice())) transfer=false;
   }
}

bool cStatusMonitor::AlterDisplayChannel(cSkinDisplayChannel *displayChannel)
{
  actuator_status status;
  char *buf=NULL;

  CHECK(ioctl(fd_actuator, AC_RSTATUS, &status));
  
  bool showit=((last_state_shown != status.state) || (last_position_shown != status.position));
  last_state_shown=status.state;
  last_position_shown=status.position;
   
  switch(status.state) {
      case ACM_IDLE: 
        return false; 
        
      case ACM_WEST:
      case ACM_EAST:
        if (showit) {
          asprintf(&buf,tr(dishmoving),status.target, status.position);
          displayChannel->SetMessage(mtInfo,buf);
          }
        return true;
        
      case ACM_REACHED:
        if (showit) displayChannel->SetMessage(mtInfo,tr(dishreached));
        return true;
        
      case ACM_STOPPED:
      case ACM_CHANGE:
        if (showit) displayChannel->SetMessage(mtInfo,tr(dishwait));
        return true;
        
      case ACM_ERROR:
        if (showit) displayChannel->SetMessage(mtError,tr(disherror));
        return false;      
  }  
  return false;

}

// --- cMenuEditIntpItem ----------------------------------------------------

class cMenuEditIntpItem : public cMenuEditIntItem {
protected:
  virtual void Set(void);
  const char *falseString, *trueString;
  int *value2;
public:
  cMenuEditIntpItem(const char *Name, int *Value, int Min = 0, int Max = INT_MAX, int *Value2=0, const char *FalseString = "", const char *TrueSting = NULL);
  virtual eOSState ProcessKey(eKeys Key);
};

void cMenuEditIntpItem::Set(void)
{
  char buf[16];
  snprintf(buf, sizeof(buf), "%d.%d %s", *value/10, *value % 10, *value2 ? trueString : falseString);
  SetValue(buf);
}

cMenuEditIntpItem::cMenuEditIntpItem(const char *Name, int *Value, int Min, int Max,int *Value2, const char *FalseString,const char *TrueString):cMenuEditIntItem(Name, Value, Min, Max)
{
  value = Value;
  value2= Value2;
  trueString = TrueString;
  falseString = FalseString;
  min = Min;
  max = Max;
  Set();
}

eOSState cMenuEditIntpItem::ProcessKey(eKeys Key)
{
  eOSState state = cMenuEditItem::ProcessKey(Key);

  if (state == osUnknown) {
     int newValue = *value;
     int newValue2= *value2;
     Key = NORMALKEY(Key);
     switch (Key) {
       case kNone: break;
       case k0 ... k9:
            if (fresh) {
               *value = 0;
               fresh = false;
               }
            newValue = *value * 10 + (Key - k0);
            break;
       case kLeft: // TODO might want to increase the delta if repeated quickly?
            newValue2 = 0;
            fresh = true;
            break;
       case kRight:
            newValue2 = 1;
            fresh = true;
            break;
       default:
            if (*value < min) { *value = min; Set(); }
            if (*value > max) { *value = max; Set(); }
            return state;
       }
     if ((!fresh || min <= newValue) && newValue <= max) {
        *value = newValue;
        *value2 = newValue2;
        Set();
        }
     state = osContinue;
     }
  return state;
}


// --- cMenuSetupActuator ------------------------------------------------------

class cMenuSetupActuator : public cMenuSetupPage {
private:
  int newDvbKarte;
protected:
  virtual void Store(void);
public:
  cMenuSetupActuator(void);
  };

cMenuSetupActuator::cMenuSetupActuator(void)
{
  newDvbKarte=DvbKarte+1;
  Add(new cMenuEditIntItem( tr("Card, connected with motor"), &newDvbKarte,1,cDevice::NumDevices()));

}

void cMenuSetupActuator::Store(void)
{
  SetupStore("DVB-Karte", DvbKarte=newDvbKarte-1);
}

// -- cMainMenuActuator --------------------------------------------------------

class cMainMenuActuator : public cOsdObject {
private:
  int digits,paint,menucolumn,menuline,conf,repeat,HasSwitched,fd_frontend;
  enum em {
    EM_NONE,
    EM_OUTSIDELIMITS,
    EM_ATLIMITS,
    EM_NOTPOSITIONED
    } errormessage;
  bool LimitsDisabled;
  cSource *curSource;
  cSatPosition *curPosition;
  int menuvalue[15];
  char *Pol;
  cChannel *OldChannel;
  cChannel Channel;
  cOsd *osd;
  tColor color;
  fe_status_t fe_status;
  unsigned int fe_ber;
  unsigned int fe_ss;
  unsigned int fe_snr;
  unsigned int fe_unc;
  float ssdBm;
  float snrdB;

public:
  cMainMenuActuator(void);
  ~cMainMenuActuator();
  virtual void Show(void);
  virtual eOSState ProcessKey(eKeys Key);
  void DisplaySignalInfoOnOsd(void);
  void GetSignalInfo(void);
  bool Signal(int Frequenz, char *Pol, int Symbolrate);
};


cMainMenuActuator::cMainMenuActuator(void)
{
  OldChannel=Channels.GetByNumber(cDevice::GetDevice(DvbKarte)->CurrentChannel());
  repeat=HasSwitched=false;
  conf=0;
  errormessage=EM_NONE;
  menucolumn=menuvalue[MI_STEPSEAST]=menuvalue[MI_STEPSWEST]=1;
  menuline=1;
  paint=0;
  osd=NULL;
  digits=0;
  LimitsDisabled=false;
  PosTracker->SaveUpdate();
  static char buffer[PATH_MAX];
  snprintf(buffer, sizeof(buffer), "%s%d/%s%d", "/dev/dvb/adapter",DvbKarte,"frontend",0);
  fd_frontend = open(buffer,0);
  cChannel *Channel=Channels.GetByNumber(cDevice::GetDevice(DvbKarte)->CurrentChannel(),0);
  curSource=Sources.Get(Channel->Source());
  curPosition=SatPositions.Get(Channel->Source());
  if (curPosition) menuvalue[MI_GOTO]=curPosition->Position();
  else menuvalue[MI_GOTO]=0;
  menuvalue[MI_FREQUENCY]=(*Channel).Frequency();
  if ((*Channel).Polarization() == 'v' || (*Channel).Polarization() == 'V')
      Pol="V";
   else Pol="H";
  menuvalue[MI_SYMBOLRATE]=(*Channel).Srate();
  actuator_status status;
  CHECK(ioctl(fd_actuator, AC_RSTATUS, &status));
  //fast refresh only when the dish is moving
  needsFastResponse=((status.state == ACM_EAST) || (status.state == ACM_WEST));
}

cMainMenuActuator::~cMainMenuActuator()
{
  delete osd;
  close(fd_frontend);
  CHECK(ioctl(fd_actuator, AC_MSTOP));
  PosTracker->RestoreUpdate();
  //if (HasSwitched) {
     cDevice::GetDevice(DvbKarte)->SwitchChannel(OldChannel,true);
  //   }
}


void cMainMenuActuator::Show(void)
{
        GetSignalInfo();
        DisplaySignalInfoOnOsd();

}

eOSState cMainMenuActuator::ProcessKey(eKeys Key)
{
  int selected;
  if (menuline<4) selected=menuline*3+menucolumn;
    else selected=menuline+8;
  int newpos;
  eOSState state = cOsdObject::ProcessKey(Key);
  actuator_status status;
  CHECK(ioctl(fd_actuator, AC_RSTATUS, &status));
  if ((status.position<=0 && status.state==ACM_EAST || 
       status.position>=WestLimit && status.state==ACM_WEST) && !LimitsDisabled) {
    CHECK(ioctl(fd_actuator, AC_MSTOP));
    errormessage=EM_ATLIMITS;
  }
  needsFastResponse=(status.state != ACM_IDLE);
  if (state == osUnknown) {
  int oldcolumn,oldline;
  oldcolumn=menucolumn;
  oldline=menuline;
  if (Key!=kNone) errormessage=EM_NONE;
  switch(Key) {
    case kLeft:
                if ((menucolumn!=0) && (menuline<4)) menucolumn--;
                if (selected==MI_FREQUENCY) Pol="V";
                if (selected==MI_SATPOSITION) 
                  if(curSource->Prev()) {
                    curSource=(cSource *)curSource->Prev();
                    curPosition=SatPositions.Get(curSource->Code());
                  }
                break;
    case kRight:
                if ((menucolumn!=2) && (menuline<4)) menucolumn++;
                if (selected==MI_FREQUENCY) Pol="H";
                if (selected==MI_SATPOSITION) 
                  if(curSource->Next()) {
                    curSource=(cSource *)curSource->Next();
                    curPosition=SatPositions.Get(curSource->Code());
                  }
                break;
    case kUp:
                if (menuline!=0) menuline--;
                break;
    case kDown: 
                if (menuline<3) menuline++;
                else if (menuline>=3 && menuline<7) {
                  menuline++;
                  menucolumn=1;
                  }
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
                     needsFastResponse=true;
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
                     needsFastResponse=true;
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
                     needsFastResponse=true;
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
                     needsFastResponse=true;
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
                   Signal(menuvalue[MI_FREQUENCY],Pol,menuvalue[MI_SYMBOLRATE]); 
                   break;
                case MI_SCANTRANSPONDER:
                   if (!curPosition || curPosition->Position() != status.target || abs(status.target-status.position)>POSTOLERANCE) {
                     errormessage=EM_NOTPOSITIONED;
                     break;
                   }     
                   if (conf==0) conf=1;
                   else {
                     Setup.UpdateChannels=4;
                     cChannel *SChannel= new cChannel; // OldChannel;
                     int tmp=OldChannel->Apid1();
                     char tmp2[1][4]={"deu"};
                     SChannel->SetPids(OldChannel->Vpid(),OldChannel->Ppid(),&tmp,tmp2,&tmp,tmp2,OldChannel->Tpid());
                     SChannel->cChannel::SetSatTransponderData(curSource->Code(),menuvalue[MI_FREQUENCY],*Pol,menuvalue[MI_SYMBOLRATE],FEC_AUTO,false);
                     if (cDevice::GetDevice(DvbKarte)==cDevice::ActualDevice())
                        HasSwitched=true;
                     cDevice::GetDevice(DvbKarte)->SwitchChannel(SChannel,HasSwitched);
                     conf=0;
                   }
                   break;
                 case MI_SATPOSITION:
                   if (curPosition) {
                     newpos=curPosition->Position();
                     if (LimitsDisabled || (newpos>=0 && newpos<=WestLimit)) {
                       CHECK(ioctl(fd_actuator,AC_WTARGET,&newpos));
                       PosTracker->Track();
                       needsFastResponse=true;
                     } else errormessage=EM_OUTSIDELIMITS; 
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
                Show();
                return state;
    }
  if ((oldcolumn!=menucolumn) || (menuline!=oldline)) {
    digits=0;
    conf=0;
   }
  }
  Show();
  return state;
}

void cMainMenuActuator::DisplaySignalInfoOnOsd(void)
{
      int RedLimit=415*30/100;
      int YellowLimit=max(RedLimit,415*60/100);
      int ShowSNR = 415*fe_snr/100;
      int ShowSS =  415*fe_ss/100;
      const cFont *textfont=cFont::GetFont(fontOsd);
      int rowheight=textfont->Height();
      
#define OSDWIDTH 655

if (paint==0) {
        paint=1;
      osd = cOsdProvider::NewOsd(40,50);
         tArea Area[] =    {{0, 0,           OSDWIDTH, rowheight*4-1, 4},   //rows 0..3  signal info
			   { 0, rowheight*5, OSDWIDTH, rowheight*13-1, 2},  //rows 5..12 menu
			   { 0, rowheight*13,OSDWIDTH, rowheight*14-1, 2}}; //row  13    prompt/error
	 osd->SetAreas(Area, sizeof(Area) / sizeof(tArea));			
;
	 osd->Flush();


}
      if(osd)
      {
         osd->DrawRectangle(0,0,OSDWIDTH,rowheight*13-1,clrGray50);
         char buf[1024];
         
         int y=0;
         
         actuator_status status;
         tColor text, background;
         CHECK(ioctl(fd_actuator, AC_RSTATUS, &status));
         switch(status.state) {
           case ACM_STOPPED:
           case ACM_CHANGE:
             snprintf(buf,sizeof(buf), tr(dishwait));
             background=clrWhite;
             text=clrBlack;
             break;
           case ACM_ERROR:
             snprintf(buf,sizeof(buf), tr(disherror));
             background=clrRed;
             text=clrWhite;
             break;
          default:
             snprintf(buf,sizeof(buf),tr(dishmoving),status.target, status.position);
             background=clrWhite;
             text=clrBlack;
         }      
         osd->DrawText(10,y,buf,text,background,textfont,OSDWIDTH,y+rowheight-1,taLeft);
         
         
         y+=rowheight;
         int bartop=y+rowheight/3;
         int barbottom=y+rowheight-rowheight/3-1;
         osd->DrawText(10, y, "SNR:",clrWhite,clrTransparent, textfont);
         osd->DrawRectangle(68, bartop, 68+min(RedLimit,ShowSNR), barbottom, clrRed);
         if(ShowSNR>RedLimit) osd->DrawRectangle(68+RedLimit, bartop, 68+min(YellowLimit,ShowSNR), barbottom, clrYellow);
         if(ShowSNR>YellowLimit) osd->DrawRectangle(68+YellowLimit, bartop, 68+ShowSNR, barbottom, clrGreen);
         sprintf(buf,"%d%% = %.1fdB",fe_snr,snrdB);
         osd->DrawText(485, y,buf,clrWhite,clrTransparent, textfont);
         
         y+=rowheight;
         bartop+=rowheight;
         barbottom+=rowheight;
         sprintf(buf,"SS:");
         osd->DrawText(10, y,"SS:",clrWhite,clrTransparent, textfont);
         osd->DrawRectangle(68, bartop, 68+min(RedLimit,ShowSS), barbottom, clrRed);
         if(ShowSS>RedLimit) osd->DrawRectangle(68+RedLimit, bartop, 68+min(YellowLimit,ShowSS), barbottom, clrYellow);
         if(ShowSS>YellowLimit) osd->DrawRectangle(68+YellowLimit, bartop, 68+ShowSS, barbottom, clrGreen);
         sprintf(buf,"%d%% = %.1fdBm",fe_ss,ssdBm);
         osd->DrawText(485, y,buf,clrWhite,clrTransparent, textfont);
         
#define OSDSTATUSWIN_XC(col,txt) (((col - 1) * OSDWIDTH / 5) + ((OSDWIDTH / 5 - textfont->Width(txt)) / 2))
         
         y+=rowheight;
         osd->DrawText(OSDSTATUSWIN_XC(1,"LOCK"),    y, "LOCK",    (fe_status & FE_HAS_LOCK)    ? clrYellow : clrBlack, clrTransparent, textfont);
         osd->DrawText(OSDSTATUSWIN_XC(2,"SIGNAL"),  y, "SIGNAL",  (fe_status & FE_HAS_SIGNAL)  ? clrYellow : clrBlack, clrTransparent, textfont);
         osd->DrawText(OSDSTATUSWIN_XC(3,"CARRIER"), y, "CARRIER", (fe_status & FE_HAS_CARRIER) ? clrYellow : clrBlack, clrTransparent, textfont);
         osd->DrawText(OSDSTATUSWIN_XC(4,"VITERBI"), y, "VITERBI", (fe_status & FE_HAS_VITERBI) ? clrYellow : clrBlack, clrTransparent, textfont);
         osd->DrawText(OSDSTATUSWIN_XC(5,"SYNC"),    y, "SYNC",    (fe_status & FE_HAS_SYNC)    ? clrYellow : clrBlack, clrTransparent, textfont);
         
         int left=10;
         int pagewidth=OSDWIDTH-left*2;
         int colspace=6;
         int colwidth=(pagewidth+colspace)/3;
         int curcol=0;
         
         y+=rowheight*2; //enter second display area
         
         int selected;
         if (menuline<4) selected=menuline*3+menucolumn;
           else selected=menuline+8;
           
         int itemindex;
         int x=left;
         for (itemindex=0; itemindex<=MAXMENUITEM; itemindex++) {
           int curwidth=menurowcol[itemindex][IWIDTH]*colwidth-colspace;
           if (itemindex==selected) {
             background=clrYellow;
             text=clrBlack;
           } else {
             background=clrGray50;
             text=clrYellow;
           }  
           switch(itemindex) {
             case MI_ENABLEDISABLELIMITS:
               snprintf(buf,sizeof(buf), LimitsDisabled ? tr(ENABLELIMITS) : tr(DISABLELIMITS));  
               break;
             case MI_FREQUENCY:
               curwidth-=colwidth;
               snprintf(buf, sizeof(buf),"%d%s ", menuvalue[itemindex], Pol);
               osd->DrawText(x+curwidth,y,buf,text,background,textfont,colwidth,rowheight,taRight);
               snprintf(buf, sizeof(buf), tr(menucaption[itemindex]));
               break;
             case MI_SYMBOLRATE:
               curwidth-=colwidth;
               snprintf(buf, sizeof(buf),"%d ", menuvalue[itemindex]);
               osd->DrawText(x+curwidth,y,buf,text,background,textfont,colwidth,rowheight,taRight);
               snprintf(buf, sizeof(buf),tr(menucaption[itemindex]));
               break;
             case MI_SATPOSITION:
               if (curPosition) snprintf(buf,sizeof(buf), "%s %s: %d",cSource::ToString(curSource->Code()),curSource->Description(),curPosition->Position()); 
               else snprintf(buf,sizeof(buf), "%s %s: %s",cSource::ToString(curSource->Code()),curSource->Description(),tr(dishnopos)); 
               break;  
             default:    
               snprintf(buf,sizeof(buf),tr(menucaption[itemindex]),menuvalue[itemindex]);
           }
           osd->DrawText(x,y,buf,text,background,textfont,curwidth,rowheight,
               curcol==0 ? taDefault : (curcol==1 ? taCenter : taRight));
           if(itemindex<12) {
             curcol++;
             x+=colwidth;
             if (curcol>2) {
               y+=rowheight;
               curcol=0;
               x=left;
             }
           } else {
             curcol=0;
             y+=rowheight;
             x=left;
           }  
         }
         
        //y=+rowheight;
        if (conf==1) osd->DrawText(left,y,tr("Are you sure?"),clrWhite,clrRed,textfont,OSDWIDTH-left-1,rowheight,taCenter);
        else {
          switch(errormessage) {
            case EM_OUTSIDELIMITS:
              snprintf(buf, sizeof(buf),"%s (0..%d)", tr(outsidelimits), WestLimit);
              osd->DrawText(left,y,buf,clrWhite,clrRed,textfont,OSDWIDTH-left-1,rowheight,taCenter);
              break;
            case EM_ATLIMITS:  
              snprintf(buf, sizeof(buf), "%s (0..%d)", tr(atlimits), WestLimit);
              osd->DrawText(left,y,buf,clrWhite,clrRed,textfont,OSDWIDTH-left-1,rowheight,taCenter);
              break;
            case EM_NOTPOSITIONED:
              osd->DrawText(left,y,tr(notpositioned),clrWhite,clrRed,textfont,OSDWIDTH-left-1,rowheight,taCenter);
              break;
            default:  
              osd->DrawRectangle(left,y,OSDWIDTH,y+rowheight-1,clrTransparent); 
          }  
        }
        osd->Flush();
      }
      else esyslog("Darn! Couldn't create osd");
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
      fe_ss=fe_ss/655;
      ssdBm = (logf(((double)fe_ss)/65535)*10.8);
      usleep(15);
      fe_snr=0;
      CHECK(ioctl(fd_frontend, FE_READ_SNR, &fe_snr));
      fe_snr=fe_snr/655;
      if (fe_snr>57000)
       snrdB = (logf((double)fe_snr/6553.5)*10.0);
      else
       snrdB = (-3/(logf(fe_snr/65535.0)));
      usleep(15);
      fe_unc=0;
      CHECK(ioctl(fd_frontend, FE_READ_UNCORRECTED_BLOCKS, &fe_unc));
}

bool cMainMenuActuator::Signal(int Frequenz, char *Pol, int Symbolrate)
{
  dvb_frontend_parameters Frontend;
  memset(&Frontend, 0, sizeof(Frontend));
  if (Frequenz < Setup.LnbSLOF) {
     Frontend.frequency=abs(Frequenz-Setup.LnbFrequLo) * 1000UL;
     CHECK(ioctl(fd_frontend, FE_SET_TONE, SEC_TONE_OFF));
        }
   else {
    Frontend.frequency=abs(Frequenz-Setup.LnbFrequHi) * 1000UL;
    CHECK(ioctl(fd_frontend, FE_SET_TONE, SEC_TONE_ON));
            }
  if (Pol=="H") {
    CHECK(ioctl(fd_frontend,FE_SET_VOLTAGE,SEC_VOLTAGE_18)); }
  else CHECK(ioctl(fd_frontend,FE_SET_VOLTAGE,SEC_VOLTAGE_13));
  Frontend.inversion=INVERSION_AUTO;
  Frontend.u.qpsk.symbol_rate=Symbolrate * 1000UL;
  Frontend.u.qpsk.fec_inner=FEC_AUTO;
  CHECK(ioctl(fd_frontend,FE_SET_FRONTEND,&Frontend));
  fe_status_t  status = fe_status_t(0);
  usleep(15);
  CHECK(ioctl(fd_frontend, FE_READ_STATUS, &status));
  if (status & FE_HAS_SYNC)
  return true;
        else return false;
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
  virtual void Housekeeping(void);
  virtual const char *MainMenuEntry(void) { return tr(MAINMENUENTRY); }
  virtual cOsdObject *MainMenuAction(void);
  virtual cMenuSetupPage *SetupMenu(void);
  virtual bool SetupParse(const char *Name, const char *Value);
};

cPluginActuator::cPluginActuator(void)
{
  statusMonitor = NULL;
  PosTracker = NULL;
  fd_actuator = open("/dev/actuator",0);
  if (fd_actuator<0) {
    esyslog("cannot open /dev/actuator");
    exit(1);
  }
  // Initialize any member variables here.
  // DON'T DO ANYTHING ELSE THAT MAY HAVE SIDE EFFECTS, REQUIRE GLOBAL
  // VDR OBJECTS TO EXIST OR PRODUCE ANY OUTPUT!
}

cPluginActuator::~cPluginActuator()
{
  // Clean up after yourself!
  delete statusMonitor;
  delete PosTracker;
  close(fd_actuator);
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
  RegisterI18n(Phrases);
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
  // Parse your own setup parameters and store their values.
  if      (!strcasecmp(Name, "DVB-Karte"))      DvbKarte = atoi(Value);
  else if (!strcasecmp(Name, "WestLimit"))      WestLimit = atoi(Value);
  else
    return false;
  return true;
}

VDRPLUGINCREATOR(cPluginActuator); // Don't touch this!

