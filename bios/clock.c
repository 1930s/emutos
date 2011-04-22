/*
 * clock.c - BIOS time and date routines
 *
 * Copyright (c) 2001-2008 EmuTOS development team
 *
 * Authors:
 *  MAD   Martin Doering
 *  LVL   Laurent Vogel
 *  THH   Thomas Huth
 *  joy   Petr Stehlik
 *
 * This file is distributed under the GPL, version 2 or at your
 * option any later version.  See doc/license.txt for details.
 */


#include "portab.h"
#include "bios.h"
#include "kprint.h"
#include "clock.h"
#include "ikbd.h"
#include "tosvars.h"
#include "string.h"
#include "vectors.h"
#include "nvram.h"
#include "config.h"
#include "machine.h"
#include "cookie.h"


#if TOS_VERSION < 0x200
#define NO_NVRAM 1
#endif

/* set this to 1 to debug IKBD clock (you should also disable NVRAM) */
#define NO_MEGARTC 0

/* set this to 1 to not use IKBD clock */
#define NO_IKBD_CLOCK 0


/*==== MegaRTC section ====================================================*/

/* one if there is a MegaST real-time clock. */
int has_megartc;

#define CLK_BASE (0xfffffc20L)

#define CLK     struct clkreg
CLK     /*  needs name for casting      */
{
        BYTE fill0,  sec_l;     /* seconds elapsed in current minute */
        BYTE fill2,  sec_h;
        BYTE fill4,  min_l;     /* minutes elapsed in current hour   */
        BYTE fill6,  min_h;
        BYTE fill8,  hour_l;    /* hours elapsed in current day      */
        BYTE filla,  hour_h;
        BYTE fillc,  daywk;     /* day of week (1-7); sunday=1       */
        BYTE fille,  day_l;     /* day of month (1-31) */
        BYTE fill10, day_h;
        BYTE fill12, mon_l;     /* month of year (1-12) */
        BYTE fill14, mon_h;
        BYTE fill16, year_l;    /* year of century (0-99) */
        BYTE fill18, year_h;
        BYTE fill1a, rega;      /* register A */
        BYTE fill1c, regb;      /* register B */
        BYTE fill1e, regc;      /* register C */
};

#define clk (*(volatile CLK*)CLK_BASE)

struct myclkreg {
        BYTE sec_l;     /* seconds elapsed in current minute */
        BYTE sec_h;
        BYTE min_l;     /* minutes elapsed in current hour   */
        BYTE min_h;
        BYTE hour_l;    /* hours elapsed in current day      */
        BYTE hour_h;
        BYTE daywk;     /* day of week (1-7); sunday=1       */
        BYTE day_l;     /* day of month (1-31) */
        BYTE day_h;
        BYTE mon_l;     /* month of year (1-12) */
        BYTE mon_h;
        BYTE year_l;    /* year of century (0-99) */
        BYTE year_h;
};

/* buffers to hols the megartc regs */
static struct myclkreg clkregs1, clkregs2;


void detect_megartc(void)
{
  has_megartc = 0;
#if !NO_MEGARTC
  /* first check if the address is valid */
  if (check_read_byte(CLK_BASE+1)) {
    if ((UBYTE)clk.sec_l != 0xff && (UBYTE)clk.sec_h != 0xff) {
      has_megartc = 1;
    }
  }
#endif /* NO_MEGARTC */
  
#if NEEDED
  /* 
   * Then this is what TOS 1.2 seems to do. However emulators like
   * STonX do not accept writing to the RTC registers, even in the
   * 'second bank' (if this is what setting rega := 9 means).
   * For this reason it is commented out.
   */
  if (has_megartc) {
    /* use 'second bank' (?), write some test (?) values in registers */
    clk.rega = 9;
    clk.min_l = 10;
    clk.min_h = 5;
    if((clk.min_l != 10) || (clk.min_h != 5)) {
      has_megartc = 0;
    } else {
      /* set back to 'first bank' (?), do some initialisation (?) */
      has_megartc = 1;
      clk.sec_l = 1;
      clk.rega = 8;
      clk.regb = 0;
    }
  }
#endif /* NEEDED */
}

/*==== MegaRTC internal functions =========================================*/

