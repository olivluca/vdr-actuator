/*
 * actuator.c -- control a linear actuator through the control
 * signal of a serial port.
 *
 *  Signal  DB9  DB25  Function
 *==============================================================
 *   RTS     7    4    rotate west output
 *   DTR     4   20    rotate east output
 *   CTS     8    5    pulse counter input
 *
 * the rotate east/west outputs have to be connected to a driver and
 * a relay
 *
 * the input is connected to the reed switch in the
 * actuator for position control. It has do be connected through
 * a debouncing circuit
 *
 ******************************************************************************
 *
 * Copyright (C) 2009 Luca Olivetti <luca@olivetti.cjb.net>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
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
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/ioctl.h>
#include <linux/uaccess.h>
#include <linux/device.h>
#include <linux/version.h>
#include <linux/tty.h>
#include "actuator.h"

#define DRIVER_NAME "seractuator"
#define DEV_NAME "actuator"
/* this is very conservative, enough for 25 pulses per second*/
#define SAMPLING_RATE HZ/50
#define TIMEOUT HZ*2

/* compatibility defines */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,18)
/* on older kernels, class_device_create will in turn be a compat macro */
# define device_create(a, b, c, d, e) class_device_create(a, b, c, d, e)
# define device_destroy(a, b) class_device_destroy(a, b)
#endif

/* direction */
enum direction {
	MD_STOP = 0,
	MD_WEST,
	MD_EAST
};

static char port[20];
module_param_string(port, port, 20, 0);
MODULE_PARM_DESC(port, "serial port to use");

static int major;
static int opened = 0;
static int oldpulse;
static unsigned long timeout;

struct file *serport;
struct class *actuator_class;

/* workqueue used for polling */
static struct workqueue_struct *actuator_workqueue;
/* workqueue API changed in kernel 2.6.20 */
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20) )
static void actuator_poll(void *);
static DECLARE_WORK(actuator_work, actuator_poll, NULL);
#else
static void actuator_poll(struct work_struct *);
static DECLARE_DELAYED_WORK(actuator_work, actuator_poll);
#endif

static rwlock_t lock;
static struct actuator_status status;
static unsigned char lastdirection;
static unsigned int nextstate;

static long do_ioctl(struct file *f, unsigned op, unsigned long param)
{
#ifdef HAVE_UNLOCKED_IOCTL
	if (f->f_op->unlocked_ioctl) {
		return f->f_op->unlocked_ioctl(f, op, param);
	}
#else
	if (f->f_op->ioctl) {
		return f->f_op->ioctl(f->f_dentry->d_inode, f, op, param);
	}
#endif	
	return -ENOSYS;
}

static int check_pulse(void)
{
	int lines = 0;
	mm_segment_t old_fs = get_fs();
	set_fs(KERNEL_DS);
	do_ioctl(serport, TIOCMGET, (unsigned long)&lines);
	set_fs(old_fs);
	if (lines & TIOCM_CTS)
		return 1;
	return 0;
}

static long motor_go(unsigned char direction)
{
	int lines;
	long result;

	mm_segment_t old_fs = get_fs();
	set_fs(KERNEL_DS);
	result = do_ioctl(serport, TIOCMGET, (unsigned long)&lines);
	if (result) {
		set_fs(old_fs);
		return result;
	}
	lines &= ~TIOCM_DTR;
	lines &= ~TIOCM_RTS;
	if (direction == MD_WEST)
		lines |= TIOCM_RTS;
	else if (direction == MD_EAST)
		lines |= TIOCM_DTR;
	result = do_ioctl(serport, TIOCMSET, (unsigned long)&lines);
	set_fs(old_fs);
	if (direction != MD_STOP) {
		lastdirection = direction;
		if (direction == MD_WEST)
			status.state = ACM_WEST;
		else
			status.state = ACM_EAST;
	}
	return result;
}

