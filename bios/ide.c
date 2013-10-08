/*
 * ide.c - Falcon IDE functions
 *
 * Copyright (c) 2011-2013 The EmuTOS development team
 *
 * Authors:
 *  VRI   Vincent Rivière
 *  RFB   Roger Burrows
 *
 * This file is distributed under the GPL, version 2 or at your
 * option any later version.  See doc/license.txt for details.
 */

/*
 * Warning: This is beta IDE support.
 */

/* #define ENABLE_KDEBUG */

#include "config.h"
#include "portab.h"
#include "asm.h"
#include "blkdev.h"
#include "delay.h"
#include "disk.h"
#include "ide.h"
#include "mfp.h"
#include "gemerror.h"
#include "tosvars.h"
#include "vectors.h"
#include "kprint.h"
#include "coldfire.h"
#ifdef MACHINE_AMIGA
#include "amiga.h"
#endif

#if CONF_WITH_IDE

#ifdef MACHINE_M548X

#include "coldpriv.h"

struct IDE
{
    UBYTE filler00[2];
    UBYTE sector_number;
    UBYTE sector_count;
    UBYTE cylinder_high;
    UBYTE cylinder_low;
    UBYTE command;  /* Read: status */
    UBYTE head;
    UWORD data;
    UBYTE filler0a[4];
    UBYTE dummy;
    UBYTE control;  /* Read: Alternate status */
};

#define ide_interface ((volatile struct IDE *)(COMPACTFLASH_BASE + 0x1800))

/* On M548X, the IDE registers must be read and written as a single word. */

#define IDE_WRITE_REGISTER_PAIR(r,a,b) \
    *(volatile UWORD *)&ide_interface->r = MAKE_UWORD(a,b)

#define IDE_WRITE_SECTOR_NUMBER_SECTOR_COUNT(a,b) \
    IDE_WRITE_REGISTER_PAIR(sector_number,a,b)

#define IDE_WRITE_CYLINDER_HIGH_CYLINDER_LOW(a) \
    *(volatile UWORD *)&ide_interface->cylinder_high = a

#define IDE_WRITE_COMMAND_HEAD(a,b) \
    IDE_WRITE_REGISTER_PAIR(command,a,b)

#define IDE_WRITE_CONTROL(a) \
    IDE_WRITE_REGISTER_PAIR(dummy,0,a)

/*
 * this macro uses the NOP command, which is specifically provided
 * for situations where the command register must be written at the
 * same time as the head register.  see the x3t10 ata-2 and ata-3
 * specifications for details.
 */
#define IDE_WRITE_HEAD(a) \
    IDE_WRITE_REGISTER_PAIR(command,IDE_CMD_NOP,a)

#define IDE_READ_REGISTER_PAIR(r) \
    *(volatile UWORD *)&ide_interface->r

#define IDE_READ_SECTOR_NUMBER_SECTOR_COUNT() \
    IDE_READ_REGISTER_PAIR(sector_number)

#define IDE_READ_CYLINDER_HIGH_CYLINDER_LOW() \
    IDE_READ_REGISTER_PAIR(cylinder_high)

#define IDE_READ_STATUS()   ide_interface->command

#define IDE_READ_ALT_STATUS() \
    IDE_READ_REGISTER_PAIR(dummy)

#endif /* MACHINE_M548X */

#if CONF_ATARI_HARDWARE

struct IDE
{
    UWORD data;
    UBYTE filler02[3];
    UBYTE features; /* Read: error */
    UBYTE filler06[3];
    UBYTE sector_count;
    UBYTE filler0a[3];
    UBYTE sector_number;
    UBYTE filler0e[3];
    UBYTE cylinder_low;
    UBYTE filler12[3];
    UBYTE cylinder_high;
    UBYTE filler16[3];
    UBYTE head;
    UBYTE filler1a[3];
    UBYTE command;  /* Read: status */
    UBYTE filler1e[27];
    UBYTE control;  /* Read: Alternate status */
    UBYTE filler3a[6];
};

