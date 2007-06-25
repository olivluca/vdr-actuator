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

static const char *VERSION        = "0.0.1";
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
#define POSTOLERANCE  10

//Plugin parameters 
int DvbKarte=0;
int WestLimit=0;
//EastLimit is always 0

//actuator device
int fd_actuator;

//position memory 
const char *PositionFilename;
int LastPositionSaved;
cMutex *PosMutex;

//how to access SetupStore inside the main menu?
cPlugin *theplugin;

// ----------------------------------------------------------------------------

void SavePos(int NewPos) {
  cMutexLock(PosMutex);
  if(NewPos!=LastPositionSaved) {
    cSafeFile f(PositionFilename);
    if (f.Open()) {
      fprintf(f,"%d",NewPos);
      f.Close();
      LastPositionSaved=NewPos;
    } else LOG_ERROR;
  }    
}

// --- cSatPosition -----------------------------------------------------------
// associates one source with its position

class cSatPosition:public cListObject {
private:
  int source;
  int position;
public:
  cSatPosition(void) {}  
  cSatPosition(int Source, int Position);
//  ~cSatPosition();
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

// --- cStatusMonitor -------------------------------------------------------

class cStatusMonitor:public cStatus {
private:
  bool transfer;
  unsigned int last_state_shown;
  int last_position_shown;
  int oldupdate;
  int target;
protected:
  virtual void ChannelSwitch(const cDevice *Device, int ChannelNumber);
  virtual bool AlterDisplayChannel(cSkinDisplayChannel *displayChannel);
public:
  cStatusMonitor();
  ~cStatusMonitor();
};

cStatusMonitor::cStatusMonitor()
{
  oldupdate=-1;
  target=-1;
  transfer=false;
  last_state_shown=ACM_IDLE;
}

cStatusMonitor::~cStatusMonitor()
{
  if (oldupdate>=0) Setup.UpdateChannels=oldupdate;
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
     //workaround for broken autoupdate logic
     if (oldupdate<0) {
       oldupdate=Setup.UpdateChannels;
       Setup.UpdateChannels=0;
     }
     target=-1;
     if (p) {
       target=p->Position();
       dsyslog("New sat position: %i",target);
       if (target>=0 && target<=WestLimit) {
         CHECK(ioctl(fd_actuator, AC_RSTATUS, &status));
         SavePos(status.position);
         if (status.position!=target || status.target!=target)  {
           dsyslog("Satellite changed");
           CHECK(ioctl(fd_actuator, AC_WTARGET, &target));
         } else { //already positioned
           if(oldupdate>=0) {
             Setup.UpdateChannels=oldupdate;
             oldupdate=-1;
             }
           target=-1;  
           }
       } else {
         CHECK(ioctl(fd_actuator, AC_MSTOP));
         Skins.Message(mtError, tr(outsidelimits));
         target=-1;
       }      
     } else Skins.Message(mtError, tr(dishnopos));
          
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
  SavePos(status.position);
  
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
        if (target>=0 && status.target == target && oldupdate>=0 ) {
          Setup.UpdateChannels=oldupdate;
          oldupdate=-1;
          }
        target=-1;    
        return false;
        
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
  int digits,paint,menucolumn,menuline,conf,repeat,oldupdate,HasSwitched,fd_frontend;
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

public:
  cMainMenuActuator(void);
  ~cMainMenuActuator();
  virtual void Show(void);
  virtual eOSState ProcessKey(eKeys Key);
  void DisplaySignalInfoOnOsd(fe_status_t status,unsigned int ber,unsigned int ss,float ssdBm,unsigned int snr,float snrdB,unsigned int unc);
  void GetSignalInfo(         fe_status_t &status,
                             unsigned int &BERChip,
                             unsigned int &SSChip,
                             unsigned int &SNRChip,
                             unsigned int &UNCChip  );
  bool Signal(int Frequenz, char *Pol, int Symbolrate);
};


cMainMenuActuator::cMainMenuActuator(void)
{
  oldupdate=Setup.UpdateChannels;
  Setup.UpdateChannels=0;
  OldChannel=Channels.GetByNumber(cDevice::GetDevice(DvbKarte)->CurrentChannel());
  repeat=HasSwitched=false;
  conf=0;
  errormessage=EM_NONE;
  menucolumn=menuvalue[MI_STEPSEAST]=menuvalue[MI_STEPSWEST]=1;
  menuline=1;
  paint=0;
  osd=NULL;
  LimitsDisabled=false;
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
  SavePos(status.position);
}

cMainMenuActuator::~cMainMenuActuator()
{
  delete osd;
  close(fd_frontend);
  CHECK(ioctl(fd_actuator, AC_MSTOP));
  //if (HasSwitched) {
     cDevice::GetDevice(DvbKarte)->SwitchChannel(OldChannel,true);
  //   }
  Setup.UpdateChannels=oldupdate;
}


void cMainMenuActuator::Show(void)
{
        fe_status_t status;
        unsigned int BERChip=0;
        unsigned int SSChip=0;
        unsigned int SNRChip=0;
        unsigned int UNCChip=0;
        GetSignalInfo(status,BERChip,SSChip,SNRChip,UNCChip);
        float snrdB = 0.0;
        float ssdBm = 0.0;
        unsigned int unc = UNCChip;
        unsigned int ber = BERChip;
        unsigned int ss = SSChip/655;
        unsigned int snr = SNRChip/655;
        ssdBm = (logf(((double)SSChip)/65535)*10.8);
        if (SNRChip>57000)
         snrdB = (logf((double)SNRChip/6553.5)*10.0);
        else
         snrdB = (-3/(logf(SNRChip/65535.0)));
        DisplaySignalInfoOnOsd(status,ber,ss,ssdBm,snr,snrdB,unc);

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
  SavePos(status.position);
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
             Setup.UpdateChannels=0;
             switch(selected) {
                case MI_DRIVEEAST:
                   if (status.position<=0 && !LimitsDisabled) errormessage=EM_ATLIMITS;
                   else {   
                     CHECK(ioctl(fd_actuator, AC_MEAST));
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
                     needsFastResponse=true;
                   }  
                   break;
                case MI_RECALC:
                   if (curPosition) {
                     if (conf==0) conf=1;
                     else {
                       newpos=curPosition->Position();
                       CHECK(ioctl(fd_actuator, AC_WPOS, &newpos));
                       conf=0;
                     }  
                   }
                   break;
                case MI_GOTO:
                   if (LimitsDisabled || (menuvalue[MI_GOTO]>=0 && menuvalue[MI_GOTO]<=WestLimit)) {
                     CHECK(ioctl(fd_actuator, AC_WTARGET, &menuvalue[MI_GOTO]));
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
                     conf=0;
                   }  
                   break;
                case MI_SETZERO:
                   if (conf==0) conf=1;
                   else {
                     newpos=0;
                     CHECK(ioctl(fd_actuator, AC_WPOS, &newpos));
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

void cMainMenuActuator::DisplaySignalInfoOnOsd(fe_status_t status,unsigned int ber,unsigned int ss,float ssdBm,unsigned int snr,float snrdB,unsigned int unc)
{
      int RedLimit=415*30/100;
      int YellowLimit=max(RedLimit,415*60/100);
      int ShowSNR = 415*snr/100;
      int ShowSS =  415*ss/100;
      int PositionY = 0;

if (paint==0) {
        paint=1;
      osd = cOsdProvider::NewOsd(40,50);
         tArea Area[] =    {{0,0, 655, 119, 4},
			   { 0, 120,655,419, 2},
			   { 0, 420,655,449,2}};
	 osd->SetAreas(Area, sizeof(Area) / sizeof(tArea));			
;
	 osd->Flush();


}
      if(osd)
      {
         osd->DrawRectangle(0,0,655,470,clrGray50);
         char buf[1024];
         const cFont *textfont=cFont::GetFont(fontOsd);
         
         sprintf(buf,"SNR:");
         osd->DrawText(10, 3,buf,clrWhite,clrTransparent, textfont);
         osd->DrawRectangle(68, 10, 68+min(RedLimit,ShowSNR), 25, clrRed);
         if(ShowSNR>RedLimit) osd->DrawRectangle(68+RedLimit, 10, 68+min(YellowLimit,ShowSNR), 25, clrYellow);
         if(ShowSNR>YellowLimit) osd->DrawRectangle(68+YellowLimit, 10, 68+ShowSNR, 25, clrGreen);
         
         sprintf(buf,"%d%% = %.1fdB",snr,snrdB);
         osd->DrawText(485, 3,buf,clrWhite,clrTransparent, textfont);
         PositionY = PositionY + 35;
         sprintf(buf,"SS:");
         osd->DrawText(10, 3+PositionY,buf,clrWhite,clrTransparent, textfont);
         osd->DrawRectangle(68, 10+PositionY, 68+min(RedLimit,ShowSS), 25+PositionY, clrRed);
         if(ShowSS>RedLimit) osd->DrawRectangle(68+RedLimit, 10+PositionY, 68+min(YellowLimit,ShowSS), 25+PositionY, clrYellow);
         if(ShowSS>YellowLimit) osd->DrawRectangle(68+YellowLimit, 10+PositionY, 68+ShowSS, 25+PositionY, clrGreen);
         
         sprintf(buf,"%d%% = %.1fdBm",ss,ssdBm);
         osd->DrawText(485, 3+PositionY,buf,clrWhite,clrTransparent, textfont);
         PositionY = PositionY + 35;
         
         osd->DrawRectangle(100,70,110,80, (status & FE_HAS_SIGNAL) ? clrGreen : clrRed);
         osd->DrawRectangle(150,70,160,80, (status & FE_HAS_CARRIER) ? clrGreen : clrRed);
         osd->DrawRectangle(200,70,210,80, (status & FE_HAS_VITERBI) ? clrGreen : clrRed);
         osd->DrawRectangle(250,70,260,80, (status & FE_HAS_SYNC) ? clrGreen : clrRed);
         
         tColor text, background;
         
         int top=90;
         int left=15;
         int pagewidth=655-left*2;
         int colspace=6;
         int colwidth=(pagewidth+colspace)/3;
         int rowheight=30;
         int rowspace=3;
         int height=rowheight-rowspace;
         int currow=0;
         int curcol=0;
         int curtop;
         
         actuator_status status;
         CHECK(ioctl(fd_actuator, AC_RSTATUS, &status));
         SavePos(status.position);
         switch(status.state) {
           case ACM_STOPPED:
           case ACM_CHANGE:
             snprintf(buf,sizeof(buf), tr(dishwait));
             background=clrGray50;
             text=clrWhite;
             break;
           case ACM_ERROR:
             snprintf(buf,sizeof(buf), tr(disherror));
             background=clrRed;
             text=clrWhite;
             break;
          default:
             snprintf(buf,sizeof(buf),tr(dishmoving),status.target, status.position);
             background=clrGray50;
             text=clrWhite;
         }      
         osd->DrawText(left,top+currow*rowheight,buf,text,background,textfont,pagewidth,height);
         
         currow+=2; //empty line -- enter second tArea
         
         int selected;
         if (menuline<4) selected=menuline*3+menucolumn;
           else selected=menuline+8;
           
         int itemindex;
         for (itemindex=0; itemindex<=MAXMENUITEM; itemindex++) {
           curtop=top+currow*rowheight;
           int curleft=left+curcol*colwidth;
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
               osd->DrawText(curleft+curwidth,curtop,buf,text,background,textfont,colwidth,height,taRight);
               snprintf(buf, sizeof(buf), tr(menucaption[itemindex]));
               break;
             case MI_SYMBOLRATE:
               curwidth-=colwidth;
               snprintf(buf, sizeof(buf),"%d ", menuvalue[itemindex]);
               osd->DrawText(curleft+curwidth,curtop,buf,text,background,textfont,colwidth,height,taRight);
               snprintf(buf, sizeof(buf),tr(menucaption[itemindex]));
               break;
             case MI_SATPOSITION:
               if (curPosition) snprintf(buf,sizeof(buf), "%s %s: %d",cSource::ToString(curSource->Code()),curSource->Description(),curPosition->Position()); 
               else snprintf(buf,sizeof(buf), "%s %s: %s",cSource::ToString(curSource->Code()),curSource->Description(),tr(dishnopos)); 
               break;  
             default:    
               snprintf(buf,sizeof(buf),tr(menucaption[itemindex]),menuvalue[itemindex]);
           }
           osd->DrawText(curleft,curtop,buf,text,background,textfont,curwidth,height,
               curcol==0 ? taDefault : (curcol==1 ? taCenter : taRight));
           if(itemindex<12) {
             curcol++;
             if (curcol>2) {
               currow++;
               curcol=0;
             }
           } else {
             curcol=0;
             currow++;
           }  
         }
        currow++;
        curtop=top+currow*rowheight;
         
        if (conf==1) osd->DrawText(left,curtop,tr("Are you sure?"),clrWhite,clrRed,textfont,pagewidth,height,taCenter);
        else {
          switch(errormessage) {
            case EM_OUTSIDELIMITS:
              snprintf(buf, sizeof(buf),"%s (0..%d)", tr(outsidelimits), WestLimit);
              osd->DrawText(left,curtop,buf,clrWhite,clrRed,textfont,pagewidth,height,taCenter);
              break;
            case EM_ATLIMITS:  
              snprintf(buf, sizeof(buf), "%s (0..%d)", tr(atlimits), WestLimit);
              osd->DrawText(left,curtop,buf,clrWhite,clrRed,textfont,pagewidth,height,taCenter);
              break;
            case EM_NOTPOSITIONED:
              osd->DrawText(left,curtop,tr(notpositioned),clrWhite,clrRed,textfont,pagewidth,height,taCenter);
              break;
            default:  
              osd->DrawRectangle(left,curtop,left+pagewidth-1,curtop+height-1,clrGray50); 
          }  
        }
        osd->Flush();
      }
      else esyslog("Darn! Couldn't create osd");
}

void cMainMenuActuator::GetSignalInfo(fe_status_t &status,
                             unsigned int &BERChip,
                             unsigned int &SSChip,
                             unsigned int &SNRChip,
                             unsigned int &UNCChip
                                )
{
      status = fe_status_t(0);
      usleep(15);
      CHECK(ioctl(fd_frontend, FE_READ_STATUS, &status));
      usleep(15);                                                 //µsleep necessary else returned values may be junk
      BERChip=0;                                    //set variables to zero before ioctl, else FE_call sometimes returns junk
      CHECK(ioctl(fd_frontend, FE_READ_BER, &BERChip));
      usleep(15);
      SSChip=0;
      CHECK(ioctl(fd_frontend, FE_READ_SIGNAL_STRENGTH, &SSChip));
      usleep(15);
      SNRChip=0;
      CHECK(ioctl(fd_frontend, FE_READ_SNR, &SNRChip));
      usleep(15);
      UNCChip=0;
      CHECK(ioctl(fd_frontend, FE_READ_UNCORRECTED_BLOCKS, &UNCChip));
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
  fd_actuator = open("/dev/actuator",0);
  if (fd_actuator<0) {
    esyslog("Cannot open /dev/actuator: %s", strerror(errno));
    exit(1);
  }
  PosMutex = new cMutex();
  // Initialize any member variables here.
  // DON'T DO ANYTHING ELSE THAT MAY HAVE SIDE EFFECTS, REQUIRE GLOBAL
  // VDR OBJECTS TO EXIST OR PRODUCE ANY OUTPUT!
}

cPluginActuator::~cPluginActuator()
{
  // Clean up after yourself!
  actuator_status status;
  CHECK(ioctl(fd_actuator, AC_RSTATUS, &status));
  SavePos(status.position);
  delete statusMonitor;
  delete PosMutex;
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
  PositionFilename=strdup(AddDirectory(ConfigDirectory(), "actuator.pos"));
  FILE *f=fopen(PositionFilename,"r");
  if(f) {
    int newpos;
    if (fscanf(f,"%d",&newpos)==1) { 
      isyslog("Read position %d from %s",newpos,PositionFilename); 
      CHECK(ioctl(fd_actuator, AC_WPOS, &newpos));
    } else esyslog("couldn't read dish position from %s",PositionFilename);
    fclose(f);
  } else esyslog("Couldn't open file %s: %s",PositionFilename,strerror(errno));
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
  actuator_status status;
  CHECK(ioctl(fd_actuator, AC_RSTATUS, &status));
  SavePos(status.position);
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

