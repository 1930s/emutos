/*
 * nvram.c - Non-Volatile RAM access
 *
 * Copyright (c) 2001 EmuTOS development team.
 *
 * Authors:
 *  LVL     Laurent Vogel
 *
 * This file is distributed under the GPL, version 2 or at your
 * option any later version.  See doc/license.txt for details.
 */


/* internal feature detection */

extern int has_nvram;
void detect_nvram(void);

/* XBios function */

WORD nvmaccess(WORD type, WORD start, WORD count, PTR buffer);



