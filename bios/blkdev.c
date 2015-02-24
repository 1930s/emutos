/*
 * blkdev.c - BIOS block device functions
 *
 * Copyright (c) 2002-2015 The EmuTOS development team
 *
 * Authors:
 *  MAD     Martin Doering
 *  PES     Petr Stehlik
 *
 * This file is distributed under the GPL, version 2 or at your
 * option any later version.  See doc/license.txt for details.
 */

/* #define ENABLE_KDEBUG */

#include "config.h"
#include "portab.h"
#include "string.h"
#include "gemerror.h"
#include "kprint.h"
#include "tosvars.h"
#include "ahdi.h"
#include "mfp.h"
#include "floppy.h"
#include "disk.h"
#include "ikbd.h"
#include "blkdev.h"
#include "processor.h"
#include "acsi.h"
#include "ide.h"
#include "sd.h"

/*
 * Global variables
 */

BLKDEV blkdev[BLKDEVNUM];

static BYTE diskbuf[2*SECTOR_SIZE];     /* buffer for 2 sectors */

static PUN_INFO pun_info;

/*
 * Function prototypes
 */
static void blkdev_hdv_init(void);
static LONG blkdev_mediach(WORD dev);
static LONG blkdev_rwabs(WORD rw, LONG buf, WORD cnt, WORD recnr, WORD dev, LONG lrecnr);
static void bus_init(void);

/* get intel words */
static UWORD getiword(UBYTE *addr)
{
    UWORD value;
    value = (((UWORD)addr[1])<<8) + addr[0];
    return value;
}

/*
 * blkdevs_init - BIOS block drive initialization
 *
 * Called from bios.c, this routine will do necessary block device
 * initialization.
 */

void blkdev_init(void)
{
    /* set the block buffer pointer to reserved memory */
    dskbufp = &diskbuf;
    KDEBUG(("diskbuf = %08lx\n",(long)dskbufp));

    /* setup booting related vectors */
    hdv_boot    = blkdev_hdv_boot;
    hdv_init    = 0; // blkdev_hdv_init;

    /* setup general block device vectors */
    hdv_bpb     = blkdev_getbpb;
    hdv_rw      = blkdev_rwabs;
    hdv_mediach = blkdev_mediach;

    /* setting drvbits */
    blkdev_hdv_init();
}

/* currently the only valid information in the PUN_INFO is the max_sect_siz */
/* which is exactly what FreeMiNT was missing and was complaining about... */
static void pun_info_setup(void)
{
    int i;
    BPB *bpb;
    LONG max_size;

    /* set PUN_INFO */
    pun_info.puns = BLKDEVNUM;
    pun_info.max_sect_siz = MAX_LOGSEC_SIZE;    /* temporarily, for blkdev_getbpb() */

    /* floppy A: */
    pun_info.pun[0] = 0;    /* FIXME */
    pun_info.partition_start[0] = 0;

    /* floppy B: */
    pun_info.pun[1] = 0;    /* FIXME */
    pun_info.partition_start[1] = 0;

    /* disks C: - P: */
    for(i = 2, max_size = SECTOR_SIZE; i < 16; i++) {
        pun_info.pun[i] = 0;    /* FIXME */
        pun_info.partition_start[i] = 0;    /* FIXME */

        bpb = (BPB *)blkdev_getbpb(i);
        if (!bpb)
            continue;
        if (bpb->recsiz > max_size) {
            max_size = bpb->recsiz;
        }
    }

    pun_info.cookie = 0x41484449L; /* AHDI */
    pun_info.cookie_ptr = &pun_info.cookie;
    pun_info.version_num = 0x300;      /* AHDI v3.00 */
    pun_info.max_sect_siz = max_size;

    pun_ptr = &pun_info;

    KDEBUG(("PUN INFO: max sector size = %d\n",pun_info.max_sect_siz));
}

/*
 * blkdev_hdv_init
 *
 */

static void blkdev_hdv_init(void)
{
    /* Start with no drives. This matters for EmuTOS-RAM, because the system
     * variables are not automatically reinitialized. */
    drvbits = 0;

    /* Detect and initialize floppy drives */
    flop_hdv_init();

    /*
     * do bus initialisation, such as setting delay values
     */
    bus_init();

    disk_init_all();    /* Detect hard disk partitions */

    pun_info_setup();
}

/*
 * call bus initialisation routines
 */
static void bus_init(void)
{
#if CONF_WITH_ACSI
    acsi_init();
#endif

#if CONF_WITH_IDE
    ide_init();
#endif

#if CONF_WITH_SDMMC
    sd_init();
#endif
}

/*
 * blkdev_hdv_boot - BIOS boot vector
 */