#define ide_interface           ((volatile struct IDE *)0xfff00000)

#endif /* CONF_ATARI_HARDWARE */

#ifndef MACHINE_M548X

/* On standard hardware, the IDE registers can be accessed as single bytes. */

#define IDE_WRITE_SECTOR_NUMBER_SECTOR_COUNT(a,b) \
    { interface->sector_number = a; interface->sector_count = b; }
#define IDE_WRITE_CYLINDER_HIGH_CYLINDER_LOW(a) \
    { interface->cylinder_high = (a)>>8; interface->cylinder_low = (a)&0xff; }
#define IDE_WRITE_COMMAND_HEAD(a,b) \
    { interface->head = b; interface->command = a; }
#define IDE_WRITE_CONTROL(a)    interface->control = a
#define IDE_WRITE_HEAD(a)       interface->head = a

#define IDE_READ_STATUS()       interface->command
#define IDE_READ_ALT_STATUS()   interface->control
#define IDE_READ_SECTOR_NUMBER_SECTOR_COUNT() \
    ((interface->sector_number<<8) | interface->sector_count)
#define IDE_READ_CYLINDER_HIGH_CYLINDER_LOW() \
    ((interface->cylinder_high<<8) | interface->cylinder_low)

#endif /* !defined(MACHINE_M548X) */

/* IDE defines */

#define IDE_CMD_IDENTIFY_DEVICE 0xec
#define IDE_CMD_NOP             0x00
#define IDE_CMD_READ_SECTOR     0x20
#define IDE_CMD_WRITE_SECTOR    0x30

#define IDE_MODE_CHS    (0 << 6)
#define IDE_MODE_LBA    (1 << 6)
#define IDE_DEVICE(n)   ((n) << 4)

#define IDE_CONTROL_nIEN (1 << 1)
#define IDE_CONTROL_SRST (1 << 2)

#define IDE_STATUS_ERR  (1 << 0)
#define IDE_STATUS_DRQ  (1 << 3)
#define IDE_STATUS_DF   (1 << 5)
#define IDE_STATUS_DRDY (1 << 6)
#define IDE_STATUS_BSY  (1 << 7)

/* bus/device info */

struct BUSINFO {
    struct {
        UBYTE type;
        UBYTE features;
    } dev[2];
};

#define DEVTYPE_NONE    0
#define DEVTYPE_UNKNOWN 1
#define DEVTYPE_ATA     2
#define DEVTYPE_ATAPI   3


/* timing stuff */

#define DELAY_400NS     delay_loop(delay400ns)
#define DELAY_5US       delay_loop(delay5us)

#define SHORT_TIMEOUT   (CLOCKS_PER_SEC/10) /* 100ms */
#define XFER_TIMEOUT    (CLOCKS_PER_SEC)    /* 1000ms for data xfer */
#define LONG_TIMEOUT    (31*CLOCKS_PER_SEC) /* 31 seconds for reset (!)*/

static int has_ide;
static struct BUSINFO businfo[1];
static ULONG delay400ns;
static ULONG delay5us;


/* prototypes */
static void ide_detect_devices(UWORD bus);
static int wait_for_not_BSY(volatile struct IDE *interface,WORD timeout);


void detect_ide(void)
{
#ifdef MACHINE_AMIGA
    has_ide = has_gayle;
#elif defined(MACHINE_M548X)
    has_ide = TRUE;
#else
    has_ide = check_read_byte((long)&ide_interface[0].command);
#endif

    KDEBUG(("has_ide = %d\n",has_ide));
}

/*
 * perform any one-time initialisation required
 */
void ide_init(void)
{
    delay400ns = loopcount_1_msec / 2500;
    delay5us = loopcount_1_msec / 200;
    if (has_ide)
        ide_detect_devices(0);
}

