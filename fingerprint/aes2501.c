
/*
 * aes2501.c -- AuthenTec AES2501 Fingerprint Sensor Driver for Linux
 *
 * Maintainer: Cyrille Bagard <nocbos@gmail.com>
 *
 * Copyright (C) 2007 Cyrille Bagard
 *
 * This file is part of the AES2501 driver.
 *
 * the AES2501 driver is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * the AES2501 driver is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with the AES2501 driver; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */


#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/usb.h>
#include <linux/wait.h>
#include <linux/workqueue.h>
#include <linux/version.h>
#include <linux/vmalloc.h>
#include <asm/uaccess.h>


#include "aes2501.h"
#include "aes2501_regs.h"


static int major = 0;

module_param(major, int, 0);
MODULE_PARM_DESC(major, "major number");


//#define DEBUG


#define PRINTK_INFO(fmt, arg...) printk(KERN_INFO "aes2501: " fmt, ## arg)

#define PRINTK_CRIT(fmt, arg...) printk(KERN_CRIT "aes2501: " fmt, ## arg)

#define PRINTK_DBG(fmt, arg...) printk(KERN_DEBUG "aes2501: " fmt, ## arg)


static struct workqueue_struct *comm_queue;


static char *finger_print = NULL;
static size_t finger_print_len;



static DECLARE_WAIT_QUEUE_HEAD(wq);
static int flag = 0;



struct usb_aes2501 *_dev;






struct aes2501 {

	struct usb_device *udev;

	int temp;

	unsigned char *int_in_buffer;
	struct urb *int_in_urb;

};


/*
#define VENDOR_ID   0x08f7
#define PRODUCT_ID  0x0002
*/

#define VENDOR_ID   0x08ff
#define PRODUCT_ID  0x2580


/* Table of devices that work with this driver */

static struct usb_device_id id_table [] = {
    { USB_DEVICE(VENDOR_ID, PRODUCT_ID) },
    { }
};

MODULE_DEVICE_TABLE(usb, id_table);


/* Get a minor range for your devices from the usb maintainer */
#define USB_AES2501_MINOR_BASE	192



#define WRITES_IN_FLIGHT	8	/* ??? */



enum AES2501_Status {

	AESS_INIT_DONE,
	AESS_IS_SCANNING,

	AESS_COUNT

};


#define BULK_OUT_BUFFER_SIZE 32


/* Read a register value from a dump sent by the device. */
static int read_register_value(const uint8_t *, Aes2501Registers);

/* Tells if a finger motion is still detected. */
static int sum_histogram_values(const uint8_t *, uint8_t);


/* Structure to hold all device specific stuff */
struct usb_aes2501 {

	struct usb_device	*udev;					/* the usb device for this device */
	struct usb_interface	*interface;				/* the interface for this device */
	struct semaphore	limit_sem;				/* limiting the number of writes in progress */
	unsigned char		*bulk_in_buffer;			/* the buffer to receive data */
	size_t			bulk_in_size;				/* the size of the receive buffer */
	__u8			bulk_in_endpointAddr;			/* the address of the bulk in endpoint */
	uint8_t			bulk_out_buffer[BULK_OUT_BUFFER_SIZE];	/* the buffer to send data (32 / 8 = 4) */
	size_t			bulk_out_buffer_used;			/* the quantity of buffered data to send */
	__u8			bulk_out_endpointAddr;			/* the address of the bulk out endpoint */
	struct kref		kref;
	struct mutex		io_mutex;				/* synchronize I/O with disconnect */

	DECLARE_BITMAP(status, AESS_COUNT);				/* Information about what has been set / is running */

	struct work_struct scan_work;
	uint8_t			stop_scan;				/* Flag to stop finger detecting during module removal */
};


#define to_aes2501_dev(r) container_of(r, struct usb_aes2501, kref)

static struct usb_driver aes2501_driver;

static void aes2501_delete(struct kref *kref)
{
	struct usb_aes2501 *dev;

	dev = to_aes2501_dev(kref);

	usb_put_dev(dev->udev);
	kfree(dev->bulk_in_buffer);
	kfree(dev);

}


static int aes2501_open(struct inode *inode, struct file *file);
/*
struct usb_aes2501 *_dev;
static int aes2501_open(struct inode *inode, struct file *file)
{
	printk(KERN_INFO "Plop !\n");



	return 0;
}
*/

static ssize_t aes2501_read(struct file *file, char *buffer, size_t count, loff_t *ppos)
{
	int read_len;

	//printk(KERN_INFO "aes2501_read ! got %d -> need (%d ; %d)\n", finger_print_len, *ppos, count);

	if (flag == 0)
		printk(KERN_INFO "Process %i (%s) is going to sleep\n", current->pid, current->comm);

	wait_event_interruptible(wq, flag == 1);

	if (signal_pending(current))
		return -ERESTARTSYS;

	if (finger_print_len == 0)
		return -ENODATA;

	if (unlikely((*ppos < 0) || (loff_t)(*ppos + count) < 0))
		return -EINVAL;

	if (*ppos + count > finger_print_len)
		read_len = finger_print_len - *ppos;
	else read_len = count;

	if (read_len > 0)
	{
		if (!access_ok(VERIFY_WRITE, buffer, read_len))
			return -EFAULT;

		read_len -= copy_to_user(buffer, finger_print + *ppos, read_len);
		*ppos += read_len;

	}

	/* !!!! */
	else flag = 0;

	return read_len;

}



static int detect_finger_on_aes2501(struct usb_aes2501 *dev);

static void do_scanning(struct work_struct *work);


// ioctl - I/O control
static int aes2501_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	struct usb_aes2501 *dev;
	int retval;

	dev = _dev;


	printk(KERN_INFO "Get IOCtl :: cmd = %d vs %d\n", cmd, (AES2501_IOC_TEST));

	switch (cmd) {

		case AES2501_IOC_TEST:

#if defined(_arch_um__) || !(LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 20))
			INIT_WORK(&dev->scan_work, do_scanning);
#else
			INIT_WORK(&dev->scan_work, do_scanning, NULL);
#endif

			retval = queue_work(comm_queue, &dev->scan_work);


	}



	return 0;


#if 0
	int retval = 0;
	switch ( cmd ) {
		case CASE1:/* for writing data to arg */
			if (copy_from_user(&data, (int *)arg, sizeof(int)))
			return -EFAULT;
			break;
		case CASE2:/* for reading data from arg */
			if (copy_to_user((int *)arg, &data, sizeof(int)))
			return -EFAULT;
			break;
		default:
			retval = -EINVAL;
	}
	return retval;
#endif
}



static const struct file_operations aes2501_fops = {
	.owner =	THIS_MODULE,
	.read =		aes2501_read,/*
	.write =	aes2501_write,*/
	.open =		aes2501_open,/*
	.release =	skel_release,*/
	.ioctl =	aes2501_ioctl
};



