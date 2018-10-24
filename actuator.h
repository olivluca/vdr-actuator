/*
 * actuator.c: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id$
 */

#ifndef _ACTUATOR_H
#define _ACTUATOR_H
#include <vdr/plugin.h>
#include "module/actuator.h"



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


// --- cTransponder -----------------------------------------------------------
//Transponder data for satellite scan

class cTransponder:public cListObject {
private:
  int frequency;
  int srate;
  int system;
  int modulation;
  int fec;
  char polarization;
  
public:
  cTransponder(void) { frequency=0; }  
  ~cTransponder();
  bool Parse(const char *s);
  int Frequency(void) const { return frequency; }
  int Srate(void) const { return srate; }
  int System(void) const { return system; }
  int Modulation(void) const { return modulation; }
  int Fec(void) const { return fec; }
  char Polarization(void) const { return polarization; }
  };

// --- cTransponders --------------------------------------------------------
//All the transponders of the current satellite

class cTransponders : public cConfig<cTransponder> {
public:
  void LoadTransponders(int source);
  //dvb_file *Entries(void);
  };  

// -- cMainMenuActuator --------------------------------------------------------

class dvbScanner;
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
  char Pol;
  cChannel *OldChannel;
  cChannel *SChannel;
  cOsd *osd;
  const cFont *textfont;
  fe_status_t fe_status;
  uint16_t fe_ss;
  uint16_t fe_snr;
  uint32_t fe_ber;
  uint32_t fe_unc;
  cTimeMs *scantime;
  cTimeMs *refresh;
  bool HideOsd;
  dvbScanner *Scanner;
  enum sm {
    SM_NONE,
    SM_TRANSPONDER,
    SM_SATELLITE
    } scanmode;
  void DisplayOsd(void);
  void GetSignalInfo(void);
  void Tune(void);
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
  bool Tune(int delsys, int modulation, int frequency, char polarization, int symbolrate);
  cMainMenuActuator(void);
  ~cMainMenuActuator();
  void Refresh(void);
  virtual void Show(void);
  virtual eOSState ProcessKey(eKeys Key);
};


#endif
