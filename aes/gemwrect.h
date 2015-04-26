/*
 * EmuTOS aes
 *
 * Copyright (c) 2002-2013 The EmuTOS development team
 *
 * This file is distributed under the GPL, version 2 or at your
 * option any later version.  See doc/license.txt for details.
 */

#ifndef GEMWRECT_H
#define GEMWRECT_H

extern ORECT    *rul;
extern ORECT    gl_mkrect;


void or_start(void);
ORECT *get_orect(void);
void newrect(LONG tree, WORD wh);

#endif
