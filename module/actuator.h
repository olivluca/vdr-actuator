#if defined (__linux__)
#include <asm/types.h>
#include <linux/ioctl.h>
#else
#include <sys/types.h>
typedef u_int32_t __u32;
#endif

/* operating modes of the actuator */
enum operating_modes {
             AC_STOP=0,
             AC_AUTO,
             AC_MANUAL_WEST,
             AC_MANUAL_EAST
             };
             
             
     
/* states in auto */
enum motor_states {
     ACM_IDLE=0,
     ACM_WEST,
     ACM_EAST,
     ACM_REACHED,
     ACM_STOPPED,
     ACM_CHANGE,
     ACM_ERROR
     };

/* actuator status */
struct actuator_status {
    unsigned int mode;
    unsigned int state;
    int target;
    int position;
    };
     
/* ioctls */
      
#define PAC_IOCTL	'p'

     
/* Read current status */
#define AC_RSTATUS	_IOR(PAC_IOCTL, 0x80, struct actuator_status)

/* Write current position (zero adjustment) */
#define AC_WPOS		_IOW(PAC_IOCTL, 0x81, int)

/* Write target */
#define AC_WTARGET	_IOW(PAC_IOCTL, 0x82, int)

/* Stop motor */
#define AC_MSTOP	_IO(PAC_IOCTL, 0x83)

/* Manual west */
#define AC_MWEST	_IO(PAC_IOCTL, 0x84)

/* Manual east */
#define AC_MEAST	_IO(PAC_IOCTL, 0x85)
