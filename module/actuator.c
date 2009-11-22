/*
 * actuator.c -- control a linear actuator through the parallel
 * port.
 *
 * D0 (pin 2) is used as a "rotate west" output
 * D1 (pin 3) is "rotate east"
 * both have to be connected to a driver and a relay
 *
 * Ack input (pin 10) is connected to the reed switch in the
 * actuator for position control. It has do be connected through
 * a debouncing circuit
 *
 * Some code taken from the lirc_parallel driver
 *
 ******************************************************************************
 *
 * Copyright (C) 2004 Luca Olivetti <luca@olivetti.cjb.net>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/sched.h>
#ifdef CONFIG_DEVFS_FS
#include <linux/devfs_fs_kernel.h>
#endif
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/mm.h>
#include <linux/parport.h>
#include <linux/interrupt.h>
#include <linux/ioctl.h>
#include <asm/uaccess.h>
#include <linux/device.h>
#include <linux/version.h>
#include "actuator.h"

#define DRIVER_NAME "actuator"

/* compatibility defines */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,13)
# define class_device_create(a, b, c, d, e, f, g, h) class_simple_device_add(a, c, d, e, f, g, h)
# define class_device_destroy(a, b) class_simple_device_remove(b)
# define class_create(a, b) class_simple_create(a, b)
# define class_destroy(a) class_simple_destroy(a)
#else
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,15)
# define class_device_create(a, b, c, d, e, f, g, h) class_device_create(a, c, d, e, f, g, h)
#endif
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,18)
/* on older kernels, class_device_create will in turn be a compat macro */
# define device_create(a, b, c, d, e) class_device_create(a, b, c, d, e)
# define device_destroy(a ,b) class_device_destroy(a,b)
#endif

/* direction */
enum direction {
     MD_STOP=0,
     MD_WEST,
     MD_EAST
     };
     
static unsigned int base = 0x378;
module_param(base, int, 0);
MODULE_PARM_DESC(base,"I/O address of the parallel port to use (defaults to 0x378)");

int major;

struct parport *pport;
struct pardevice *ppdevice;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,13)
struct class_simple *actuator_class;
#else
struct class *actuator_class;
#endif

/* timer, used as a delay for position reached/direction changed */

static struct timer_list timer;
static int stopping;
#ifdef TESTSIGNAL
/* generates a signal on pin 4 for testing with no real hardware 
   (connect pin 4 to pin 10) */
static struct timer_list testsignalt;
static int testsignal;
#endif

static rwlock_t lock;
static struct actuator_status status;
static unsigned char lastdirection;
static unsigned int nextstate;

static void set_timer(void)
{
  if (timer_pending(&timer)) mod_timer(&timer, jiffies+HZ*2); 
  else {
    timer.expires = jiffies+HZ*2;
    add_timer(&timer);
  }
}

static void motor_go(unsigned char direction)
{
  parport_write_data(pport,direction);
  if (direction != MD_STOP) {
    lastdirection = direction;
    if (direction == MD_WEST) status.state = ACM_WEST; else status.state = ACM_EAST;
  }  
}

static void set_mode(unsigned int newmode, int newtarget)
{
  status.mode=newmode;
  switch(status.mode)
  {
  case AC_STOP:
      motor_go(MD_STOP);
      nextstate=ACM_IDLE;
      status.state=ACM_STOPPED;
      set_timer();
      return;
      
  case AC_AUTO:
      status.target=newtarget;
      if (status.state==ACM_REACHED) status.state=ACM_STOPPED;
      if (status.target>status.position) nextstate=ACM_WEST;
      else if (status.target<status.position) nextstate=ACM_EAST;
      else nextstate=ACM_REACHED;
      break;
      
  case AC_MANUAL_WEST:
      nextstate=ACM_WEST;
      break;
      
  case AC_MANUAL_EAST:
      nextstate=ACM_EAST;  
      break;
  }
  
  if ((status.state==ACM_EAST && nextstate==ACM_WEST) || (status.state==ACM_WEST && nextstate==ACM_EAST))
  {
    motor_go(MD_STOP);
    status.state=ACM_CHANGE;
    set_timer();
  }
  else if (status.state==ACM_IDLE)
  {
    status.state=nextstate;
    nextstate=ACM_IDLE;
    if (status.state==ACM_EAST) motor_go(MD_EAST);
    else if (status.state==ACM_WEST) motor_go(MD_WEST);
    else motor_go(MD_STOP);
    if (status.state!=ACM_IDLE) set_timer();
  }
}