/*
static int send_data_to_aes2501(struct usb_aes2501 *dev, unsigned char *buffer, int len)
{
#ifdef DEBUG
	int i;
#endif
	int bytes_written;
	int ret;

#ifdef DEBUG
	printk(KERN_INFO "\n:: OUT :: len=%d\n", len);

	for (i = 0; i < len; i++)
	{
		if (i > 0 && i % 16 == 0)
			printk("\n");
		if (i % 16 == 0)
			printk(KERN_INFO "  %05x: ", i);

		if (i % 8 == 0)
			printk(" ");

		printk("%02x ", buffer[i]);

	}

	printk("\n");
#endif

	ret = usb_bulk_msg(dev->udev,
			   usb_sndbulkpipe(dev->udev, dev->bulk_out_endpointAddr),
			   buffer,
			   len,
			   &bytes_written, 4000);

	if (ret < 0)
		PRINTK_CRIT("error while sending data: %d\n", ret);

	if (bytes_written != len)
		printk(KERN_INFO "did not write all !  %d vs %d\n", bytes_written, len);

	return ret;

}
*/

static int flush_aes2501_bulk_out(struct usb_aes2501 *dev)
{
	int ret;
	int bytes_written;

	ret = 0;

	if (dev->bulk_out_buffer_used > 0) {

		ret = usb_bulk_msg(dev->udev,
				   usb_sndbulkpipe(dev->udev, dev->bulk_out_endpointAddr),
				   dev->bulk_out_buffer,
				   dev->bulk_out_buffer_used,
				   &bytes_written, 4000);

		if (ret < 0)
			PRINTK_CRIT("error while sending data: %d\n", ret);

		else if (bytes_written != dev->bulk_out_buffer_used)
		{
			printk(KERN_INFO "did not write all !  %d vs %d\n", bytes_written, dev->bulk_out_buffer_used);
			/*ret = ... */
		}

		dev->bulk_out_buffer_used = 0;

	}

	return ret;

}


static int write_aes2501_register(struct usb_aes2501 *dev, uint8_t reg, uint8_t data)
{
	int ret;

	if (dev->bulk_out_buffer_used == BULK_OUT_BUFFER_SIZE)
		ret = flush_aes2501_bulk_out(dev);
	else
		ret = 0;

	dev->bulk_out_buffer[dev->bulk_out_buffer_used++] = reg;
	dev->bulk_out_buffer[dev->bulk_out_buffer_used++] = data;

	return ret;

}


static int recv_data_to_aes2501(struct usb_aes2501 *dev, unsigned char *buffer, int len)
{
	int bytes_read;
	int ret;
#ifdef DEBUG
	int i;
#endif

	ret = usb_bulk_msg(dev->udev,
			   usb_rcvbulkpipe(dev->udev, dev->bulk_in_endpointAddr),
			   buffer,
			   len,
			   &bytes_read, 4000);

	if (ret < 0)
		PRINTK_CRIT("error while sending data: %d\n", ret);

	if (bytes_read != len)
		printk(KERN_INFO "did not read all !  %d vs %d\n", bytes_read, len);

#ifdef DEBUG
	if (ret == 0)
	{
		printk(KERN_INFO "\n:: IN :: len=%d\n", len);

		for (i = 0; i < len; i++)
		{
			if (i > 0 && i % 16 == 0)
				printk("\n");
			if (i % 16 == 0)
				printk(KERN_INFO "  %05x: ", i);

			if (i % 8 == 0)
				printk(" ");

			printk("%02x ", buffer[i]);

		}

		printk("\n");

	}
#endif

	return ret;

}



static int standby_aes2501(struct usb_aes2501 *dev)
{

	//unsigned char cmd[] = "\xac\x01\xad\x1a\x81\x02";


	unsigned char buffer[4];




	printk(KERN_INFO ">> Standby aes2501 !\n");


	//msleep(800);

	write_aes2501_register(dev, AES2501_REG_TREGC, AES2501_TREGC_ENABLE);
	write_aes2501_register(dev, AES2501_REG_TREGD, 0x1a);

	flush_aes2501_bulk_out(dev);

	/**
	 * TODO:
	 * - lire ici les deux octets.
	 * - voir pour inverser les commandes.
	 */

	write_aes2501_register(dev, AES2501_REG_CTRL2, 0x02/* AES2501_CTRL2_READ_REGS ??? */);
	flush_aes2501_bulk_out(dev);

	//send_data_to_aes2501(dev, cmd, 6);
	recv_data_to_aes2501(dev, buffer, 2);



	printk(KERN_INFO "  %02x %02x\n", buffer[0], buffer[1]);


	if ((buffer[1] & AES2501_STAT_SCAN) == buffer[1])
	{
		PRINTK_DBG("scan state: ");

		switch (buffer[1])
		{
			case STATE_WAITING_FOR_FINGER:
				printk("'Waiting for finger'\n");
				break;
			case STATE_FINGER_SETTLING_DELAY:
				printk("'In Finger settling delay'\n");
				break;
			case STATE_POWER_UP_DELAY:
				printk("'In power up delay'\n");
				break;
			case STATE_WAITING_TO_START_SCAN:
				printk("'Waiting to start image scan'\n");
				break;
			case STATE_PRELOADING_SUBARRAY_0:
				printk("'Pre-loading subarray 0'\n");
				break;
			case STATE_SETUP_FOR_ROW_ADVANCE:
				printk("'Setup for row advance'\n");
				break;
			case STATE_WAITING_FOR_ROW_ADVANCE:
				printk("'Waiting for row advance'\n");
				break;
			case STATE_PRELOADING_COL_0:
				printk("'Pre-loading column 0'\n");
				break;
			case STATE_SETUP_FOR_COL_ADVANCE:
				printk("'Setup for column advance'\n");
				break;
			case STATE_WAITING_FOR_COL_ADVANCE:
				printk("'Waiting for column advance'\n");
				break;
			case STATE_WAITING_FOR_SCAN_START:
				printk("'Waiting for scan start'\n");
				break;
			case STATE_WAITING_FOR_SCAN_END:
				printk("'Waiting for scan end'\n");
				break;
			case STATE_WAITING_FOR_ROW_SETUP:
				printk("'Waiting for row setup'\n");
				break;
			case STATE_WAITING_FOR_COL_TIME:
				printk("'Waiting for one column time (depends on scan rate)'\n");
				break;
			case STATE_WAITING_FOR_QUEUED_DATA:
				printk("'Waiting for queued data transmission to be completed'\n");
				break;
			case STATE_WAIT_FOR_128_US:
				printk("'Wait for 128 us'\n");
				break;
			default:
				printk("none (?!)\n");
				break;

		}

	}

	if (buffer[1] & AES2501_STAT_ERROR)
		PRINTK_DBG("got serial interface framing error\n");

	if (buffer[1] & AES2501_STAT_PAUSED)
		PRINTK_DBG("scan was paused due to input buffer full\n");

	if (buffer[1] & AES2501_STAT_RESET)
		PRINTK_DBG("master reset input has been asserted\n");




	return 0;


}