LONG blkdev_hdv_boot(void)
{
    LONG mode = kbshift(-1);

    KDEBUG(("drvbits = %08lx\n",drvbits));

    /* boot eventually from a block device (floppy or harddisk) */

    /* the actual boot device or better the order of several boot devices
       should be configurable like for example in PC CMOS SETUP:
       e.g. first CD-ROM, then IDE primary, then SCSI and floppy at last.
       As we don't have such configuration yet there's a hardcoded order
       for now: if C: exists use it as the boot device, otherwise boot from A:
    */

    /* boot hard drive only if user does not hold the Alternate key down */
    if (!(mode & MODE_ALT)) {
        if (blkdev_avail(2)) {  /* if drive C: is available */
            bootdev = 2;        /* make it the boot drive */
            return 0;           /* don't actually boot from the boot device
                                   as there is most probably a harddisk driver
                                   installed and that would require complete
                                   lowlevel IDE/ACSI/SCSI emulation. Luckily
                                   EmuTOS got its own internal hdd driver,
                                   so we don't need to execute the boot sector
                                   code and boot the drive at all! :-) */
        }
    }

    /* otherwise boot device is floppy A: */
    bootdev = 0;
    if (!(mode & MODE_CTRL))    /* if Control is NOT held down, */
        return(flop_hdv_boot());/*  try to boot from the floppy */

    return 0;
}

/*
 * return next available device number in bitmap
 */
static int next_logical(LONG *devices_available)
{
    int logical;

    for (logical = 0; logical < BLKDEVNUM; logical++)
        if (*devices_available & (1L<<logical))
            return logical;

    return -1;
}

/*
 * Add a partition's details to the device's partition description.
 */
int add_partition(UWORD unit, LONG *devices_available, char id[], ULONG start, ULONG size)
{
    int logical;
    BLKDEV *b;

    logical = next_logical(devices_available);
    if (logical < 0) {
        KDEBUG(("add_partition(): maximum number of partitions reached!\n"));
        return -1;
    }
    b = blkdev + logical;

    if (id[0])
        KDEBUG((" %c=%c%c%c",'A'+logical,id[0],id[1],id[2]));
    else
        KDEBUG((" %c=$%02x",'A'+logical,id[2]));
    KDEBUG((",start=%ld,size=%ld\n",start,size));

    b->id[0] = id[0];
    b->id[1] = id[1];
    b->id[2] = id[2];
    b->id[3] = '\0';
    b->start = start;
    b->size  = size;

    b->valid = 1;
    b->mediachange = MEDIANOCHANGE;
    b->unit  = unit;

    /* make just GEM/BGM partitions visible to applications */
/*
    if (strcmp(b->id, "GEM") == 0
        || strcmp(b->id, "BGM") == 0)
*/
        drvbits |= (1L << logical);

    *devices_available &= ~(1L << logical); /* mark device as used */

    return 0;
}


/*
 * blkdev_rwabs - BIOS block device read/write vector
 */

#define CNTMAX  0x7FFF  /* 16-bit MAXINT */

