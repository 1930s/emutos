/*
 *  chardev.c - BIOS character device funtions
 *
 * Copyright (c) 2001 by
 *
 * Authors:
 *  THO     Thomas Huth
 *  LVL     Laurent Vogel
 *  MAD     Martin Doering
 *
 * This file is distributed under the GPL, version 2 or at your
 * option any later version.  See doc/license.txt for details.
 */



#include "portab.h"
#include "bios.h"
#include "gemerror.h"
#include "kprint.h"

/*==== Defines ============================================================*/

#define DBGBIOSC TRUE

/*==== External declarations ==============================================*/

extern void cputc(WORD);        /* found in conout.s */



/*== Bios devices prototypes ==============================================*/

LONG bconstat0(void);
LONG bconstat1(void);
LONG bconstat2(void);
LONG bconstat3(void);
LONG bconstat4(void);
LONG bconstat5(void);
LONG bconstat6(void);
LONG bconstat7(void);

LONG bconin0(void);
LONG bconin1(void);
LONG bconin2(void);
LONG bconin3(void);
LONG bconin4(void);
LONG bconin5(void);
LONG bconin6(void);
LONG bconin7(void);

void bconout0(WORD, WORD);
void bconout1(WORD, WORD);
void bconout2(WORD, WORD);
void bconout3(WORD, WORD);
void bconout4(WORD, WORD);
void bconout5(WORD, WORD);
void bconout6(WORD, WORD);
void bconout7(WORD, WORD);

LONG bcostat0(void);
LONG bcostat1(void);
LONG bcostat2(void);
LONG bcostat3(void);
LONG bcostat4(void);
LONG bcostat5(void);
LONG bcostat6(void);
LONG bcostat7(void);

/* Arrays of BIOS function pointers for fast function calling */

extern LONG (*bconstat_vec[])(void);
extern LONG (*bconin_vec[])(void);
extern void (*bconout_vec[])(WORD, WORD);
extern LONG (*bcostat_vec[])(void);


/*==== BIOS initialization ================================================*/
/*
 *      called from startup.s, this routine will do necessary bios initialization
 *      that can be done in hi level lang.  startup.s has the rest.
 */

void chardev_init()
{
    /* initialise bios device functions */

    bconstat_vec[0] = bconstat0;
    bconstat_vec[1] = bconstat1;
    bconstat_vec[2] = bconstat2;
    bconstat_vec[3] = bconstat4;    /* IKBD and MIDI bconstat swapped */
    bconstat_vec[4] = bconstat3;
    bconstat_vec[5] = bconstat5;
    bconstat_vec[6] = bconstat6;
    bconstat_vec[7] = bconstat7;

    bconin_vec[0] = bconin0;
    bconin_vec[1] = bconin1;
    bconin_vec[2] = bconin2;
    bconin_vec[3] = bconin3;
    bconin_vec[4] = bconin4;
    bconin_vec[5] = bconin5;
    bconin_vec[6] = bconin6;
    bconin_vec[7] = bconin7;
    
    bconout_vec[0] = bconout0;
    bconout_vec[1] = bconout1;
    bconout_vec[2] = bconout2;
    bconout_vec[3] = bconout3;
    bconout_vec[4] = bconout4;
    bconout_vec[5] = bconout5;
    bconout_vec[6] = bconout6;
    bconout_vec[7] = bconout7;

    bcostat_vec[0] = bcostat0;
    bcostat_vec[1] = bcostat1;
    bcostat_vec[2] = bcostat2;
    bcostat_vec[3] = bcostat3;
    bcostat_vec[4] = bcostat4;
    bcostat_vec[5] = bcostat5;
    bcostat_vec[6] = bcostat6;
    bcostat_vec[7] = bcostat7;
}



/* BIOS devices - bconstat functions */

LONG bconstat0(void)
{
  return 0;
}

LONG bconstat1(void)
{
  return 0;
}

LONG bconstat4(void)
{
  return 0;
}

LONG bconstat5(void)
{
  return 0;
}

LONG bconstat6(void)
{
  return 0;
}

LONG bconstat7(void)
{
  return 0;
}



/* BIOS devices - bconin functions */

LONG bconin0(void)
{
  return 0;
}
LONG bconin1(void)
{
  return 0;
}
LONG bconin4(void)
{
  return 0;
}
LONG bconin5(void)
{
  return 0;
}
LONG bconin6(void)
{
  return 0;
}
LONG bconin7(void)
{
  return 0;
}



/* BIOS devices - bconout functions */

void bconout0(WORD dev, WORD b)
{
}

void bconout1(WORD dev, WORD b)
{
}

void bconout2(WORD dev, WORD b)
{
  cputc(b);
}

void bconout6(WORD dev, WORD b)
{
}

void bconout7(WORD dev, WORD b)
{
}



/* BIOS devices - bcostat functions */

LONG bcostat0(void)
{
  return -1;
}

LONG bcostat1(void)
{
  return -1;
}

LONG bcostat2(void)
{
  return -1;
}

LONG bcostat5(void)
{
  return -1;
}

LONG bcostat6(void)
{
  return -1;
}

LONG bcostat7(void)
{
  return -1;
}
