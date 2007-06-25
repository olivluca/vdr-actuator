import curses
import curses.textpad
from fcntl import ioctl
import struct

AC_RSTATUS=-2146406272 #0x80107080
AC_WPOS=0x40047081
AC_WTARGET=0x40047082
AC_MSTOP=0x7083
AC_MWEST=0x7084
AC_MEAST=0x7085

modes=('STOP','AUTO','MAN WEST','MAN EAST')
states=('IDLE','WEST','EAST','REACHED','STOPPED', 'CHANGE','ERROR')


def mifunc(stdscr):
  curses.halfdelay(1)
  w=curses.newwin(3,20,10,1)
  ac=open("/dev/actuator","rw")
  while 1:
    c=stdscr.getch()
    if c == ord('q'): 
      break
    elif c == ord(' '):
      # stop
      ioctl(ac,AC_MSTOP)
    elif c == curses.KEY_RIGHT:
      ioctl(ac,AC_MWEST)
    elif c == curses.KEY_LEFT:
      ioctl(ac,AC_MEAST)
    elif c == ord('g'):
      newpos=int(curses.textpad.Textbox(w).edit())
      ioctl(ac,AC_WTARGET,struct.pack("i",newpos))
    elif c == ord('p'):
      newpos=int(curses.textpad.Textbox(w).edit())
      ioctl(ac,AC_WPOS,struct.pack("i",newpos))
    stat=ioctl(ac,AC_RSTATUS,"                ");
    mode=struct.unpack("I",stat[0:4])[0]
    state=struct.unpack("I",stat[4:8])[0]
    target=struct.unpack("i",stat[8:12])[0]
    position=struct.unpack("i",stat[12:16])[0]
    stdscr.addstr(2,1,str(position));stdscr.clrtoeol()
    stdscr.addstr(3,1,str(target));stdscr.clrtoeol()
    stdscr.addstr(4,1,modes[mode]);stdscr.clrtoeol()
    stdscr.addstr(5,1,states[state]);stdscr.clrtoeol()
    

curses.wrapper(mifunc)