/*
 * the following routines for device type detection are adapted
 * from Hale Landis's public domain ATA driver, MINDRVR.
 */
static int wait_for_signature(volatile struct IDE *interface,WORD timeout)
{
    LONG next = hz_200 + timeout;
    UWORD n;

    DELAY_400NS;
    while(hz_200 < next) {
        n = IDE_READ_SECTOR_NUMBER_SECTOR_COUNT();
        if (n == 0x0101)
            return 0;
    }

    KDEBUG(("Timeout in wait_for_signature(%p,%d)\n",interface,timeout));
    return 1;
}

static void ide_reset(UWORD bus)
{
    volatile struct IDE *interface = ide_interface + bus;
    struct BUSINFO *info = businfo + bus;
    int err;

    /* set, then reset, the soft reset bit */
    IDE_WRITE_CONTROL((IDE_CONTROL_SRST|IDE_CONTROL_nIEN));
    DELAY_5US;
    IDE_WRITE_CONTROL(IDE_CONTROL_nIEN);
    DELAY_400NS;

    /* if device 0 exists, wait for it to clear BSY */
    if (info->dev[0].type != DEVTYPE_NONE) {
        if (wait_for_not_BSY(interface,LONG_TIMEOUT)) {
            info->dev[0].type = DEVTYPE_NONE;
            KDEBUG(("IDE bus %d device 0 timeout after soft reset\n",bus));
        }
    }

    /* if device 1 exists, wait for the signature bits, then check BSY */
    if (info->dev[1].type != DEVTYPE_NONE) {
        err = 0;
        if (wait_for_signature(interface,LONG_TIMEOUT))
            err = 1;
        else if ((IDE_READ_ALT_STATUS() & IDE_STATUS_BSY) == 0)
            err = 1;
        if (err) {
            info->dev[1].type = DEVTYPE_NONE;
            KDEBUG(("IDE bus %d device 1 timeout after soft reset\n",bus));
        }
    }
}

static UBYTE ide_decode_type(UBYTE status,UWORD signature)
{
    if (signature == 0xeb14)    /* PATAPI */
        return DEVTYPE_ATAPI;
    if (signature == 0x9669)    /* SATAPI */
        return DEVTYPE_ATAPI;

    if (status == 0)
        return DEVTYPE_UNKNOWN;

    if (signature == 0x0000)    /* PATA */
        return DEVTYPE_ATA;
    if (signature == 0xc33c)    /* SATA */
        return DEVTYPE_ATA;

    return DEVTYPE_UNKNOWN;
}

static void ide_detect_devices(UWORD bus)
{
    volatile struct IDE *interface = ide_interface + bus;
    struct BUSINFO *info = businfo + bus;
    UBYTE status;
    UWORD signature;
    int i;

    MAYBE_UNUSED(interface);

    IDE_WRITE_CONTROL(IDE_CONTROL_nIEN);    /* no interrupts please */

    /* initial check for devices */
    for (i = 0; i < 2; i++) {
        IDE_WRITE_HEAD(IDE_DEVICE(i));
        DELAY_400NS;
        IDE_WRITE_SECTOR_NUMBER_SECTOR_COUNT(0xaa,0x55);
        IDE_WRITE_SECTOR_NUMBER_SECTOR_COUNT(0x55,0xaa);
        IDE_WRITE_SECTOR_NUMBER_SECTOR_COUNT(0xaa,0x55);
        if (IDE_READ_SECTOR_NUMBER_SECTOR_COUNT() == 0xaa55)
            info->dev[i].type = DEVTYPE_UNKNOWN;
        else 
            info->dev[i].type = DEVTYPE_NONE;
        info->dev[i].features = 0;
    }

    /* recheck after soft reset, also detect ata/atapi */
    IDE_WRITE_HEAD(IDE_DEVICE(0));
    DELAY_400NS;
    ide_reset(bus);

    for (i = 0; i < 2; i++) {
        IDE_WRITE_HEAD(IDE_DEVICE(i));
        DELAY_400NS;
        if (IDE_READ_SECTOR_NUMBER_SECTOR_COUNT() == 0x0101) {
            status = IDE_READ_STATUS();
            signature = IDE_READ_CYLINDER_HIGH_CYLINDER_LOW();
            info->dev[i].type = ide_decode_type(status,signature);
        }
    }

    for (i = 0; i < 2; i++)
        KDEBUG(("IDE bus %d device %d is type %d\n",bus,i,info->dev[i].type));
}

