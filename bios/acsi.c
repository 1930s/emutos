/*
 * acsi.c - Atari Computer System Interface (ACSI) support
 *
 * Copyright (c) 2002-2013 The EmuTOS development team
 *
 * Authors:
 *  LVL   Laurent Vogel
 *
 * This file is distributed under the GPL, version 2 or at your
 * option any later version.  See doc/license.txt for details.
 */

/* #define ENABLE_KDEBUG */

#include "config.h"
#include "acsi.h"
#include "dma.h"
#include "kprint.h"
#include "string.h"
#include "mfp.h"
#include "machine.h"
#include "tosvars.h"
#include "gemerror.h"
#include "blkdev.h"
#include "processor.h"
#include "asm.h"
#include "cookie.h"
#include "delay.h"

#if CONF_WITH_ACSI

/*
 * private prototypes
 */
static void acsi_begin(void);
static void acsi_end(void);
static void hdc_start_dma(UWORD control);
static void dma_send_byte(UBYTE data, UWORD control);
static int build_rw_command(UBYTE *cdb,WORD rw,WORD dev,LONG sector,WORD cnt);
static int send_command(UBYTE *cdb,WORD cdblen,WORD rw,WORD dev,WORD cnt);
static int do_acsi_rw(WORD rw, LONG sect, WORD cnt, LONG buf, WORD dev);


/* the following exist to allow the data and control registers to
 * be written together as well as separately.  this avoids the
 * "DMA chip anomaly" (see "Atari ACSI/DMA Integration Guide" p.14).
 */
#define ACSIDMA ((union acsidma *) 0xFFFF8604)

union acsidma {
    volatile ULONG datacontrol;
    struct {
        volatile UWORD data;
        volatile UWORD control;
    } s;
};


/*
 * defines
 */
#define SMALL_TIMEOUT (CLOCKS_PER_SEC/10)   /* 100ms between cmd bytes */
#define LARGE_TIMEOUT (CLOCKS_PER_SEC)      /* 1000ms for the data xfer itself */

/*
 * there should be a minimum of 5msec between ACSI I/Os.  because
 * of the granularity of the system clock, we need 2 ticks ...
 */
#define INTER_IO_TIME 2     /* ticks between I/Os */

/* delay for dma out toggle */
#define delay() delay_loop(loopcount_delay)


/*
 * local variables
 */
static ULONG loopcount_delay;   /* used by delay() macro */
static ULONG next_acsi_time;    /* earliest time we can start the next i/o */


/*
 * High-level ACSI stuff.
 */
void acsi_init(void)
{
    /* the following delay is used between toggling dma out.  in Atari
     * TOSes, the delay is provided by an instruction sequence which
     * takes about 15usec on an ST, 5usec on a TT or Falcon.  we always
     * use 15usec.
     */
    loopcount_delay = 15 * loopcount_1_msec / 1000;

    next_acsi_time = hz_200;    /* immediate :-) */
}

LONG acsi_rw(WORD rw, LONG sector, WORD count, LONG buf, WORD dev)
{
    int cnt;
    int need_frb = 0;
    int retry;
    int err = 0;
    LONG tmp_buf;

    /* read by chunks of at most 0x80 sectors.
     * (0x80 * 512 bytes will fit in the 64 kB buffer _FRB, and cnt
     * must fit in a byte anyway.)
     */
    while(count > 0) {
        cnt = 0x80;
        if(cnt > count)
            cnt = count;

#if CONF_WITH_FRB
        if (buf > 0x1000000L) {
            if (cookie_frb == 0) {
                KDEBUG(("acsi.c: FRB is missing\n"));
                return -1L;
            } else {
                /* proper FRB lock (TODO) */
                need_frb = 1;
                tmp_buf = cookie_frb;
            }
        } else
#endif
        {
            tmp_buf = buf;
        }

        if(rw && need_frb) {
            memcpy((void *)tmp_buf, (void *)buf, (LONG)cnt * SECTOR_SIZE);
        }
        for(retry = 0; retry < 2 ; retry++) {
            err = do_acsi_rw(rw, sector, cnt, tmp_buf, dev);
            if(err == 0) break;
        }
        if((!rw) && need_frb) {
            memcpy((void *)buf, (void *)tmp_buf, (LONG)cnt * SECTOR_SIZE);
        }
        if(need_frb) {
            /* proper FRB unlock (TODO) */
        }
        if(err) {
            KDEBUG(("acsi.c: %s error %d\n",rw?"write":"read",err));
            KDEBUG(("        dev=%d,sector=%ld,cnt=%d\n",dev,sector,cnt));
            return err;
        }

        count -= cnt;
        buf += (LONG)cnt * SECTOR_SIZE;
        sector += cnt;
    }
    return 0;
}

LONG acsi_testunit(WORD dev)
{
    UBYTE cdb[6];
    int status;

    acsi_begin();

    memset(cdb,0x00,6);     /* set up Test Unit Ready cdb */
    status = send_command(cdb,6,RW_READ,dev,0);

    acsi_end();

    return status;
}

/* must call this before manipulating any ACSI-related hardware */
static void acsi_begin(void)
{
    while(hz_200 < next_acsi_time)  /* wait until safe */
        ;

    flock = -1;     /* don't let floppy interfere */
}