static void set_mode(unsigned int newmode, int newtarget)
{
	status.mode = newmode;
	switch (status.mode) {
	case AC_STOP:
		motor_go(MD_STOP);
		nextstate = ACM_IDLE;
		status.state = ACM_STOPPED;
		timeout = jiffies + TIMEOUT;
		queue_delayed_work(actuator_workqueue, &actuator_work, SAMPLING_RATE);
		return;

	case AC_AUTO:
		status.target = newtarget;
		if (status.state == ACM_REACHED)
			status.state = ACM_STOPPED;
		if (status.target > status.position)
			nextstate = ACM_WEST;
		else if (status.target < status.position)
			nextstate = ACM_EAST;
		else
			nextstate = ACM_REACHED;
		break;

	case AC_MANUAL_WEST:
		nextstate = ACM_WEST;
		break;

	case AC_MANUAL_EAST:
		nextstate = ACM_EAST;
		break;
	}

	if ((status.state == ACM_EAST && nextstate == ACM_WEST)
	    || (status.state == ACM_WEST && nextstate == ACM_EAST)) {
		motor_go(MD_STOP);
		status.state = ACM_CHANGE;
		timeout = jiffies + TIMEOUT;
		queue_delayed_work(actuator_workqueue, &actuator_work, SAMPLING_RATE);
	} else if (status.state == ACM_IDLE) {
		status.state = nextstate;
		nextstate = ACM_IDLE;
		if (status.state == ACM_EAST)
			motor_go(MD_EAST);
		else if (status.state == ACM_WEST)
			motor_go(MD_WEST);
		else
			motor_go(MD_STOP);
		if (status.state != ACM_IDLE) {
			timeout = jiffies + TIMEOUT;
			queue_delayed_work(actuator_workqueue, &actuator_work, SAMPLING_RATE);
		}
	}
}

/* input polling in a workqueue */
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20) )
static void actuator_poll(void *dummy)
#else
static void actuator_poll(struct work_struct *dummy)
#endif
{
	int pulse;
	write_lock(&lock);

	pulse = check_pulse();
	if (pulse == 1 && pulse != oldpulse) {

		if (lastdirection == MD_WEST)
			status.position++;
		else if (lastdirection == MD_EAST)
			status.position--;
		if (lastdirection == MD_WEST || lastdirection == MD_EAST)
			timeout = jiffies + TIMEOUT;

		if (status.state == ACM_WEST || status.state == ACM_EAST) {
			if (((status.state == ACM_WEST
			      && status.position >= status.target)
			     || (status.state == ACM_EAST
				 && status.position <= status.target))
			    && status.mode == AC_AUTO) {
				status.state = ACM_REACHED;
				motor_go(MD_STOP);	/* stop the motor */
			}
		}
	}
	oldpulse = pulse;
	if (time_after_eq(jiffies, timeout)) {
		switch (status.state) {
		case ACM_WEST:
		case ACM_EAST:
			motor_go(MD_STOP);
			status.state = ACM_ERROR;
			lastdirection = MD_STOP;	//avoid spurious pulses
			timeout = jiffies + TIMEOUT;
			break;
		case ACM_ERROR:
			status.state = ACM_IDLE;
			break;
		case ACM_REACHED:
		case ACM_STOPPED:
		case ACM_CHANGE:
			status.state = nextstate;
			nextstate = ACM_IDLE;
			lastdirection = MD_STOP;	//if needed motor_go will change it
			if (status.state == ACM_EAST)
				motor_go(MD_EAST);
			else if (status.state == ACM_WEST)
				motor_go(MD_WEST);
			timeout = jiffies + TIMEOUT;
			break;
		}
	}
	if (status.state != ACM_IDLE)
		queue_delayed_work(actuator_workqueue, &actuator_work, SAMPLING_RATE);
	write_unlock(&lock);
}

/*
 *  Open device
 */