/*
 * MegaRTC, TODO:
 * - leap year ?
 */

/* read the 13 non-control clock registers into clkregs1
 * read the registers twice, and returns only when the two reads 
 * returned the same value.
 * This is because the MegaRTC clock is a very slow chip (32768 kHz)
 * and presumably the carry is not reported instantly when the
 * time changes!!! (this is LVL interpretation, any other reason 
 * is welcome.)
 */

static void mgetregs(void)
{
  int i;
  BYTE *a, *b, *c;
  do {
    c = (BYTE *) &clk.sec_l;
    a = (BYTE *) &clkregs1.sec_l;
    for (i = 0 ; i < 13 ; i++) {
      *a++ = *c;
      c += 2;
    }
    c = (BYTE *) &clk.sec_l;
    b = (BYTE *) &clkregs2.sec_l;
    a = (BYTE *) &clkregs1.sec_l;
    for (i = 0 ; i < 13 ; i++) {
      *b = *c;
      if(*b++ != *a++) break;
      c += 2;
    }
  } while(i != 13);
}

static void msetregs(void)
{
  int i;
  BYTE *a, *c;
  c = (BYTE *) &clk.sec_l;
  a = (BYTE *) &clkregs1.sec_l;
  for (i = 0 ; i < 13 ; i++) {
    *c = *a++;
    c += 2;
  }
}


static void mdosettime(UWORD time)
{
  clkregs1.sec_l = (time * 2) % 10;
  clkregs1.sec_h = (time & 0x1F) / 5;
  clkregs1.min_l = ((time >> 5) & 0x2F) % 10;
  clkregs1.min_h = ((time >> 5) & 0x2F) / 10;
  clkregs1.hour_l = ((time >> 11) & 0x1F) % 10;
  clkregs1.hour_h = ((time >> 11) & 0x1F) / 10;
}


static UWORD mdogettime(void)
{
  UWORD time;

  time = (((clkregs1.sec_l & 0xf) + 10 * (clkregs1.sec_h & 0xF)) >> 1)
    |  (((clkregs1.min_l & 0xf) + 10 * (clkregs1.min_h & 0xf)) << 5)
    |  (((clkregs1.hour_l & 0xf) + 10 * (clkregs1.hour_h & 0xf)) << 11) ;
 
  return time;
}


static void mdosetdate(UWORD date)
{
  clkregs1.day_l = (date & 0x1F) % 10;
  clkregs1.day_h = (date & 0x1F) / 10;
  clkregs1.mon_l = ((date >> 5) & 0xF) % 10;
  clkregs1.mon_h = ((date >> 5) & 0xF) % 10;
  clkregs1.year_l = (date >> 9) % 10;
  clkregs1.year_h = (date >> 9) / 10;  
}

static UWORD mdogetdate(void)
{
  UWORD date;

  /* The MegaRTC stores the year as the offset from 1980.
     Fortunately, this is exactly the same as in the BIOS format. */

  date = ((clkregs1.day_l & 0xf) + 10 * (clkregs1.day_h & 0xf))
    |  (((clkregs1.mon_l & 0xf) + 10 * (clkregs1.mon_h & 0xf)) << 5) 
    |  (((clkregs1.year_l & 0xf) + 10 * (clkregs1.year_h & 0xf)) << 9) ;
 
  return date;
}

/*==== MegaRTC high-level functions ========================================*/

static ULONG mgetdt(void)
{
  mgetregs();
  return (((ULONG) mdogetdate()) << 16) | mdogettime();
}

static void msetdt(ULONG dt)
{
  mdosetdate(dt>>16);
  mdosettime(dt);
  msetregs();
}


/*==== NVRAM RTC internal functions =======================================*/

#if ! NO_NVRAM

/*
 * The MC146818 was used as the RTC and NVRAM in MegaSTE, TT and Falcon.
 * You can find a header file in /usr/src/linux/include/linux/mc146818rtc.h
 * Proper implementation of RTC functions is in linux/arch/m68k/atari/time.c.
 * The code below is just my quick hack. It works but it could not be used
 * for updating real RTC because it doesn't handle the control registers
 * and also doesn't provide proper timing (32kHz device needs proper timing).
 * Reading of RTC should be OK on real machines.
 * (joy)
 */