static int setup_aes2501(struct usb_aes2501 *dev)
{
	//unsigned char patch_msg[] = "\x80\x01";


	//unsigned char cmd[] = "\x80\x01\x81\x02";

	//unsigned char cmd_00[] = "\xb0\x27";
	//unsigned char cmd_01[] = "\x80\x01\x82\x40";
	//unsigned char cmd_02[] = "\xff\x00";
	//unsigned char cmd_03[] = "\x80\x01\x82\x40\x83\x00\x88\x02\x89\x10\x8a\x05\x8c\x00\x8e\x13\x91\x44\x92\x34\x95\x16\x96\x16\x97\x18\xa1\x70\xa2\x02\xa7\x00\xac\x01\xad\x1a\x80\x04\x81\x04\xb4\x00";/*42*/
	//unsigned char cmd_04[] = "\x80\x01\x82\x40";

	//unsigned char cmd2[] = "\x80\x01\xa8\x41\x82\x42\x83\x53\x80\x04\x81\x02";/*12*/

	//unsigned char cmd3_00[] = "\xff\x00";
	//unsigned char cmd3_01[] = "\x80\x01\xa8\x41\x82\x42\x83\x53\x80\x04\x81\x02";/*12*/

	//unsigned char cmd4[] = "\x80\x01\x82\x40\xb0\x27\x94\x0a\x80\x04\x83\x45\xa8\x41";/*14*/

	//unsigned char cmd5_00[] = "\xb0\x27";
	//unsigned char cmd5_01[] = "\x80\x01\x82\x40";
	//unsigned char cmd5_02[] = "\xff\x00";
	//unsigned char cmd5_03[] = "\x80\x02";
	//unsigned char cmd5_04[] = "\x81\x02";


	int i;

	unsigned char buffer[128];


	/* To be sure the device will respond */
	write_aes2501_register(dev, AES2501_REG_CTRL1, AES2501_CTRL1_MASTER_RESET);
	//send_data_to_aes2501(dev, patch_msg, 2);
	flush_aes2501_bulk_out(dev);/* ... */

	printk(KERN_INFO ">> Setting up aes2501 !\n");

	/* Part 1 */

	printk(KERN_INFO " -- part 1 --\n");

	write_aes2501_register(dev, AES2501_REG_CTRL1, AES2501_CTRL1_MASTER_RESET);
	write_aes2501_register(dev, AES2501_REG_CTRL2, AES2501_CTRL2_READ_REGS);
	flush_aes2501_bulk_out(dev);

	//send_data_to_aes2501(dev, cmd, 4);
	recv_data_to_aes2501(dev, buffer, 126);



	//send_data_to_aes2501(dev, cmd_00, 2);

	write_aes2501_register(dev, 0xb0, 0x27);/* Reserved */



	//send_data_to_aes2501(dev, cmd_01, 4);

	write_aes2501_register(dev, AES2501_REG_CTRL1, AES2501_CTRL1_MASTER_RESET);
	write_aes2501_register(dev, AES2501_REG_EXCITCTRL, 0x40);/* ??? */

	for (i = 0; i <= 10; i++)
	{
		//msleep(200);
		//send_data_to_aes2501(dev, cmd_02, 2);
		write_aes2501_register(dev, 0xff, 0x00);/* Reserved */
	}

	//msleep(100);


	//send_data_to_aes2501(dev, cmd_03, 42);

	write_aes2501_register(dev, AES2501_REG_CTRL1, AES2501_CTRL1_MASTER_RESET);
	write_aes2501_register(dev, AES2501_REG_EXCITCTRL, 0x40);/* ??? */
	write_aes2501_register(dev, AES2501_REG_DETCTRL, AES2501_DETCTRL_DRATE_CONTINUOUS | AES2501_DETCTRL_SDELAY_31_MS);
	write_aes2501_register(dev, AES2501_REG_COLSCAN, AES2501_COLSCAN_SRATE_128_US);
	write_aes2501_register(dev, AES2501_REG_MEASDRV, AES2501_MEASDRV_MDRIVE_0_325 | AES2501_MEASDRV_MEASURE_SQUARE);
	write_aes2501_register(dev, AES2501_REG_MEASFREQ, AES2501_MEASFREQ_2M);
	write_aes2501_register(dev, AES2501_REG_DEMODPHASE1, DEMODPHASE_NONE);/* Default */
	write_aes2501_register(dev, AES2501_REG_DEMODPHASE2, DEMODPHASE_NONE);
	write_aes2501_register(dev, AES2501_REG_CHANGAIN, AES2501_CHANGAIN_STAGE2_4X | AES2501_CHANGAIN_STAGE1_16X);
	write_aes2501_register(dev, AES2501_REG_ADREFHI, 0x44);
	write_aes2501_register(dev, AES2501_REG_ADREFLO, 0x34);
	write_aes2501_register(dev, AES2501_REG_STRTCOL, 0x16);
	write_aes2501_register(dev, AES2501_REG_ENDCOL, 0x16);
	write_aes2501_register(dev, AES2501_REG_DATFMT, AES2501_DATFMT_BIN_IMG | 0x08);/* ??? */
	write_aes2501_register(dev, AES2501_REG_TREG1, 0x70);/* Reserved + 0xXX */
	write_aes2501_register(dev, 0xa2, 0x02);/* Reserved */
	write_aes2501_register(dev, 0xa7, 0x00);/* Reserved */
	write_aes2501_register(dev, AES2501_REG_TREGC, AES2501_TREGC_ENABLE);
	write_aes2501_register(dev, AES2501_REG_TREGD, 0x1a);
	write_aes2501_register(dev, AES2501_REG_CTRL1, AES2501_CTRL1_REG_UPDATE);
	write_aes2501_register(dev, AES2501_REG_CTRL2, AES2501_CTRL2_SET_ONE_SHOT);
	write_aes2501_register(dev, AES2501_REG_LPONT, AES2501_LPONT_MIN_VALUE);


	flush_aes2501_bulk_out(dev);

	recv_data_to_aes2501(dev, buffer, 20);



	//send_data_to_aes2501(dev, cmd_04, 4);

	write_aes2501_register(dev, AES2501_REG_CTRL1, AES2501_CTRL1_MASTER_RESET);
	write_aes2501_register(dev, AES2501_REG_EXCITCTRL, 0x40);/* ??? */

	/* Part 2 */

	printk(KERN_INFO " -- part 2 --\n");

	//msleep(100);

	//send_data_to_aes2501(dev, cmd2, 12);

	write_aes2501_register(dev, AES2501_REG_CTRL1, AES2501_CTRL1_MASTER_RESET);
	write_aes2501_register(dev, AES2501_REG_AUTOCALOFFSET, 0x41);
	write_aes2501_register(dev, AES2501_REG_EXCITCTRL, 0x42);/* ??? */
	write_aes2501_register(dev, AES2501_REG_DETCTRL, 0x53);/* Cumul ??? */
	write_aes2501_register(dev, AES2501_REG_CTRL1, AES2501_CTRL1_REG_UPDATE);
	write_aes2501_register(dev, AES2501_REG_CTRL2, AES2501_CTRL2_READ_REGS);




	flush_aes2501_bulk_out(dev);


	recv_data_to_aes2501(dev, buffer, 126);

	/* Part 3 */

	printk(KERN_INFO " -- part 3 --\n");

	i = 0;

	printk(KERN_INFO " reg 0xaf = 0x%x\n", buffer[0x5f]);
	while (buffer[0x5f] == 0x6b)
	{
		//msleep(200);

	
		//send_data_to_aes2501(dev, cmd3_00, 2);
		write_aes2501_register(dev, 0xff, 0x00);/* Reserved */

		//msleep(80);

		//send_data_to_aes2501(dev, cmd3_01, 12);


		write_aes2501_register(dev, AES2501_REG_CTRL1, AES2501_CTRL1_MASTER_RESET);
		write_aes2501_register(dev, AES2501_REG_AUTOCALOFFSET, 0x41);
		write_aes2501_register(dev, AES2501_REG_EXCITCTRL, 0x42);/* ??? */
		write_aes2501_register(dev, AES2501_REG_DETCTRL, 0x53);/* Cumul ??? */
		write_aes2501_register(dev, AES2501_REG_CTRL1, AES2501_CTRL1_REG_UPDATE);
		write_aes2501_register(dev, AES2501_REG_CTRL2, AES2501_CTRL2_READ_REGS);

		flush_aes2501_bulk_out(dev);




		recv_data_to_aes2501(dev, buffer, 126);

		printk(KERN_INFO " +reg 0xaf = 0x%x\n", buffer[0x5f]);

		if (++i == 13) break;

	}





	/* Part 4 */

	printk(KERN_INFO " -- part 4 --\n");

	//send_data_to_aes2501(dev, cmd4, 14);

	write_aes2501_register(dev, AES2501_REG_CTRL1, AES2501_CTRL1_MASTER_RESET);
	write_aes2501_register(dev, AES2501_REG_EXCITCTRL, 0x40);/* ??? */
	write_aes2501_register(dev, 0xb0, 0x27);/* Reserved */
	write_aes2501_register(dev, AES2501_REG_ENDROW, 0x0a);
	write_aes2501_register(dev, AES2501_REG_CTRL1, AES2501_CTRL1_REG_UPDATE);
	write_aes2501_register(dev, AES2501_REG_DETCTRL, 0x45);/* ???? */
	write_aes2501_register(dev, AES2501_REG_AUTOCALOFFSET, 0x41);






	/* ... */


/*
	for (i = 0; i < 126; i += 10)
	{
		printk(KERN_INFO " %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
		       buffer[i + 0], buffer[i + 1], buffer[i + 2], buffer[i + 3], buffer[i + 4],
		       buffer[i + 5], buffer[i + 6], buffer[i + 7], buffer[i + 8], buffer[i + 9]);

		if (i % 20 == 0)
			printk(KERN_INFO "\n  ");

	}

	printk(KERN_INFO "\n");
*/



	/* Part 5 */

	printk(KERN_INFO " -- part 5 --\n");

	//send_data_to_aes2501(dev, cmd5_00, 2);

	write_aes2501_register(dev, 0xb0, 0x27);/* Reserved */

	//send_data_to_aes2501(dev, cmd5_01, 4);

	write_aes2501_register(dev, AES2501_REG_CTRL1, AES2501_CTRL1_MASTER_RESET);
	write_aes2501_register(dev, AES2501_REG_EXCITCTRL, 0x40);/* ??? */

	//msleep(200);


	//send_data_to_aes2501(dev, cmd5_02, 2);
	//send_data_to_aes2501(dev, cmd5_01, 4);
	//send_data_to_aes2501(dev, cmd5_01, 4);

	write_aes2501_register(dev, 0xff, 0x00);/* Reserved */
	write_aes2501_register(dev, AES2501_REG_CTRL1, AES2501_CTRL1_MASTER_RESET);
	write_aes2501_register(dev, AES2501_REG_EXCITCTRL, 0x40);/* ??? */
	write_aes2501_register(dev, AES2501_REG_CTRL1, AES2501_CTRL1_MASTER_RESET);
	write_aes2501_register(dev, AES2501_REG_EXCITCTRL, 0x40);/* ??? */



	//msleep(50);

	//send_data_to_aes2501(dev, cmd5_01, 4);
	//send_data_to_aes2501(dev, cmd5_01, 4);
	//send_data_to_aes2501(dev, cmd5_01, 4);
	//send_data_to_aes2501(dev, cmd5_03, 2);
	//send_data_to_aes2501(dev, cmd5_03, 2);
	//send_data_to_aes2501(dev, cmd5_04, 2);

	write_aes2501_register(dev, AES2501_REG_CTRL1, AES2501_CTRL1_MASTER_RESET);
	write_aes2501_register(dev, AES2501_REG_EXCITCTRL, 0x40);/* ??? */
	write_aes2501_register(dev, AES2501_REG_CTRL1, AES2501_CTRL1_MASTER_RESET);
	write_aes2501_register(dev, AES2501_REG_EXCITCTRL, 0x40);/* ??? */
	write_aes2501_register(dev, AES2501_REG_CTRL1, AES2501_CTRL1_MASTER_RESET);
	write_aes2501_register(dev, AES2501_REG_EXCITCTRL, 0x40);/* ??? */
	write_aes2501_register(dev, AES2501_REG_CTRL1, AES2501_CTRL1_SCAN_RESET);
	write_aes2501_register(dev, AES2501_REG_CTRL1, AES2501_CTRL1_SCAN_RESET);
	write_aes2501_register(dev, AES2501_REG_CTRL2, AES2501_CTRL2_READ_REGS);
	flush_aes2501_bulk_out(dev);


	recv_data_to_aes2501(dev, buffer, 126);





	return 0;

}