/*
 * the following code is intended to follow the PIO data transfer diagrams
 * as shown in the X3T10 specifications for the ATA/ATAPI interface.  note
 * that the transfer diagrams differ slightly between different versions
 * of the interface, so this is my choice of what to believe in :-)  RFB
 */

/*
 * wait for access to IDE registers
 */
static int wait_for_not_BSY(volatile struct IDE *interface,WORD timeout)
{
    LONG next = hz_200 + timeout;

    DELAY_400NS;
    while(hz_200 < next) {
        if ((IDE_READ_ALT_STATUS() & IDE_STATUS_BSY) == 0)
            return 0;
    }

    KDEBUG(("Timeout in wait_for_not_BSY(%p,%d)\n",interface,timeout));
    return 1;
}

static int wait_for_not_BSY_not_DRQ(volatile struct IDE *interface,WORD timeout)
{
    LONG next = hz_200 + timeout;

    DELAY_400NS;
    while(hz_200 < next) {
        if ((IDE_READ_ALT_STATUS() & (IDE_STATUS_BSY|IDE_STATUS_DRQ)) == 0)
            return 0;
    }

    KDEBUG(("timeout in wait_for_not_BSY_not_DRQ(%p,%d)\n",interface,timeout));
    return 1;
}

/*
 * select device in IDE registers
 */
static int ide_select_device(volatile struct IDE *interface,UWORD dev)
{
    if (wait_for_not_BSY_not_DRQ(interface,SHORT_TIMEOUT))
        return ERR;

    IDE_WRITE_HEAD(IDE_DEVICE(dev));

    if (wait_for_not_BSY_not_DRQ(interface,SHORT_TIMEOUT))
        return ERR;

    return E_OK;
}

/*
 * set device / command / sector start / count / LBA mode in IDE registers
 */ 
static void ide_rw_start(volatile struct IDE *interface,UWORD dev,ULONG sector,UBYTE cmd)
{
    IDE_WRITE_SECTOR_NUMBER_SECTOR_COUNT((sector & 0xff), 1);
    IDE_WRITE_CYLINDER_HIGH_CYLINDER_LOW((UWORD)((sector & 0xffff00) >> 8));
    IDE_WRITE_COMMAND_HEAD(cmd,IDE_MODE_LBA|IDE_DEVICE(dev)|(UBYTE)((sector>>24)&0x0f));
}

/*
 * get data from IDE device
 */
static void ide_get_data(volatile struct IDE *interface,UBYTE *buffer,int need_byteswap)
{
    UWORD *p = (UWORD *)buffer;
    UWORD *end = (UWORD *)(buffer + SECTOR_SIZE);

    while (p < end)
        *p++ = interface->data;

    if (need_byteswap)
        byteswap(buffer,SECTOR_SIZE);
}

/*
 * read sector
 */