static LONG blkdev_rwabs(WORD rw, LONG buf, WORD cnt, WORD recnr, WORD dev, LONG lrecnr)
{
    int retries = RWABS_RETRIES;
    int unit = dev;
    LONG lcount = cnt;
    LONG retval;
    WORD psshift;
    void *bufstart = (void *)buf;

    KDEBUG(("rwabs(rw=%d, buf=%ld, count=%ld, recnr=%u, dev=%d, lrecnr=%ld)\n",
            rw,buf,lcount,recnr,dev,lrecnr));

    if (recnr != -1)            /* if long offset not used */
        lrecnr = (UWORD)recnr;  /* recnr as unsigned to enable 16-bit recn */

    if (rw & RW_NORETRIES)
        retries = 1;

    /*
     * are we accessing a physical unit or a logical device?
     */
    if (! (rw & RW_NOTRANSLATE)) {      /* logical */
        int sectors;
        if ((dev < 0 ) || (dev >= BLKDEVNUM) || !blkdev[dev].valid)
            return EUNDEV;  /* unknown device */

        if (blkdev[dev].bpb.recsiz == 0)/* invalid BPB, e.g. FAT32 or ext2 */
            return ESECNF;              /* (this is an XHDI convention)    */

        /* convert logical sectors to physical ones */
        sectors = blkdev[dev].bpb.recsiz >> units[blkdev[dev].unit].psshift;
        lcount *= sectors;
        lrecnr *= sectors;

        /* check if the count does fit to this partition */
        if ((lrecnr < 0) || (blkdev[dev].size > 0
                             && (lrecnr + lcount) >= blkdev[dev].size))
            return ESECNF;  /* sector not found */

        /* convert partition offset to absolute offset on a unit */
        lrecnr += blkdev[dev].start;

        /* convert logical drive to physical unit */
        unit = blkdev[dev].unit;

        KDEBUG(("rwabs translated: sector=%ld, count=%ld\n",lrecnr,lcount));

        if (! (rw & RW_NOMEDIACH)) {
            if (blkdev_mediach(dev) != MEDIANOCHANGE) {
                KDEBUG(("blkdev_rwabs(): media change detected\n"));
                return E_CHNG;
            }
        }
    }
    else {                              /* physical */
        if (unit < 0 || unit >= UNITSNUM || !units[unit].valid)
            return EUNDEV;  /* unknown device */

        /* check if the start & count values are valid for this unit */
        if ((lrecnr < 0) || (units[unit].size > 0
                             && (lrecnr + lcount) >= units[unit].size))
            return ESECNF;  /* sector not found */

        /*
         * the following is how I think this *ought* to work when a
         * physical unit is specified.  TOS may do it differently:
         * I haven't checked (RFB)
         */
        if (! (rw & RW_NOMEDIACH)) {
            if (units[unit].status&UNIT_CHANGED) {
                KDEBUG(("blkdev_rwabs(): media change detected\n"));
                units[unit].status &= ~UNIT_CHANGED;
                return E_CHNG;
            }
        }
    }

    psshift = units[unit].psshift;

    do {
        /* split the transfer to 15-bit count blocks (lowlevel functions take WORD count) */
        WORD scount = (lcount > CNTMAX) ? CNTMAX : lcount;
        do {
            if (unit < NUMFLOPPIES) {
                retval = floppy_rw(rw, buf, scount, lrecnr,
                                   blkdev[unit].geometry.spt,
                                   blkdev[unit].geometry.sides, unit);
            }
            else {
                retval = disk_rw(unit, (rw & ~RW_NOTRANSLATE), lrecnr, scount, (void *)buf);
            }
            if (retval == E_CHNG)   /* no retry on media change */
                break;
        } while((retval < 0) && (--retries > 0));
        if (retval < 0)     /* error, retries exhausted */
            break;
        buf += scount << psshift;
        lrecnr += scount;
        lcount -= scount;
    } while(lcount > 0);

    /* TOS invalidates the i-cache here, so be compatible */
    if ((rw&RW_RW) == RW_READ)
        instruction_cache_kludge(bufstart,cnt<<psshift);

    if (retval == 0)
        units[unit].last_access = hz_200;

    if (retval == E_CHNG)
        if (unit >= NUMFLOPPIES)
            disk_rescan(unit);

    return retval;
}


/*
 * get_shift - get #bits to shift left to convert from blocksize to bytes
 *
 * returns -1 iff blocksize is not a power of 2
 */
WORD get_shift(ULONG blocksize)
{
    WORD i;
    ULONG n;

    for (i = 0, n = 1UL; i < 32; i++, n <<= 1)
        if (n == blocksize)
            return i;

    return -1;
}


/*
 * blkdev_getbpb - Get BIOS parameter block
 *
 * implement the Mediach flush as documented in Compendium
 */

