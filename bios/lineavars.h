/*
 * lineavars.h - name of linea graphics related variables
 *
 * Copyright (c) 2001 by Authors:
 *
 *  MAD   Martin Doering
 *
 * This file is distributed under the GPL, version 2 or at your
 * option any later version.  See doc/license.txt for details.
 */

/*
 * Put in this file only the low-mem vars actually used by
 * C code.
 */

#ifndef LINEAVARS_H
#define LINEAVARS_H

#include "portab.h"



// ==== Defines ==============================================================

/* Bits for cursor state */
#define M_CFLASH        0x0001  // cursor flash (0:disabled,  1:enabled)
#define M_CSTATE        0x0002  // cursor flash state  (0:off, 1:on)
#define M_CVIS          0x0004  // cursor visibility (0:invisible, 1:visible)


// ==== Variables ============================================================

/* Color related variables */
extern WORD v_col_bg;           // current background color
extern WORD v_col_fg;           // current foreground color

/* Cursor related variables */
extern void os_entry(void);
extern void *v_cur_ad;          // current cursor address
extern WORD v_cur_of;           // cursor offset
extern WORD v_cur_cx;           // current cursor cell x
extern WORD v_cur_cy;           // current cursor cell y
extern BYTE v_cur_tim;          // cursor blink timer.

extern BYTE v_period;           //
extern WORD disab_cnt;          // disable depth count. (>0 means disabled)
extern BYTE v_stat_0;           // video cell system status
extern WORD sav_cur_x;          // saved cursor cell x
extern WORD sav_cur_y;          // saved cursor cell y


/* Screen related variables */
extern UWORD v_planes;          // count of color planes
extern UWORD v_lin_wr;          // line wrap : bytes per line
extern UWORD v_hz_rez;          // screen horizontal resolution
extern UWORD v_vt_rez;          // screen vertical resolution
extern UWORD v_bytes_lin;       // width of line in bytes

extern WORD cursconf(WORD, WORD);       // XBIOS cursor configuration

extern void     (*tim_addr)(int);       // timer interrupt vector
extern void     (*tim_chain)(int);      // timer interrupt vector save
extern void     (*user_but)(void);      // user button vector
extern void     (*user_cur)(void);      // user cursor vector
extern void     (*user_mot)(void);      // user motion vector

/* Mouse specific externals */
extern WORD GCURX;              // mouse X position
extern WORD GCURY;              // mouse Y position
extern WORD HIDE_CNT;           // Number of levels the mouse is hidden
extern WORD MOUSE_BT;           // mouse button state

/* Mouse related variables */
extern WORD     newx;           // new mouse x&y position
extern WORD     newy;           // new mouse x&y position
extern BYTE     draw_flag;      // non-zero means draw mouse form on vblank
extern BYTE     mouse_flag;     // non-zero, if mouse ints disabled
extern BYTE     cur_ms_stat;    /* current mouse status */


#endif /* LINEAVARS_H */