static int detect_finger_on_aes2501(struct usb_aes2501 *dev)
{
	//uint8_t cmd1[] = "\x80\x01\x82\x40";
	//uint8_t cmd2[] = "\x80\x01\x82\x40\x83\x00\x88\x02\x89\x10\x8a\x05\x8c\x00\x8e\x13\x91\x44\x92\x34\x95\x16\x96\x16\x97\x18\xa1\x70\xa2\x02\xa7\x00\xac\x01\xad\x1a\x80\x04\x81\x04\xb4\x00";/*42*/


	unsigned char buffer[22];
	unsigned i, sum;

	//send_data_to_aes2501(dev, cmd1, 4);

	//msleep(30);



	write_aes2501_register(dev, AES2501_REG_CTRL1, AES2501_CTRL1_MASTER_RESET);
	write_aes2501_register(dev, AES2501_REG_EXCITCTRL, 0x40);/* Reserved */
	write_aes2501_register(dev, AES2501_REG_DETCTRL, AES2501_DETCTRL_DRATE_CONTINUOUS | AES2501_DETCTRL_SDELAY_31_MS);
	write_aes2501_register(dev, AES2501_REG_COLSCAN, AES2501_COLSCAN_SRATE_128_US);/* not the faster ??? */
	write_aes2501_register(dev, AES2501_REG_MEASDRV, AES2501_MEASDRV_MDRIVE_0_325 | AES2501_MEASDRV_MEASURE_SQUARE);
	write_aes2501_register(dev, AES2501_REG_MEASFREQ, AES2501_MEASFREQ_2M);
	write_aes2501_register(dev, AES2501_REG_DEMODPHASE1, DEMODPHASE_NONE);/* Default */
	write_aes2501_register(dev, AES2501_REG_DEMODPHASE2, DEMODPHASE_NONE);
	write_aes2501_register(dev, AES2501_REG_CHANGAIN, AES2501_CHANGAIN_STAGE2_4X | AES2501_CHANGAIN_STAGE1_16X);
	write_aes2501_register(dev, AES2501_REG_ADREFHI, 0x44);
	write_aes2501_register(dev, AES2501_REG_ADREFLO, 0x34);
	write_aes2501_register(dev, AES2501_REG_STRTCOL, 0x16);
	write_aes2501_register(dev, AES2501_REG_ENDCOL, 0x16);
	write_aes2501_register(dev, AES2501_REG_DATFMT, AES2501_DATFMT_BIN_IMG | 0x08);
	write_aes2501_register(dev, AES2501_REG_TREG1, 0x70);/* Reserved + 0xXX */
	write_aes2501_register(dev, 0xa2, 0x02);/* Reserved */
	write_aes2501_register(dev, 0xa7, 0x00);/* Reserved */
	write_aes2501_register(dev, AES2501_REG_TREGC, AES2501_TREGC_ENABLE);
	write_aes2501_register(dev, AES2501_REG_TREGD, 0x1a);

	flush_aes2501_bulk_out(dev);

	/**
	 * TODO:
	 * - lire ici les deux octets.
	 * - voir pour inverser les commandes.
	 */

	write_aes2501_register(dev, AES2501_REG_CTRL1, AES2501_CTRL1_REG_UPDATE);
	write_aes2501_register(dev, AES2501_REG_CTRL2, AES2501_CTRL2_SET_ONE_SHOT);
	write_aes2501_register(dev, AES2501_REG_LPONT, AES2501_LPONT_MIN_VALUE);

	flush_aes2501_bulk_out(dev);


	//send_data_to_aes2501(dev, cmd2, 42);

	recv_data_to_aes2501(dev, buffer, 20);


	/* One column is returned here but I don't know which one.
	   Maybe an average over the whole sensor area. */
	sum = 0;
	for (i = 1; i != 9; i++) {
		sum += (buffer[i] & 0xf) + (buffer[i] >> 4);
	}


	//printk(KERN_INFO "===finger=== sum = %d vs %d\n", sum, 20);


	/*
	  return (sum > aes->fingerTh1);
	*/

	return sum > 20;

}


