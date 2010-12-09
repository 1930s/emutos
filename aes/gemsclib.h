/*
 * EmuTOS aes
 *
 * Copyright (c) 2002, 2010 EmuTOS development team
 *
 * This file is distributed under the GPL, version 2 or at your
 * option any later version.  See doc/license.txt for details.
 */

#ifndef GEMSCLIB_H
#define GEMSCLIB_H


extern LONG  ad_scrap;

WORD sc_read(LONG pscrap);
WORD sc_write(LONG pscrap);
WORD sc_clear(void);


#endif
