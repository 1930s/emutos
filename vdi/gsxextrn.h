/*
 * gsxextrn.h - External definitions
 *
 * Copyright (c) 1999 Caldera, Inc.
 *               2002 The EmuTOS development team
 *
 *
 * This file is distributed under the GPL, version 2 or at your
 * option any later version.  See doc/license.txt for details.
 */



#ifndef _GSXEXTRN_H
#define _GSXEXTRN_H

#include "portab.h"

#include "fontdef.h"
#include "attrdef.h"

#include "asm.h"


extern struct attribute virt_work;      /* Virtual workstation attributes */
extern struct attribute *cur_work;      /* Pointer to current works attr. */

extern WORD DDA_INC;            /* the fraction to be added to the DDA */
extern WORD T_SCLSTS;           /* 0 if scale down, 1 if enlarge */

extern WORD FLIP_Y;             /* True if magnitudes being returned */
extern WORD MONO_STATUS;        /* True if current font monospaced */

extern WORD deftxbuf[];         /* Default text scratch buffer */
extern WORD scrtsiz;            /* Default offset to large text buffer */

extern WORD scrpt2;             /* Offset to large text buffer */
extern WORD *scrtchp;           /* Pointer to text scratch buffer */

extern WORD font_count;         /* Number of fonts in driver */

extern struct font_head first;  /* The small system font */

extern struct font_head *cur_font;      /* Current font */
extern struct font_head *def_font;      /* Default font from open workstation 
                                         */

extern struct font_head *font_ring[];   /* Ring of available fonts */

extern WORD h_align;            /* Text horizontal alignment */
extern WORD v_align;            /* Text vertical alignment */
extern WORD STYLE;              /* Requested text special effects */
extern WORD DOUBLE;             /* True if current font scaled */
extern WORD CHUP;               /* Text baseline vector */

extern WORD line_cw;            /* Linewidth for current circle */
extern WORD num_qc_lines, q_circle[];

extern WORD val_mode, chc_mode, loc_mode, str_mode;

/* filled area variables */

extern WORD y, odeltay, deltay, deltay1, deltay2;
extern WORD fill_miny, fill_maxy;
extern WORD fil_intersect;
extern WORD fill_buffer[];
extern WORD *patptr, patmsk;
extern WORD multifill;

/* gdp area variables */

extern WORD xc, yc, xrad, yrad, del_ang, beg_ang, end_ang;
extern WORD start, angle, n_steps;

/* attribute environment save variables */

extern WORD s_fill_per, *s_patptr, s_patmsk;
extern WORD s_begsty, s_endsty, s_fil_col;

extern WORD CLIP, XMN_CLIP, XMX_CLIP, YMN_CLIP, YMX_CLIP;
extern WORD LINE_STYLE[];
extern WORD DITHER[];
extern WORD HATCH0[], HATCH1[], OEMPAT[];
extern WORD ROM_UD_PATRN[];
extern WORD SOLID;
extern WORD HOLLOW;
extern WORD HAT_0_MSK, HAT_1_MSK;
extern WORD DITHRMSK, OEMMSKPAT;

extern WORD DEV_TAB[];          /* initial intout array for open workstation */
extern WORD SIZ_TAB[];          /* initial ptsout array for open workstation */
extern WORD INQ_TAB[];          /* extended inquire values */

extern WORD *CONTRL, *INTIN, *PTSIN, *INTOUT, *PTSOUT;

extern WORD FG_BP_1, FG_BP_2, FG_BP_3, FG_BP_4;

extern WORD LN_MASK, LSTLIN;
extern WORD HIDE_CNT;
extern WORD MOUSE_BT;
extern WORD WRT_MODE;
extern WORD REQ_COL[3][MAX_COLOR];
extern WORD MAP_COL[], REV_MAP_COL[];
extern WORD X1, Y1, X2, Y2;
extern WORD GCURX, GCURY, TERM_CH;

/* Bit-Blt variables */

extern WORD COPYTRAN;

/* Assembly Language Support Routines */

/* ABLINE destroys nearly all CPU registers, and since its a real beast
   of function, I wasn't able to modify it so that it saves and restores
   the registers - so I'm now using regsafe_call() here! */
extern void _ABLINE();
#define ABLINE()  regsafe_call(_ABLINE)

extern void CLEARMEM();
extern void CHK_ESC(), INIT_G(), DINIT_G();
extern void CLC_FLIT();
extern WORD CLC_DDA(WORD top, WORD height);
extern WORD ACT_SIZ(WORD top);
extern WORD SMUL_DIV();

/* Assembly Language Support Routines NEWLY ADDED */

extern void DIS_CUR();
extern void HIDE_CUR(), TEXT_BLT();
extern void XFM_CRFM(), XFM_UNDL(), COPY_RFM(), VEX_CURV();
extern void VEX_BUTV(), VEX_MOTV(), RECTFILL();

extern WORD GSHIFT_S();
extern WORD GLOC_KEY();
extern WORD GCHC_KEY();
extern WORD GCHR_KEY();

extern WORD vec_len(WORD x, WORD y);
extern void fill_line(WORD left, WORD right, WORD val);
extern WORD end_pts(WORD x, WORD y, WORD *xleftout, WORD *xrightout);

/* C Support routines */

extern WORD isin();
extern WORD icos();
extern void text_init();
extern void st_fl_ptr();
extern WORD screen();
extern void d_justified();
extern void arb_corner(WORD * corners, WORD type);
extern WORD Icos(WORD angle);
extern WORD Isin(WORD angle);
extern void r_fa_attr();
extern void s_fa_attr();
extern void arrow(WORD * xy, WORD inc);
extern void crunch_Q();
extern void init_wk();
/* C Support routines */

extern WORD VEC_LEN();


#endif                          /* GSXEXTRN_H */