static unsigned read_fingerprint_on_aes2501(struct usb_aes2501 *dev, void *raw_, unsigned maxstrip)
{
	//unsigned char patch_msg[] = "\x80\x01";

    /* 8e xx = set gain */
    //uint8_t cmd1_00[] = "0x80\x01\x82\x40";
    //uint8_t cmd1_01[] = "0x80\x01\x82\x40\x83\x00\x88\x02\x8c\x7c\x89\x10\x8d\x24\x9b\x00\x9c\x6c\x9d\x09\x9e\x54\x9f\x78\xa2\x02\xa7\x00\xb6\x26\xb7\x1a\x80\x04\x98\x23\x95\x10\x96\x1f\x8e\x00\x91\x70\x92\x20\x81\x04\xb4\x00";/*50*/

    //uint8_t cmd2[] = "\x98\x23\x95\x10\x96\x1f\x8e\x03\x91\x70\x92\x20\x81\x04";/*14*/

    //uint8_t cmd3[] = "\x98\x22\x95\x00\x96\x2f\x8e\x03\x91\x5b\x92\x20\x81\x04";/*14*/

    uint8_t buf[1705];
    uint8_t *raw = (uint8_t *) raw_;
    unsigned nstrips;

	int threshold;
	int sum;


	//send_data_to_aes2501(dev, patch_msg, 2);
	write_aes2501_register(dev, AES2501_REG_CTRL1, AES2501_CTRL1_MASTER_RESET);
	flush_aes2501_bulk_out(dev);



	//send_data_to_aes2501(dev, cmd1_00, 4);


	//send_data_to_aes2501(dev, cmd1_01, 50);

	write_aes2501_register(dev, AES2501_REG_CTRL1, AES2501_CTRL1_MASTER_RESET);
	write_aes2501_register(dev, AES2501_REG_EXCITCTRL, 0x40);/* Reserved */
	write_aes2501_register(dev, AES2501_REG_DETCTRL, AES2501_DETCTRL_SDELAY_31_MS | AES2501_DETCTRL_DRATE_CONTINUOUS);
	write_aes2501_register(dev, AES2501_REG_COLSCAN, AES2501_COLSCAN_SRATE_128_US);
	write_aes2501_register(dev, AES2501_REG_DEMODPHASE2, 0x7c);/* ... */
	write_aes2501_register(dev, AES2501_REG_MEASDRV, AES2501_MEASDRV_MEASURE_SQUARE | AES2501_MEASDRV_MDRIVE_0_325);
	write_aes2501_register(dev, AES2501_REG_DEMODPHASE1, 0x24);/* ... */
	write_aes2501_register(dev, AES2501_REG_CHWORD1, 0x00);/* Challenge Word */
	write_aes2501_register(dev, AES2501_REG_CHWORD2, 0x6c);/* Challenge Word */
	write_aes2501_register(dev, AES2501_REG_CHWORD3, 0x09);/* Challenge Word */
	write_aes2501_register(dev, AES2501_REG_CHWORD4, 0x54);/* Challenge Word */
	write_aes2501_register(dev, AES2501_REG_CHWORD5, 0x78);/* Challenge Word */
	write_aes2501_register(dev, 0xa2, 0x02);/* Reserved */
	write_aes2501_register(dev, 0xa7, 0x00);/* Reserved */
	write_aes2501_register(dev, 0xb6, 0x26);/* Reserved */
	write_aes2501_register(dev, 0xb7, 0x1a);/* Reserved */
	write_aes2501_register(dev, AES2501_REG_CTRL1, AES2501_CTRL1_REG_UPDATE);
	write_aes2501_register(dev, AES2501_REG_IMAGCTRL, AES2501_IMAGCTRL_TST_REG_ENABLE | AES2501_IMAGCTRL_HISTO_DATA_ENABLE | AES2501_IMAGCTRL_IMG_DATA_DISABLE);
	write_aes2501_register(dev, AES2501_REG_STRTCOL, 0x10);
	write_aes2501_register(dev, AES2501_REG_ENDCOL, 0x1f);
	write_aes2501_register(dev, AES2501_REG_CHANGAIN, AES2501_CHANGAIN_STAGE1_2X | AES2501_CHANGAIN_STAGE2_2X);
	write_aes2501_register(dev, AES2501_REG_ADREFHI, 0x70);
	write_aes2501_register(dev, AES2501_REG_ADREFLO, 0x20);
	write_aes2501_register(dev, AES2501_REG_CTRL2, AES2501_CTRL2_SET_ONE_SHOT);
	write_aes2501_register(dev, AES2501_REG_LPONT, AES2501_LPONT_MIN_VALUE);

    ///////recv_data_to_aes2501(dev, buf, 159);
    /* TODO: calibration e.g setting gain (0x8e xx) */




    //send_data_to_aes2501(dev, cmd2, 14);



	write_aes2501_register(dev, AES2501_REG_IMAGCTRL, AES2501_IMAGCTRL_TST_REG_ENABLE | AES2501_IMAGCTRL_HISTO_DATA_ENABLE | AES2501_IMAGCTRL_IMG_DATA_DISABLE);
	write_aes2501_register(dev, AES2501_REG_STRTCOL, 0x10);
	write_aes2501_register(dev, AES2501_REG_ENDCOL, 0x1f);
	write_aes2501_register(dev, AES2501_REG_CHANGAIN, AES2501_CHANGAIN_STAGE1_16X);
	write_aes2501_register(dev, AES2501_REG_ADREFHI, 0x70);
	write_aes2501_register(dev, AES2501_REG_ADREFLO, 0x20);
	write_aes2501_register(dev, AES2501_REG_CTRL2, AES2501_CTRL2_SET_ONE_SHOT);

	flush_aes2501_bulk_out(dev);


    recv_data_to_aes2501(dev, buf, 159);

    nstrips = 0;
    do {
	    printk(KERN_INFO "-start-\n");
        /* Timing in this loop is critical. It decides how fast you can move your finger.
           If one loop takes tl second, the maximum speed is:
               (16/500 * 25.4) / tl    [mm per sec]
         */
	//send_data_to_aes2501(dev, cmd3, 14);

	write_aes2501_register(dev, AES2501_REG_IMAGCTRL, AES2501_IMAGCTRL_TST_REG_ENABLE | AES2501_IMAGCTRL_HISTO_DATA_ENABLE);
	write_aes2501_register(dev, AES2501_REG_STRTCOL, 0x00);
	write_aes2501_register(dev, AES2501_REG_ENDCOL, 0x2f);
	write_aes2501_register(dev, AES2501_REG_CHANGAIN, AES2501_CHANGAIN_STAGE1_16X);
	write_aes2501_register(dev, AES2501_REG_ADREFHI, 0x5b);
	write_aes2501_register(dev, AES2501_REG_ADREFLO, 0x20);
	write_aes2501_register(dev, AES2501_REG_CTRL2, AES2501_CTRL2_SET_ONE_SHOT);

	flush_aes2501_bulk_out(dev);



	recv_data_to_aes2501(dev, buf, 1705);
	memcpy(raw, buf+1, 192*8);
	raw += 192*8;

	    printk(KERN_INFO "-end of copy-\n");


	    threshold = read_register_value((buf + 1 + 192*8 + 1 + 16*2 + 1 + 8), _AES2501_REG_DATFMT);
	    /*if (threshold < 0)
		    return threshold;*/

	    threshold &= 0x0f;

	    sum = sum_histogram_values((buf + 1 + 192*8), threshold);
	    /*if (threshold < 0)
		    return threshold;*/

	nstrips++;
	    printk(KERN_INFO "-end- %d vs %d\n", nstrips, maxstrip);
    } while (sum > 0 && nstrips < maxstrip);
    printk(KERN_INFO "nstrips = %u\n", nstrips);
    if (nstrips == maxstrip)
	printk(KERN_INFO "nstrips == %u, swiping the finger too slowly?\n", maxstrip);
    return nstrips;
}