static LONG ide_read_sector(UWORD bus,UWORD dev,ULONG sector,UBYTE *buffer,BOOL need_byteswap)
{
    volatile struct IDE *interface = ide_interface + bus;
    UBYTE status1, status2;
    LONG rc;

    if (ide_select_device(interface,dev) < 0)
        return EREADF;

    ide_rw_start(interface,dev,sector,IDE_CMD_READ_SECTOR);
    if (wait_for_not_BSY(interface,XFER_TIMEOUT))
        return EREADF;

    status1 = IDE_READ_ALT_STATUS();    /* alternate status, ignore */
    status1 = IDE_READ_STATUS();        /* status, clear pending interrupt */

    rc = E_OK;
    if (status1 & IDE_STATUS_DRQ)
        ide_get_data(interface,buffer,need_byteswap);
    if (status1 & (IDE_STATUS_DF | IDE_STATUS_ERR))
        rc = EREADF;

    status2 = IDE_READ_ALT_STATUS();    /* alternate status, ignore */
    status2 = IDE_READ_STATUS();        /* status, clear pending interrupt */

    if (status2 & (IDE_STATUS_BSY|IDE_STATUS_DF|IDE_STATUS_DRQ|IDE_STATUS_ERR))
        rc = EREADF;

    return rc;
}

/*
 * send data to IDE device
 */
static void ide_put_data(volatile struct IDE *interface,UBYTE *buffer,int need_byteswap)
{
    UWORD *p = (UWORD *)buffer;
    UWORD *end = (UWORD *)(buffer + SECTOR_SIZE);

    if (need_byteswap) {
        while (p < end) {
            UWORD temp;
            temp = *p++;
            swpw(temp);
            interface->data = temp;
        }
    } else {
        while (p < end)
            interface->data = *p++;
    }
}

/*
 * write sector
 */
static LONG ide_write_sector(UWORD bus,UWORD dev,ULONG sector,UBYTE *buffer,BOOL need_byteswap)
{
    volatile struct IDE *interface = ide_interface + bus;
    UBYTE status1, status2;
    LONG rc;

    if (ide_select_device(interface,dev) < 0)
        return EWRITF;

    ide_rw_start(interface,dev,sector,IDE_CMD_WRITE_SECTOR);
    if (wait_for_not_BSY(interface,SHORT_TIMEOUT))
        return EWRITF;

    rc = E_OK;
    status1 = IDE_READ_STATUS();        /* status, clear pending interrupt */
    if (status1 & IDE_STATUS_DRQ)
        ide_put_data(interface,buffer,need_byteswap);
    if (status1 & (IDE_STATUS_DF|IDE_STATUS_ERR))
         rc = EWRITF;

    if (wait_for_not_BSY(interface,XFER_TIMEOUT))   /* while device processes data */
        return EWRITF;

    status2 = IDE_READ_ALT_STATUS();    /* alternate status (ignore) */
    status2 = IDE_READ_STATUS();        /* status, clear pending interrupt */
    if (status2 & (IDE_STATUS_BSY|IDE_STATUS_DF|IDE_STATUS_DRQ|IDE_STATUS_ERR))
         rc = EWRITF;

    return rc;
}

LONG ide_rw(WORD rw,LONG sector,WORD count,LONG buf,WORD dev,BOOL need_byteswap)
{
    UBYTE *p = (UBYTE *)buf;
    UWORD bus;
    LONG ret;

    if (!has_ide)
        return EUNDEV;

    if (dev >= 2) /* Only Master and Slave device supported */
        return EUNDEV;

    bus = dev / 2;  /* i.e. primary IDE, secondary IDE, ... */
    dev &= 1;       /* 0 or 1 */

    if (businfo[bus].dev[dev].type != DEVTYPE_ATA)
        return EUNDEV;

    while (count > 0)
    {
        ret = rw ? ide_write_sector(bus,dev,sector,p,need_byteswap)
                : ide_read_sector(bus,dev,sector,p,need_byteswap);
        if (ret < 0) {
            KDEBUG(("ide_rw(%d,%d,%ld,%p,%d) rc=%ld\n",bus,dev,sector,p,need_byteswap,ret));
            return ret;
        }

        p += SECTOR_SIZE;
        ++sector;
        --count;
    }

    return E_OK;
}

#endif /* CONF_WITH_IDE */
