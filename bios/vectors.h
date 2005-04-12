/*
 * vectors.h - exception vectors, interrupt routines and system hooks
 *
 * Copyright (c) 2001 EmuTOS development team.
 *
 * Authors:
 *  LVL     Laurent Vogel
 *
 * This file is distributed under the GPL, version 2 or at your
 * option any later version.  See doc/license.txt for details.
 */

#ifndef VECTORS_H
#define VECTORS_H
 
#include "portab.h"
 
/* initialize default exception vectors */

extern void init_exc_vec(void);
extern void init_user_vec(void);
 
/* initialise acia vectors */

extern void init_acia_vecs(void);


/* some exception vectors */

extern void int_hbl(void);
extern void int_vbl(void);
extern void int_linea(void);
extern void int_timerc(void);

extern void gemtrap(void);
extern void biostrap(void);
extern void xbiostrap(void);

extern void just_rte(void);
extern void just_rts(void);

long check_read_byte(long);

/* are these useful ? */
extern void print_stat(void);
extern void print_vec(void);
extern void serial_stat(void);
extern void serial_vec(void);
extern void dump_scr(void);
extern void print_vec(void);

/* */
extern void criter1(void);
extern void int_illegal(void);
extern void int_priv(void);

extern WORD trap_save_area[];

/* pointer to function returning LONG */
typedef LONG (*PFLONG)(void);

/* pointer to function returning VOID */
typedef void (*PFVOID)(void);

#define VEC_ILLEGAL (*(PFVOID*)0x10)    /* illegal instruction vector */
#define VEC_DIVNULL (*(PFVOID*)0x14)    /* division by zero exception vector */
#define VEC_PRIVLGE (*(PFVOID*)0x20)    /* priviledge exception vector */
#define VEC_LINEA   (*(PFVOID*)0x28)    /* LineA interrupt vector */
#define VEC_LEVEL1  (*(PFVOID*)0x64)    /* Level 1 interrupt */
#define VEC_HBL     (*(PFVOID*)0x68)    /* HBL interrupt vector */
#define VEC_LEVEL3  (*(PFVOID*)0x6c)    /* Level 3 interrupt */
#define VEC_VBL     (*(PFVOID*)0x70)    /* VBL interrupt vector */
#define VEC_LEVEL5  (*(PFVOID*)0x74)    /* Level 5 interrupt */
#define VEC_LEVEL6  (*(PFVOID*)0x78)    /* Level 6 interrupt */
#define VEC_NMI     (*(PFVOID*)0x7c)    /* NMI - not maskable interrupt */
#define VEC_AES     (*(PFVOID*)0x88)    /* AES interrupt vector */
#define VEC_BIOS    (*(PFVOID*)0xb4)    /* BIOS interrupt vector */
#define VEC_XBIOS   (*(PFVOID*)0xb8)    /* XBIOS interrupt vector */
#define VEC_ACIA   (*(PFVOID*)0x118)    /* keyboard/Midi interrupt vector */

/* protect d2/a2 when calling external user-supplied code */

LONG protect_v(LONG (*func)(void));
LONG protect_w(LONG (*func)(WORD), WORD);
LONG protect_ww(LONG (*func)(void), WORD, WORD);
LONG protect_wlwwwl(LONG (*func)(void), WORD, LONG, WORD, WORD, WORD, LONG);

#endif /* VECTORS_H */
  