#define NVRAM_RTC_SECONDS 0
#define NVRAM_RTC_MINUTES 2
#define NVRAM_RTC_HOURS   4
#define NVRAM_RTC_DAYS    7
#define NVRAM_RTC_MONTHS  8
#define NVRAM_RTC_YEARS   9

/*
 * this is an interesting moment: we should detect the year offset in the RTC
 * but it depends on the TOS version that wrote the value there.
 * It seems that MegaSTE (regardless of TOS version) does have the offset 1970.
 * But in the TT030 case it's unsure - with old TOS (3.01, 3.05) it's 1970, but
 * with latest TT TOS (3.06) it's 1968. This is completely crazy and only Atari
 * engineers could do something like that.
 * With Falcon TOS 4.0x the situation is clear - the offset is always 1968.
 * So we try to figure out the machine we're running on in clock_init and set
 * the year offset accordingly to what we've detected.
 */
static int nvram_rtc_year_offset;

static void ndosettime(UWORD time)
{
  set_nvram_rtc(NVRAM_RTC_SECONDS, (time & 0x1F) * 2);
  set_nvram_rtc(NVRAM_RTC_MINUTES, (time >> 5) & 0x2F);
  set_nvram_rtc(NVRAM_RTC_HOURS, (time >> 11) & 0x1F);
}

static UWORD ndogettime(void)
{
  UWORD time;

  time = (get_nvram_rtc(NVRAM_RTC_SECONDS) >> 1)
    |  ( get_nvram_rtc(NVRAM_RTC_MINUTES) << 5)
    |  ( get_nvram_rtc(NVRAM_RTC_HOURS) << 11) ;
 
  return time;
}

static void ndosetdate(UWORD date)
{
/*
  set_nvram_rtc(NVRAM_RTC_DAYS, date & 0x1F);
  set_nvram_rtc(NVRAM_RTC_MONTHS, (date >> 5) & 0xF);
  set_nvram_rtc(NVRAM_RTC_YEARS, (date >> 9) - nvram_rtc_year_offset);
*/
}

static UWORD ndogetdate(void)
{
  UWORD date;

  date = (get_nvram_rtc(NVRAM_RTC_DAYS) & 0x1F)
    |  ((get_nvram_rtc(NVRAM_RTC_MONTHS) & 0xF) << 5) 
    |  ((get_nvram_rtc(NVRAM_RTC_YEARS) + nvram_rtc_year_offset) << 9);
 
  return date;
}

/*==== NVRAM RTC high-level functions ======================================*/

static ULONG ngetdt(void)
{
  return (((ULONG) ndogetdate()) << 16) | ndogettime();
}

static void nsetdt(ULONG dt)
{
  ndosetdate(dt >> 16);
  ndosettime(dt);
}

#endif /* ! NO_NVRAM */


/*==== IKBD clock section =================================================*/

static struct ikbdregs {
  UBYTE cmd;
  UBYTE year;
  UBYTE month;
  UBYTE day;
  UBYTE hour;
  UBYTE min;
  UBYTE sec;
} iclkbuf;

static volatile WORD iclk_ready;

/* called by the ACIA interrupt */
void clockvec(BYTE *buf)
{
  BYTE *b = 1 + ((BYTE *)&iclkbuf);
  memmove(b, buf, 6);
  iclk_ready = 1;
}

static inline UBYTE int2bcd(UWORD a)
{
  return (a % 10) + ((a / 10) << 4);
}

static inline UWORD bcd2int(UBYTE a)
{
  return (a & 0xF) + ((a >> 4) * 10);
}

/*==== Ikbd clock internal functions ======================================*/


static void igetregs(void)
{
#if !NO_IKBD_CLOCK
  long delay;
  iclk_ready = 0;
  iclkbuf.cmd = 0x1C;
  ikbdws(0, (PTR) &iclkbuf);

  /* wait until the interrupt receives the full packet */
  delay = 8000000;
  while(!iclk_ready && delay > 0) {
    --delay;
  }

  kprintf("iclkbuf: year = %x, month = %x\n", iclkbuf.year, iclkbuf.month);
#endif /* !NO_IKBD_CLOCK */
}

