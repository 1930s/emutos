/*
 * vdi_inp.c - Pointer related input stuff
 *
 * Copyright (c) 2002 The EmuTOS development team
 *
 * This file is distributed under the GPL, version 2 or at your
 * option any later version.  See doc/license.txt for details.
 */



#include "portab.h"
#include "gsxdef.h"
#include "gsxextrn.h"
#include "vdiconf.h"
#include "tosvars.h"
#include "lineavars.h"
#include "mouse.h"
#include "asm.h"


extern void mouse_int();    /* mouse interrupt routine */
extern void mov_cur();      // user button vector
extern void vb_draw();      // user button vector
extern struct param arrow_cdb;

extern void s68(int *);
extern void s68l(long *);

#define kbshift(a)   trap13(0x0B,a)
#define bconstat(a)  trap13(0x01,a)
#define bconin(a)    trap13(0x02,a)
#define bconout(a,b) trap13(0x03,a,b)

#define initmous(a,b,c) trap14(0x00,a,b,c)



/*
 * gshift_s - get shift state
 *
 * returns:   CTL/SHIFT/ALT status
 */
 
WORD gshift_s()
{
    return (kbshift(-1) & 0x000f);
}



/*
 * GCHC_KEY - get choice for choice input
 *
 * returns:   0    nothing happened
 *            1    choice value
 *            2    button pressed
 */

WORD gchc_key()
{
    TERM_CH = 1;                /* 16 bit char info */
    return TERM_CH;
}



/*
 * gchr_key - get char for string input
 *
 * returns:  1     button pressed
 *           0     nothing happened
 * 
 * TERM_CH         16 bit char info
 */

WORD gchr_key()
{
    ULONG ch;

    if (bconstat(2)) {                  // see if a character present at con
        ch = bconin(2);
        TERM_CH = (WORD)
            (ch >> 8)|                  // scancode down to bit 8-15
            (ch & 0xff);                // asciicode to bit 0-7
        return 1;
    }
    return 0;
}



/*
 * gloc_key - get locator key
 *
 * returns:  0    - nothing
 *           1    - button pressed
 *                  TERM_CH = 16 bit char info
 *
 *           2    - coordinate info
 *                     X1 = new x
 *                     Y1 = new y
 *           4    - NOT IMPLIMENTED IN THIS VERSION
 *
 * The variable cur_ms_stat holds the bitmap of mouse status since the last
 * interrupt. The bits are
 *
 * 0 - 0x01 Left mouse button status  (0=up)
 * 1 - 0x02 Right mouse button status (0=up)
 * 2 - 0x04 Reserved
 * 3 - 0x08 Reserved
 * 4 - 0x10 Reserved
 * 5 - 0x20 Mouse move flag (1=moved)
 * 6 - 0x40 Right mouse button status flag (0=hasn't changed)
 * 7 - 0x80 Left mouse button status flag  (0=hasn't changed)
 */

WORD gloc_key()
{
    WORD retval;
    ULONG ch;

    if (cur_ms_stat & 0xc0) {           // some button status bits set?
        if (cur_ms_stat & 0x40)         // if bit 6 set
            TERM_CH = 0x21;             // send terminator code for left key
        else
            TERM_CH = 0x20;             // send terminator code for right key
        cur_ms_stat &= 0x23;            // clear mouse button status (bit 6/7)
        retval = 1;                     // set button pressed flag
    } else {                            // check key stat
        if (bconstat(2)) {              // see if a character present at con
            ch = bconin(2);
            TERM_CH = (WORD)
                (ch >> 8)|              // scancode down to bit 8-15
                (ch & 0xff);            // asciicode to bit 0-7
            retval = 1;                 // set button pressed flag
        } else {
            if (cur_ms_stat & 0x20) {   // if bit #5 set ...
                cur_ms_stat |= ~0x20;   // clear bit 5
                X1 = GCURX;             // set _X1 = _GCURX
                Y1 = GCURY;             // set _Y1 = _GCURY
                retval = 2;
            } else {
                retval = 0;
            }
        }
    }
    return retval;
}



/*
 * do_nothing - doesn't do much  :-)
 */

static void do_nothing()
{
}



/*
 * vdimouse_init - Initializes the mouse (VDI part)
 *
 * entry:          none
 * exit:           none
 */

void vdimouse_init()
{
    WORD * pointer;             /* help for storing LONGs in INTIN */

    user_but = do_nothing;
    user_mot = do_nothing;
    user_cur = mov_cur;        	/* initialize user_cur vector */

    /* Move in the default mouse form (presently the arrow) */
    pointer = INTIN; 		/* save INTIN */
    INTIN = (WORD *)&arrow_cdb; /* it points to the arrow data */
    xfm_crfm();                 /* transform mouse */
    INTIN = pointer;        	/* restore old value */

    MOUSE_BT = 0;               // clear the mouse button state
    cur_ms_stat = 0;            // clear the mouse status
    mouse_flag = 0;             // clear the mouse flag
    draw_flag = 0;              // clear the hide operations counter
    newx = 0;                   // set cursor x-coordinate to 0
    newy = 0;                   // set cursor y-coordinate to 0

    /* vblqueue points to start of vbl_list[] */
    *vblqueue = (LONG)vb_draw;   /* set GEM VBL-routine to vbl_list[0] */

    /* Initialize mouse via XBIOS in relative mode */
    initmous(1, &arrow_cdb, mouse_int);
}



/*
 * vdimouse_exit - deinitialize/disable mouse
 */
 
void vdimouse_exit()
{
    LONG * pointer;             /* help for storing LONGs in INTIN */

    user_but = do_nothing;
    user_mot = do_nothing;
    user_cur = do_nothing;

    pointer = vblqueue;         /* vblqueue points to start of vbl_list[] */
    *pointer = (LONG)vb_draw;   /* set GEM VBL-routine to vbl_list[0] */

    /* disable mouse via XBIOS */
    initmous(0, 0, 0);
}
