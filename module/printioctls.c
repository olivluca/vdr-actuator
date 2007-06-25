#include "actuator.h"
#include <stdio.h>

main()
{
printf("AC_RSTATUS=0x%x\n",AC_RSTATUS);
printf("AC_WPOS=0x%x\n",AC_WPOS);
printf("AC_WTARGET=0x%x\n",AC_WTARGET);
printf("AC_MSTOP=0x%x\n",AC_MSTOP);
printf("AC_MWEST=0x%x\n",AC_MWEST);
printf("AC_MEAST=0x%x\n",AC_MEAST);
}