#if 0  /* currently unused */
static void iresetregs(void)
{
#if !NO_IKBD_CLOCK
  iclkbuf.cmd   = 0x1B;
  iclkbuf.year  = 0xFF;
  iclkbuf.month = 0xFF;
  iclkbuf.day   = 0xFF;
  iclkbuf.hour  = 0xFF;
  iclkbuf.min   = 0xFF;
  iclkbuf.sec   = 0xFF;
#endif /* !NO_IKBD_CLOCK */
}
#endif

static void isetregs(void)
{
#if !NO_IKBD_CLOCK
  iclkbuf.cmd = 0x1B;
  ikbdws(6, (PTR) &iclkbuf);
#endif /* !NO_IKBD_CLOCK */
}

static UWORD idogetdate(void)
{
  UWORD date;

#if !NO_IKBD_CLOCK
  /* guess the real year from IKBD data */
  UWORD year = bcd2int(iclkbuf.year);
  if (year < 80)
    year += 2000;
  else
    year += 1900;

  date = ((year-1980) << 9)
    | ( bcd2int(iclkbuf.month) << 5 ) | bcd2int(iclkbuf.day);
#else
  date = os_dosdate; /* default date if no IKBD clock */
#endif /* !NO_IKBD_CLOCK */
  return date;
}

static UWORD idogettime(void)
{
  UWORD time;

#if !NO_IKBD_CLOCK
  time = ( bcd2int(iclkbuf.sec) >> 1 )
    | ( bcd2int(iclkbuf.min) << 5 ) | ( bcd2int(iclkbuf.hour) << 11 ) ;
#else
  time = 0;     /* default time if no IKBD clock */
#endif /* !NO_IKBD_CLOCK */
  return time;
}

static void idosettime(UWORD time) 
{
#if !NO_IKBD_CLOCK
  iclkbuf.sec = int2bcd( (time << 1) & 0x3f );
  iclkbuf.min = int2bcd( (time >> 5) & 0x3f );
  iclkbuf.hour = int2bcd( (time >> 11) & 0x1f );
#endif /* !NO_IKBD_CLOCK */
}

static void idosetdate(UWORD date) 
{
#if !NO_IKBD_CLOCK
  iclkbuf.year = int2bcd( ((date >> 9) & 0x7f) + 80 );
  iclkbuf.month = int2bcd( (date >> 5) & 0xf );
  iclkbuf.day = int2bcd( date & 0x1f );
#endif /* !NO_IKBD_CLOCK */
}


/*==== Ikbd Clock high-level functions ====================================*/

static ULONG igetdt(void)
{
  igetregs();
  return (((ULONG)idogetdate()) << 16) | idogettime();
}

static void isetdt(ULONG dt)
{
  idosetdate(dt>>16);
  idosettime(dt);
  isetregs();
}


/* internal init */

void clock_init(void)
{
#if ! NO_NVRAM
  if(has_nvram) {
    /* On Mega-STE and early TTs the year offset in the NVRAM is different */
    if (cookie_mch < MCH_TT || (cookie_mch == MCH_TT && TOS_VERSION <= 0x0305)) {
        nvram_rtc_year_offset = 1970 - 1980;
    }
    else {
        nvram_rtc_year_offset = 1968 - 1980;
    }
  }
#endif /* ! NO_NVRAM */

  if( ! (has_nvram || has_megartc) ) {
    /* no megartc, the best we can do is set the date to the
     * OS creation date, time 0.
     */
    isetdt(((ULONG) os_dosdate) << 16);
  }
}

/* xbios functions */

void settime(LONG time)
{
#if ! NO_NVRAM
  if(has_nvram) {
    nsetdt(time);
  }
  else 
#endif /* ! NO_NVRAM */
  if(has_megartc) {
    msetdt(time);
  }
  else {
    isetdt(time);
  }
}

LONG gettime(void)
{
#if ! NO_NVRAM
  if(has_nvram) {
    return ngetdt();
  }
  else 
#endif /* ! NO_NVRAM */
  if(has_megartc) {
    return mgetdt();
  }
  else {
    return igetdt();
  }
}
