/*
 * ikbd.h - Intelligent keyboard routines
 *
 * Copyright (c) 2001 EmuTOS development team
 *
 * Authors:
 *  LVL   Laurent Vogel
 *
 * This file is distributed under the GPL, version 2 or at your
 * option any later version.  See doc/license.txt for details.
 */

#ifndef IKBD_H
#define IKBD_H

#include "portab.h"

extern BYTE shifty;

/*
 * dead key support: i.e. hitting ^ then a yields �.
 * char codes DEAD(1) to DEAD(DEADMAX) included are reserved for dead 
 * keys. table keytbl->dead[i-1], 1 <= i <= DEADMAX, gives the list of 
 * couples of (before, after) char codes ended by zero.
 */

#define DEAD(a) (1+a)
#define DEADMAX 7

struct keytbl {
  /* 128-sized array giving char codes for each scan code */
  BYTE *norm;
  BYTE *shft;
  BYTE *caps;
  /* couples of (scan code, char code), ended by byte zero */
  BYTE *altnorm;
  BYTE *altshft;
  BYTE *altcaps;
  /* table of at most eight dead key translation tables */
  BYTE **dead;
};

/* initialise the ikbd */
extern void kbd_init(void);

/* called by ikbdvec to handle key events */
extern void kbd_int(WORD scancode);

/* called by timer C int to handle key repeat */
extern void kb_timerc_int(void);

/* some bios functions */
extern LONG bconstat2(void);
extern LONG bconin2(void);
extern LONG bcostat4(void);
extern void bconout4(WORD dev, WORD c);
extern LONG kbshift(WORD flag);

/* advanced ikbd functions */
extern void ikbd_pause(void);
extern void ikbd_resume(void);
extern void ikbd_reset(void);
extern void atari_kbd_leds (UWORD );

/* some xbios functions */
extern LONG keytbl(LONG norm, LONG shft, LONG caps);
extern WORD kbrate(WORD initial, WORD repeat);
extern void bioskeys(void);

extern void ikbdws(WORD cnt, LONG ptr);
extern void ikbd_writeb(BYTE b);
extern void ikbd_writew(WORD w);

#endif /* IKBD_H */