/**
 * Read a register value from a dump sent by the device.
 * @data: list of couples <register, value>.
 * @target: index of the register to process.
 * @return: -EILSEQ,-EINVAL if error, value > 0 if a finger is here.
 */
int read_register_value(const uint8_t *data, Aes2501Registers target)
{
	int result;
	uint8_t offset;

	if (*data == FIRST_AES2501_REG)
	{
		offset = target;

		if (!(FIRST_AES2501_REG <= offset && offset <= LAST_AES2501_REG))
			result = -EINVAL;

		else
		{
			offset -= FIRST_AES2501_REG;
			offset *= 2;

			result = data[++offset];

		}

	}
	else result = -EILSEQ;

	return result;

}


/**
 * Tells if a finger motion is still detected.
 * @data: start point of data to handle, preceded by 0xde.
 * @threshold: index of values set to 1 by the device.
 * @return: -EILSEQ,-EINVAL if error, value > 0 if a finger is here.
 *
 * Histograms always are a 16-uint16_t long message, where
 * histogram[i] = number of pixels of value i.
 */
int sum_histogram_values(const uint8_t *data, uint8_t threshold)
{
	int result;
	uint16_t *histogram;
	uint8_t i;

	if (*data == 0xde)
	{
		if (threshold > 0x0f)
			result = -EINVAL;

		else
		{
			result = 0;
			histogram = (uint16_t *)(data + 1);

			for (i = threshold; i < 16; i++)
				result += histogram[i];

		}

	}
	else result = -EILSEQ;

	return result;

}



/**********************************************************
 ** Image processing
 **********************************************************/

/* This function finds overlapping parts of two frames
 * Based on calculating normalized hamming distance
 * between two frames
 */