LONG blkdev_getbpb(WORD dev)
{
    BLKDEV *bdev = blkdev + dev;
    struct bs *b;
    struct fat16_bs *b16;
    ULONG tmp;
    UWORD reserved, recsiz;
    int unit;

    KDEBUG(("blkdev_getbpb(%d)\n",dev));

    if ((dev < 0 ) || (dev >= BLKDEVNUM) || !bdev->valid)
        return 0L;  /* unknown device */

    bdev->mediachange = MEDIANOCHANGE;      /* reset now */
    bdev->bpb.recsiz = 0;                   /* default to invalid BPB */

    /*
     * before we can build the BPB, we need to locate the bootsector.
     * if we're on a removable non-floppy unit, this may have moved
     * since last time, so we handle this first.
     */
    unit = bdev->unit;
    if ((unit >= NUMFLOPPIES) && (units[unit].features & UNIT_REMOVABLE))
        disk_mediach(unit);     /* check for change & rescan partitions if so */

    /* now we can read the bootsector using the physical mode */
    if (blkdev_rwabs(RW_READ | RW_NOMEDIACH | RW_NOTRANSLATE,
                     (LONG)dskbufp, 1, -1, unit, bdev->start))
        return 0L;  /* error */

    b = (struct bs *)dskbufp;
    b16 = (struct fat16_bs *)dskbufp;

    if (b->spc == 0)
        return 0L;

    /* don't login a disk if the logical sector size is too large */
    recsiz = getiword(b->bps);
    if (recsiz > pun_info.max_sect_siz)
        return 0L;

    KDEBUG(("bootsector[dev=%d] = {\n  ...\n  res = %d;\n  hid = %d;\n}\n",
            dev,getiword(b->res),getiword(b->hid)));

    bdev->bpb.recsiz = recsiz;
    bdev->bpb.clsiz = b->spc;
    bdev->bpb.clsizb = bdev->bpb.clsiz * bdev->bpb.recsiz;
    tmp = getiword(b->dir);
    if (bdev->bpb.recsiz != 0)
        bdev->bpb.rdlen = (tmp * 32) / bdev->bpb.recsiz;
    else
        bdev->bpb.rdlen = 0;
    bdev->bpb.fsiz = getiword(b->spf);

    /* the structure of the logical disk is assumed to be:
     * - bootsector
     * - other reserved sectors (if any)
     * - fats
     * - dir
     * - data clusters
     */
    reserved = getiword(b->res);
    if (reserved == 0)      /* should not happen */
        reserved = 1;       /* but if it does, Atari TOS assumes this */
    bdev->bpb.fatrec = reserved + bdev->bpb.fsiz;
    bdev->bpb.datrec = bdev->bpb.fatrec + bdev->bpb.fsiz + bdev->bpb.rdlen;

    /*
     * determine number of clusters
     */
    tmp = getiword(b->sec);
    /* handle DOS-style disks (512-byte logical sectors) >= 32MB */
    if (tmp == 0L)
        tmp = ((ULONG)getiword(b16->sec2+2)<<16) + getiword(b16->sec2);
    tmp = (tmp - bdev->bpb.datrec) / b->spc;
    if (tmp > MAX_FAT16_CLUSTERS)           /* FAT32 - unsupported */
        return 0L;
    bdev->bpb.numcl = tmp;

    /*
     * check for FAT12 or FAT16
     */
    if (b16->ext == 0x29 && !memcmp(b16->fstype, "FAT12   ", 8))
        bdev->bpb.b_flags = 0;          /* explicit FAT12 */
    else if (b16->ext == 0x29 && !memcmp(b16->fstype, "FAT16   ", 8))
        bdev->bpb.b_flags = B_16;       /* explicit FAT16 */
    else if (bdev->bpb.numcl > MAX_FAT12_CLUSTERS)
        bdev->bpb.b_flags = B_16;       /* implicit FAT16 */
    else bdev->bpb.b_flags = 0;         /* implicit FAT12 */

    /* additional geometry info */
    bdev->geometry.sides = getiword(b->sides);
    bdev->geometry.spt = getiword(b->spt);
    memcpy(bdev->serial,b->serial,3);
    memcpy(bdev->serial2,b16->serial2,4);

    KDEBUG(("bpb[dev=%d] = {\n  recsiz = %d;\n  clsiz  = %d;\n",
            dev,bdev->bpb.recsiz,bdev->bpb.clsiz));
    KDEBUG(("  clsizb = %u;\n  rdlen  = %d;\n  fsiz   = %d;\n",
            bdev->bpb.clsizb,bdev->bpb.rdlen,bdev->bpb.fsiz));
    KDEBUG(("  fatrec = %d;\n  datrec = %d;\n  numcl  = %u;\n",
            bdev->bpb.fatrec,bdev->bpb.datrec,bdev->bpb.numcl));
    KDEBUG(("  bflags = %d;\n}\n",bdev->bpb.b_flags));

    return (LONG) &bdev->bpb;
}

/*
 * blkdev_mediach - BIOS media change vector
 */

static LONG blkdev_mediach(WORD dev)
{
    BLKDEV *b = &blkdev[dev];
    UWORD unit = b->unit;
    LONG ret;

    if ((dev < 0 ) || (dev >= BLKDEVNUM) || !b->valid)
        return EUNDEV;  /* unknown device */

    switch(b->mediachange) {
    case MEDIANOCHANGE:
        /* if less than half a second since last access, assume no mediachange */
        if (hz_200 < units[unit].last_access + CLOCKS_PER_SEC/2)
            break;                  /* will return no change */
        /* drop through */
    case MEDIAMAYCHANGE:
        ret = (dev<NUMFLOPPIES) ? flop_mediach(dev) : disk_mediach(unit);
        if (ret < 0)
            return ret;
        if (ret != MEDIANOCHANGE) { /* if media (may have) changed, mark physical unit */
            units[unit].status |= UNIT_CHANGED;
            b->mediachange = ret;
        }
        break;
    /* if we've already marked the drive as MEDIACHANGE, don't change it */
    }

    return b->mediachange;
}


/**
 * blkdev_drvmap - Read drive bitmap
 *
 * Returns a long containing a bit map of logical drives on  the system,
 * with bit 0, the least significant bit, corresponding to drive A.
 * Note that if the BIOS supports logical drives A and B on a single
 * physical drive, it should return both bits set if a floppy is present.
 */

LONG blkdev_drvmap(void)
{
    return(drvbits);
}



/*
 * blkdev_avail - Read drive bitmapCheck drive availability
 *
 * Returns 0, if drive not available
 */

LONG blkdev_avail(WORD dev)
{
    return((1L << dev) & drvbits);
}