/* must call this when finished with ACSI-related hardware */
static void acsi_end(void)
{
    /* put DMA back to floppy and allow floppy i/o */
    ACSIDMA->s.control = DMA_FDC;
    flock = 0;

    next_acsi_time = hz_200 + INTER_IO_TIME;    /* next safe time */
}

/*
 * Internal implementation -
 * cnt <= 0xFF, no retry done, returns -1 if timeout, or the DMA status.
 */

static int do_acsi_rw(WORD rw, LONG sector, WORD cnt, LONG buf, WORD dev)
{
    UBYTE cdb[10];  /* allow for 10-byte read/write commands */
    int status, cdblen;
    UWORD control;
    LONG buflen = (LONG)cnt * SECTOR_SIZE;

    /* flush data cache here so that memory is current */
    if (rw == RW_WRITE)
        flush_data_cache((void *)buf,buflen);

    acsi_begin();

    /* load DMA base address */
    set_dma_addr((ULONG) buf);

    /* emit command */
    cdblen = build_rw_command(cdb,rw,dev,sector,cnt);
    status = send_command(cdb,cdblen,rw,dev,cnt);

    if (status == 0) {      /* no timeout */
        /* read status */
        control = DMA_FDC | DMA_HDC | DMA_A0;
        if (rw == RW_WRITE)
            control |= DMA_WRBIT;
        ACSIDMA->s.control = control;
        status = ACSIDMA->s.data & 0x00ff;
    }
    if (status)
        KDEBUG(("cdb=%02x%02x%02x%02x%02x%02x\n",cdb[0],cdb[1],cdb[2],cdb[3],cdb[4],cdb[5]));

    acsi_end();

    /* invalidate data cache if we've read into memory */
    if (rw == RW_READ)
        invalidate_data_cache((void *)buf,buflen);

    return status;
}

/*
 * build ACSI read/write command
 * returns length of command built
 */
static int build_rw_command(UBYTE *cdb,WORD rw,WORD dev,LONG sector,WORD cnt)
{
    /*
     * when we talk to a device on the ACSI bus, it is almost always a
     * SCSI device accessed via a converter.  so, we make our commands
     * compatible with both ACSI and SCSI.  the maximum sector number
     * for a 6-byte SCSI read/write command is 0x1fffff, corresponding
     * to a 1GB disk. for disks greater than this, we must use the
     * SCSI-only 10-byte read/write commands (this is not a problem
     * since no ACSI disk was as large as 1GB).
     */
    if (sector <= 0x1fffffL) {  /* we can use 6-byte CDBs */
        cdb[0] = (rw==RW_WRITE) ? 0x0a : 0x08;
        cdb[1] = (sector >> 16) & 0x1f;
        cdb[2] = sector >> 8;
        cdb[3] = sector;
        cdb[4] = cnt;
        cdb[5] = 0x00;
        return 6;
    }

    cdb[0] = (rw==RW_WRITE) ? 0x2a : 0x28;
    cdb[1] = 0x00;
    cdb[2] = sector >> 24;
    cdb[3] = sector >> 16;
    cdb[4] = sector >> 8;
    cdb[5] = sector;
    cdb[6] = 0x00;
    cdb[7] = cnt >> 8;
    cdb[8] = cnt;
    cdb[9] = 0x00;
    return 10;
}

/*
 * send an ACSI command; return -1 if timeout
 */
static int send_command(UBYTE *inputcdb,WORD cdblen,WORD rw,WORD dev,WORD cnt)
{
    UWORD control;
    UBYTE cdb[13];      /* allow for 12-byte input commands */
    UBYTE *p;
    int i;

    /*
     * see if we need to use ICD trickery
     */
    if (*inputcdb > 0x1e) {
        p = cdb;
        *p = 0x1f;      /* ICD extended command method */
        memcpy(p+1,inputcdb,cdblen);
        cdblen++;
    } else p = inputcdb;

    *p |= (dev << 5);   /* insert device number */

    if (rw == RW_WRITE) {
        control = DMA_WRBIT | DMA_FDC | DMA_HDC;
    } else {
        control = DMA_FDC | DMA_HDC;
    }
    hdc_start_dma(control);
    ACSIDMA->s.data = cnt;

    ACSIDMA->s.control = control;   /* assert command signal */
    control |= DMA_A0;              /* set up for remaining bytes */

    for (i = 0; i < cdblen-1; i++) {
        dma_send_byte(*p++,control);
        if (timeout_gpip(SMALL_TIMEOUT))
            return -1;
    }

    /* send the last byte & wait for completion of DMA */
    control &= 0xff00;
    dma_send_byte(*p,control);
    if (timeout_gpip(LARGE_TIMEOUT))
        return -1;

    return 0;
}

/*
 * send current byte plus the control for the _next_ byte
 */
static void dma_send_byte(UBYTE data, UWORD control)
{
    ACSIDMA->datacontrol = (((ULONG)data) << 16) | control;
}


/* the hdc_start_dma() function sets 'control' to access the sector
 * count register and then toggles the DMA write bit.  this signals
 * the DMA to clear its internal buffers.
 */
static void hdc_start_dma(UWORD control)
{
    control |= DMA_SCREG;
    ACSIDMA->s.control = control ^ DMA_WRBIT;
    delay();
    ACSIDMA->s.control = control;
    delay();
}

#endif /* CONF_WITH_ACSI */