static unsigned find_overlap(const uint8_t *first_frame,
			     const uint8_t *second_frame,
			     uint32_t frame_height,
			     uint32_t frame_width,
			     uint32_t *min_error)
{
	uint32_t dy, i;
	uint32_t error, not_overlapped_height = 0;
 	*min_error = 255 * frame_height * frame_width;
	for (dy = 0; dy < frame_height; dy++) {
		/* Calculating difference (error) between parts of frames */
		error = 0;
		for (i = 0; i < frame_width * (frame_height - dy); i++) {
			/* Using ? operator to avoid abs function */
			error += first_frame[i] > second_frame[i] ? 
					(first_frame[i] - second_frame[i]) :
					(second_frame[i] - first_frame[i]); 
		}
		
		/* Normalizing error */
		error *= 15;
		error /= i;
		if (error < *min_error) {
			*min_error = error;
			not_overlapped_height = dy;
		}
		first_frame += frame_width;
	}
	
	return not_overlapped_height; 
}


/* This function assembles frames to single image, returns image height
 * TODO: add Doxygen comments
 */
static unsigned assemble(const uint8_t *input, /* Raw data received from device */
						 uint8_t *output, /* Output buffer */
						 uint32_t frame_height, /* Height of each frame */
						 uint32_t frame_width,  /* Width of each frame */
						 uint32_t frames_count, /* Frames count */
						 /*int overlap, */ 
						 uint8_t reversed, 
						 uint32_t *errors_sum
						)
{
	uint8_t *assembled = output;
	uint32_t frame, column, row, image_height = frame_height;
	uint32_t error, not_overlapped; 
	
	*errors_sum = 0;
	
	if (frames_count < 1) return 0;
	
	/* Rotating given data by 90 degrees 
	 * Taken from document describing aes2501 image format
	 * TODO: move reversing detection here */
	if (reversed) {
	    output += (frames_count - 1) * frame_width * frame_height;
	}
	
	for (frame = 0; frame < frames_count; frame++) {
	    for (column = 0; column < frame_width; column++) {
		for (row = 0; row < (frame_height / 2); row++) {
				output[frame_width * ( 2 * row) + column] = *input & 0x0F;
				output[frame_width * ( 2 * row + 1) + column] = *input >> 4;
				input++;
			}
		}
		
		if (reversed) {
		    output -= frame_width * frame_height;
		}
		else {
		    output += frame_width * frame_height;
		}
	}

	/* Detecting where frames overlaped */
	output = assembled;
	for (frame = 1; frame < frames_count; frame++)
	{
		output += frame_width * frame_height;
		not_overlapped = find_overlap(assembled, output, frame_height, frame_width, &error);
		*errors_sum += error;
		image_height += not_overlapped;
		assembled += frame_width * not_overlapped;
		memcpy(assembled, output, frame_width * frame_height); 
	} 

	return image_height;
}



static void store_new_aes2501_pnm(const uint8_t *data, unsigned int width, unsigned int height)
{
	char header[20];
	__kernel_size_t header_len;
	int i;
	char *iter;

	sprintf(header, "P5\n%u %u\n255\n", width, height);
	header_len = strlen(header);

	finger_print_len = header_len + width * height;
	finger_print = kzalloc(finger_print_len, GFP_KERNEL);

	memcpy(finger_print, header, header_len);

	for (i = 0, iter = &finger_print[header_len]; i < width * height; i++, iter++)
		*iter = (data[i] << 4) + 0x0f;

}



static void do_scanning(struct work_struct *work)
{
	struct usb_aes2501 *dev;

	/* !!!!!!!!!!!!!!!!!!!!!!!!!!!!! */

	uint8_t *raw, *cooked;
	unsigned swidth = 192;
	unsigned sheight = 16;
	unsigned maxstrip = 150;
	unsigned nstrips, height, mindiff_sum, mindiff_sum_r;

	/* !!!!!!!!!!!!!!!!!!!!!!!!!!!!! */

        dev = container_of(work, struct usb_aes2501, scan_work);



	


	setup_aes2501(dev);

	while (!detect_finger_on_aes2501(dev))
	{
	    if (dev->stop_scan)
	    {
		PRINTK_INFO("aborting scan due to module removal\n");
		return;
	    }
	};

	/* We don't need physical continuity of allocated memory */
	raw = (uint8_t *)vmalloc((3 * maxstrip * sheight * swidth) / 2);
	if (raw == NULL)
	{
	    PRINTK_CRIT("failed to allocate memory\n");
	    return;
	}
	
	cooked = raw + (maxstrip * sheight * swidth)/2;



	printk(KERN_INFO "Raw is 0x%p\n", raw);


	printk(KERN_INFO "Scanning...\n");
	nstrips = read_fingerprint_on_aes2501(dev, raw, maxstrip);

	printk(KERN_INFO "Assembling...\n");


	height = assemble(raw, cooked, 16, 192, nstrips, 0, &mindiff_sum);
	height = assemble(raw, cooked, 16, 192, nstrips, 1, &mindiff_sum_r);
	
	
	printk(KERN_INFO "Mindiff normal: %d, mindiff reversed: %d\n", mindiff_sum, mindiff_sum_r);
	
	if (mindiff_sum_r < mindiff_sum)
	{
	    PRINTK_INFO("reversed image detected\n");
	}
	else
	{
	    height = assemble(raw, cooked, 16, 192, nstrips, 0, &mindiff_sum);
	}
	
	printk(KERN_INFO "First height :: %d\n", height);

#if 0
	    if (height < 100) {
                /* It was a "touch", not a finger scan. */
		    printk(KERN_INFO "Was only a 'touch' ;(   [%d]\n", height);
		    /*break;*/
            }
	    else

	    {
		    //store_new_aes2501_pnm(cooked, swidth, height);
		    height = assemble(raw, cooked, nstrips, 1, sensor_reversed);
		    printk(KERN_INFO "Second height :: %d\n", height);
		    store_new_aes2501_pnm(cooked, swidth, height);
	    }
#endif
	store_new_aes2501_pnm(cooked, swidth, height);
	standby_aes2501(dev);

	vfree(raw);

	flag = 1;
	wake_up_interruptible(&wq);


}


static int aes2501_open(struct inode *inode, struct file *file)
{
	struct usb_aes2501 *dev;

	//return 0;


	dev = _dev;

	printk(KERN_INFO "Plop !\n");


	return 0;
}






/*
 * usb class driver info in order to get a minor number from the usb core,
 * and to have the device registered with the driver core
 */
static struct usb_class_driver aes2501_class = {
	.name =		"aes2501%d",
	.fops =		&aes2501_fops,
	.minor_base =	USB_AES2501_MINOR_BASE,
};

