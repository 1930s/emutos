/*
 * tosvars.h - name of low-memory variables
 *
 * Copyright (c) 2001 EmuTOS development team
 *
 * Authors:
 *  LVL   Laurent Vogel
 *
 * This file is distributed under the GPL, version 2 or at your
 * option any later version.  See doc/license.txt for details.
 */

/*
 * Put in this file only the low-mem vars actually used by
 * C code.
 */

#ifndef _TOSVARS_H
#define _TOSVARS_H

#include "portab.h"

extern LONG proc_lives;
extern LONG proc_dregs[];
extern LONG proc_aregs[];
extern LONG proc_enum;
extern LONG proc_usp;
extern WORD proc_stk[];

extern BYTE conterm;

extern UBYTE *v_bas_ad;
extern LONG kbdvecs[];

extern WORD *colorptr;
extern UBYTE *screenpt;

extern WORD timer_ms;

extern LONG membot;
extern LONG memtop;


#endif /* _TOSVARS_H */