int actuator_open(struct inode *inode, struct file *filp)
{
	long result;

	if (opened)
		return -EBUSY;
	serport = filp_open(port, O_RDWR | O_NDELAY | O_NOCTTY, 0);
	if (IS_ERR(serport)) {
		printk(KERN_NOTICE "%s : could not open %s, error = %ld\n",
		       DRIVER_NAME, port, PTR_ERR(serport));
		return (-ENXIO);
	}
	result = motor_go(MD_STOP);
	if (result) {
		printk(KERN_WARNING "%s: can't issue ioctl\n", DRIVER_NAME);
		filp_close(serport, NULL);
		return result;
	}
	oldpulse = check_pulse();
	opened++;
	try_module_get(THIS_MODULE);
	return 0;
}

int actuator_release(struct inode *inode, struct file *filp)
{
	int mstate;

	/* wait for idle before closing */
	read_lock(&lock);
	mstate = status.state;
	read_unlock(&lock);
	if (mstate != ACM_IDLE) {
		write_lock(&lock);
		set_mode(AC_STOP, 0);
		write_unlock(&lock);
		while (mstate != ACM_IDLE) {
			msleep(100);
			read_lock(&lock);
			mstate = status.state;
			read_unlock(&lock);
		}
	}
	cancel_delayed_work(&actuator_work);
	flush_workqueue(actuator_workqueue);
	filp_close(serport, NULL);
	opened--;
	module_put(THIS_MODULE);
	return 0;
}

static long actuator_ioctl(struct file *filep,
			  unsigned int cmd, unsigned long arg)
{
	int result;
	int new;

	switch (cmd) {
	case AC_RSTATUS:
		read_lock(&lock);
		result = copy_to_user((void *)arg, &status, sizeof(status));
		read_unlock(&lock);
		if (result)
			return -EFAULT;
		break;

	case AC_WPOS:
		write_lock(&lock);
		set_mode(AC_STOP, 0);
		result = get_user(status.position, (unsigned int *)arg);
		write_unlock(&lock);
		if (result)
			return result;
		break;

	case AC_WTARGET:
		result = get_user(new, (int *)arg);
		if (result)
			return result;
		write_lock(&lock);
		set_mode(AC_AUTO, new);
		write_unlock(&lock);
		break;

	case AC_MSTOP:
		write_lock(&lock);
		set_mode(AC_STOP, 0);
		write_unlock(&lock);
		break;

	case AC_MWEST:
		write_lock(&lock);
		set_mode(AC_MANUAL_WEST, 0);
		write_unlock(&lock);
		break;

	case AC_MEAST:
		write_lock(&lock);
		set_mode(AC_MANUAL_EAST, 0);
		write_unlock(&lock);
		break;

	}
	return (0);
}

struct file_operations actuator_fops = {
open:	actuator_open,
release:actuator_release,
unlocked_ioctl:	actuator_ioctl,
};

/* Finally, init and cleanup */

int actuator_init(void)
{
	if (strlen(port) == 0) {
		printk(KERN_NOTICE "%s : please specify a serial port\n",
		       DRIVER_NAME);
		return (-ENXIO);
	}

	major = register_chrdev(0, DEV_NAME, &actuator_fops);
	if (major < 0) {
		printk(KERN_WARNING "%s: can't get major\n", DRIVER_NAME);
		return major;
	}

	actuator_class = class_create(THIS_MODULE, DRIVER_NAME);
	device_create(actuator_class, NULL, MKDEV(major, 0), NULL, DEV_NAME);

	/* set up workqueue */

	actuator_workqueue = create_singlethread_workqueue(DRIVER_NAME);

	/* set up spinlock */
	rwlock_init(&lock);

	return 0;
}

void actuator_cleanup(void)
{
	if (module_refcount(THIS_MODULE))
		return;

	destroy_workqueue(actuator_workqueue);

	device_destroy(actuator_class, MKDEV(major, 0));
	class_destroy(actuator_class);
	unregister_chrdev(major, DEV_NAME);
}

MODULE_AUTHOR("Luca Olivetti");
MODULE_DESCRIPTION("Control a linear actuator through a serial port");
MODULE_LICENSE("GPL");

module_init(actuator_init);
module_exit(actuator_cleanup);