static int aes2501_probe(struct usb_interface *interface, const struct usb_device_id *id)
{
	int retval;
	struct usb_aes2501 *dev;
	struct usb_host_interface *iface_desc;
	__u8 i;
	struct usb_endpoint_descriptor *endpoint;
	__u16 buffer_size;



#if 0
	/* !!!!!!!!!!!!!!!!!!!!!!!!!!!!! */

	uint8_t *raw, *cooked;
	unsigned swidth = 192;
	unsigned sheight = 16;
	unsigned maxstrip = 150;
	unsigned nstrips, height;

	/* !!!!!!!!!!!!!!!!!!!!!!!!!!!!! */


	raw = (uint8_t *)__get_free_page(GFP_KERNEL);//kmalloc((3 * maxstrip * sheight * swidth) / 2, GFP_KERNEL);
	cooked = raw + (maxstrip * sheight * swidth)/2;
#endif

	printk(KERN_INFO "aes2501 probe !\n");


	retval = -ENOMEM;

	/* Allocate memory for our device state and initialize it */

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (dev == NULL)
	{
		PRINTK_CRIT("out of memory\n");
		goto error;
	}

	_dev = dev;
	
	dev->stop_scan = 0;

	kref_init(&dev->kref);
	sema_init(&dev->limit_sem, WRITES_IN_FLIGHT);
	mutex_init(&dev->io_mutex);

	dev->udev = usb_get_dev(interface_to_usbdev(interface));
	dev->interface = interface;
	
	printk(KERN_INFO "num_altsetting == %d\n", interface->num_altsetting);

	/**
	 * Set up the endpoint information.
	 * Use only the first bulk-in and bulk-out endpoints.
	 */

	iface_desc = interface->cur_altsetting;

	printk(KERN_INFO "BULK COUNT %d\n", iface_desc->desc.bNumEndpoints);

	for (i = 0; i < iface_desc->desc.bNumEndpoints; ++i)
	{
		endpoint = &iface_desc->endpoint[i].desc;

		if (!dev->bulk_in_endpointAddr && /*usb_endpoint_is_bulk_in(endpoint))*/
		    ((endpoint->bEndpointAddress & USB_ENDPOINT_DIR_MASK)
					== USB_DIR_IN) &&
		    ((endpoint->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK)
					== USB_ENDPOINT_XFER_BULK))
		{
			/* We found a bulk in endpoint */
			buffer_size = le16_to_cpu(endpoint->wMaxPacketSize);
			dev->bulk_in_size = buffer_size;
			dev->bulk_in_endpointAddr = endpoint->bEndpointAddress;
			dev->bulk_in_buffer = kmalloc(buffer_size, GFP_KERNEL);

			printk(KERN_INFO "BULK IN %d ; size = %d\n", i, buffer_size);

			if (dev->bulk_in_buffer == NULL)
			{
				PRINTK_CRIT("could not allocate bulk_in_buffer\n");
				goto error;
			}

		}

		if (!dev->bulk_out_endpointAddr && /*usb_endpoint_is_bulk_out(endpoint))*/
		    ((endpoint->bEndpointAddress & USB_ENDPOINT_DIR_MASK)
					== USB_DIR_OUT) &&
		    ((endpoint->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK)
					== USB_ENDPOINT_XFER_BULK))
		{
			printk(KERN_INFO "BULK OUT %d\n", i);

			/* We found a bulk out endpoint */
			dev->bulk_out_endpointAddr = endpoint->bEndpointAddress;
		}

	}

	if (!(dev->bulk_in_endpointAddr && dev->bulk_out_endpointAddr))
	{
		PRINTK_CRIT("could not find both bulk-in and bulk-out endpoints\n");
		goto error;
	}

/*
	dev->once_tasklet = kzalloc(sizeof(*dev->once_tasklet), GFP_KERNEL);
	if (dev->once_tasklet == NULL)
	{
		PRINTK_CRIT("out of memory\n");
		goto error;
	}
*/
	/* Save our data pointer in this interface device */
	usb_set_intfdata(interface, dev);

	/* We can register the device now, as it is ready */
	retval = usb_register_dev(interface, &aes2501_class);
	if (retval) {
		/* something prevented us from registering this driver */
		PRINTK_CRIT("not able to get a minor for this device\n");
		usb_set_intfdata(interface, NULL);
		goto error;
	}

	/* Let the user know what node this device is now attached to */
	PRINTK_INFO("device now attached to aes2501-%d\n", interface->minor);

#if 0
	setup_aes2501(dev);

	while (!detect_finger_on_aes2501(dev));


	printk(KERN_INFO "Raw is 0x%p\n", raw);


	printk(KERN_INFO "Scanning...\n");
	nstrips = read_fingerprint_on_aes2501(dev, raw, maxstrip);

	printk(KERN_INFO "Assembling...\n");

	    height = assemble(raw, cooked, nstrips, 1);
	    if (height < 100) {
                /* It was a "touch", not a finger scan. */
		    printk(KERN_INFO "Was only a 'touch' ;(\n");
		    /*break;*/
            }
	    else
	    {
	    store_new_aes2501_pnm(cooked, swidth, height);
	    height = assemble(raw, cooked, nstrips, 0);
	    store_new_aes2501_pnm(cooked, swidth, height);
	    }

	standby_aes2501(dev);
#endif

	return 0;

error:
	if (dev != NULL)
		kref_put(&dev->kref, aes2501_delete);

	return retval;

}


static void aes2501_disconnect(struct usb_interface *interface)
{
	int minor;
	struct usb_aes2501 *dev;

	minor = interface->minor;

	/* Prevent aes2501_open() from racing aes2501_disconnect() */
	lock_kernel();

	dev = usb_get_intfdata(interface);
	usb_set_intfdata(interface, NULL);

	/* Give back our minor */
	usb_deregister_dev(interface, &aes2501_class);

	/* Prevent more I/O from starting */
	mutex_lock(&dev->io_mutex);
	dev->interface = NULL;
	mutex_unlock(&dev->io_mutex);

	unlock_kernel();

	/* Decrement our usage count */
	kref_put(&dev->kref, aes2501_delete);

	PRINTK_INFO("aes2501 device #%d now disconnected\n", minor);




	printk(KERN_INFO "aes2501 disconnect !\n");


}


static struct usb_driver aes2501_driver = {
	.name		= "aes2501",
	.probe		= aes2501_probe,
	.disconnect	= aes2501_disconnect,
	.id_table	= id_table,
};


static int __init aes2501_init(void)
{
	int retval;

	comm_queue = create_singlethread_workqueue("aes2501");
	if (comm_queue == NULL) {
		PRINTK_CRIT("could not create work queue\n");
		return -ENOMEM;
	}

	retval = usb_register(&aes2501_driver);
	if (retval == -EINVAL)
	{
		printk(KERN_ERR "usb_register failed. Error number %d\n", retval);
		destroy_workqueue(comm_queue);
		return retval;
	}

	return 0;

}

static void __exit aes2501_exit(void)
{
	if (_dev)
		_dev->stop_scan = 1;
	destroy_workqueue(comm_queue);
	usb_deregister(&aes2501_driver);
}

module_init(aes2501_init);
module_exit(aes2501_exit);

MODULE_AUTHOR("Cyrille Bagard");
MODULE_DESCRIPTION("AES 2501 fingerprint scanner driver");
MODULE_LICENSE("GPL");