static void actuator_timer (unsigned long cookie)
{
  write_lock(&lock);
  if (stopping) {
    write_unlock(&lock);
    return;
  }  
  switch (status.state) {
    case ACM_WEST:
    case ACM_EAST:
      parport_write_data(pport,MD_STOP);
      status.state = ACM_ERROR;
      lastdirection = MD_STOP; //avoid spurious pulses
      set_timer();
      break;
    case ACM_ERROR:
      status.state=ACM_IDLE;
      break;
    case ACM_REACHED:
    case ACM_STOPPED:
    case ACM_CHANGE:
      status.state=nextstate;
      nextstate=ACM_IDLE;
      lastdirection=MD_STOP; //if needed motor_go will change it
      if (status.state==ACM_EAST) motor_go(MD_EAST);
      else if (status.state==ACM_WEST) motor_go(MD_WEST);
      if (status.state!=ACM_IDLE) set_timer();
      break;
  }        
  write_unlock(&lock);
}  

#ifdef TESTSIGNAL
static void testsignal_timer (unsigned long cookie)
{
  write_lock(&lock);
  if (stopping) {
    write_unlock(&lock);
    return;
  }  
  if (status.state == ACM_WEST || status.state == ACM_EAST) {
    parport_write_data(pport,testsignal);
    testsignal=4-testsignal;
  }  
  testsignalt.expires=jiffies+HZ/10;
  add_timer(&testsignalt);
  write_unlock(&lock);
}
#endif

/*
 *  Open device
 */

int actuator_open (struct inode *inode, struct file *filp)
{
    try_module_get(THIS_MODULE);
    return 0;
}


int actuator_release (struct inode *inode, struct file *filp)
{
    module_put(THIS_MODULE);
    return 0;
}

static int actuator_ioctl(struct inode *node, struct file *filep, unsigned int cmd,
                          unsigned long arg)
{
    int result;
    int new;
    unsigned long flags;
    
    switch(cmd)
    {
    case AC_RSTATUS:
        read_lock_irqsave(&lock,flags);
        result=copy_to_user((void *)arg, &status, sizeof(status));
        read_unlock_irqrestore(&lock,flags);
        if (result) return -EFAULT;
        break;

    case AC_WPOS:
        write_lock_irqsave(&lock,flags);
        set_mode(AC_STOP,0);
        result=get_user(status.position, (unsigned int *) arg);
        write_unlock_irqrestore(&lock,flags);
        if(result) return result;
        break;

    case AC_WTARGET:
        result=get_user(new, (int *) arg);
        if(result) return result;
        write_lock_irqsave(&lock,flags);
        set_mode(AC_AUTO, new);
        write_unlock_irqrestore(&lock,flags);
        break;

    case AC_MSTOP:
        write_lock_irqsave(&lock,flags);
        set_mode(AC_STOP, 0);
        write_unlock_irqrestore(&lock,flags);
        break;
        
    case AC_MWEST:
        write_lock_irqsave(&lock,flags);
        set_mode(AC_MANUAL_WEST, 0);
        write_unlock_irqrestore(&lock,flags);
        break;
        
    case AC_MEAST:
        write_lock_irqsave(&lock,flags);
        set_mode(AC_MANUAL_EAST, 0);
        write_unlock_irqrestore(&lock,flags);
        break;
    
    }
    return(0);
}

struct file_operations actuator_fops = {
    open: actuator_open,
    release: actuator_release,
    ioctl: actuator_ioctl,
};

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,18)
void actuator_interrupt(int irq, void *dev_id, struct pt_regs *regs)
#else
void actuator_interrupt(void *private)
#endif
{
	write_lock(&lock);
	if (stopping) {
	  write_unlock(&lock);
	  return;
	}  
	if (lastdirection == MD_WEST) status.position++;
	else if (lastdirection == MD_EAST) status.position--;
	
	if (status.state == ACM_WEST || status.state == ACM_EAST) {
	  if (((status.state == ACM_WEST && status.position >= status.target) || 
	       (status.state == ACM_EAST && status.position <= status.target)) && status.mode == AC_AUTO) {
            status.state = ACM_REACHED;
            motor_go(MD_STOP); /* stop the motor */
          }  
          set_timer();
	  }
 
        write_unlock(&lock);

}

                         
/* Finally, init and cleanup */


int actuator_init(void)
{
    int i=0;

    
    while((pport=parport_find_number(i++))!=NULL)
    {
      if(pport->base==base)
		break;
    }
    if(pport==NULL)
    {
      printk(KERN_NOTICE "%s : no port at 0x%x found\n", DRIVER_NAME, base);
      return(-ENXIO);
    }
    
    if(pport->irq==-1)
    {
      printk(KERN_NOTICE "%s : port at 0x%x without interrupt\n", DRIVER_NAME, base);
      return(-ENXIO);
    } 
    
    ppdevice=parport_register_device(pport,DRIVER_NAME,
                                     NULL, /* no preempt */
                                     NULL, /* no wakeup */
                                     actuator_interrupt,PARPORT_DEV_EXCL,NULL);
    if(ppdevice==NULL)
    {
      printk(KERN_NOTICE "%s: parport_register_device() failed\n",
             DRIVER_NAME);
      return(-ENXIO);
    }
    
    if(parport_claim(ppdevice)!=0)
    {
      printk(KERN_WARNING "%s: could not claim port\n", DRIVER_NAME);
      parport_unregister_device(ppdevice);
      return(-ENXIO);
     } 

     major = register_chrdev(0, DRIVER_NAME, &actuator_fops);
     if (major<0)
     {
       printk(KERN_WARNING "%s: can't get major\n", DRIVER_NAME);
       parport_release(ppdevice);
       parport_unregister_device(ppdevice);
       return major;
     }
     
#ifdef CONFIG_DEVFS_FS
     devfs_mk_cdev(MKDEV(major, 0), S_IFCHR | S_IRUSR | S_IWUSR, DRIVER_NAME);
#endif     
     actuator_class = class_create(THIS_MODULE, DRIVER_NAME);
     device_create(actuator_class,NULL,MKDEV(major,0),NULL,DRIVER_NAME);

    /* set up timer function */
    init_timer(&timer);
    timer.function=actuator_timer;
    
    /* set up spinlock */
    rwlock_init(&lock);

    stopping = 0;
    
#ifdef TESTSIGNAL
    init_timer(&testsignalt);
    testsignalt.function=testsignal_timer;
    testsignal = 0;
    testsignalt.expires=jiffies+HZ/10;
    add_timer(&testsignalt);
#endif        
    
    /* enable interrupt */
    pport->ops->enable_irq(pport);
    return 0;
}

void actuator_cleanup(void)
{
     
    unsigned long flags;
     
    if(module_refcount(THIS_MODULE)) return;

    write_lock_irqsave(&lock,flags);
    stopping = 1; /* prevents the timer from rescheduling and the interrupt from running */
    write_unlock_irqrestore(&lock,flags);

    del_timer_sync(&timer);
#ifdef TESTSIGNAL
    del_timer_sync(&testsignalt);
#endif        
    
    parport_write_data(pport,0x00); /* disable outputs */
    
#ifdef CONFIG_DEVFS_FS
    devfs_remove(DRIVER_NAME);
#endif    
    device_destroy(actuator_class,MKDEV(major,0));
    class_destroy(actuator_class);
    unregister_chrdev(major, DRIVER_NAME); 

    /* release the parallel port */
    pport->ops->disable_irq(pport);   /* disable the interrupt */
    parport_release(ppdevice);
    parport_unregister_device(ppdevice);
    
}

MODULE_AUTHOR("Luca Olivetti");
MODULE_DESCRIPTION("Control a linear actuator through the parallel port");
MODULE_LICENSE("GPL");

module_init(actuator_init);
module_exit(actuator_cleanup);

