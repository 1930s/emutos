/*
 * config.h - default settings
 *
 * Copyright (c) 2001 Martin Doering
 *
 * Authors:
 *  MAD     Martin Doering
 *
 * This file is distributed under the GPL, version 2 or at your
 * option any later version.  See doc/license.txt for details.
 */

#ifndef _CONFIG_H
#define _CONFIG_H

/* contains all default-drive dependant info */
#ifndef DEFDRV
#define	DEFDRV 2		/* 0 = A:, 2 = C: */
#endif

/* set this to 1 if your emulator provides an STonX-like 
 * native_print() function, i.e. if the code:
 *   dc.w 0xa0ff
 *   dc.l 0
 * executes native function void print_native(WORD char);
 */
#define STONX_NATIVE_PRINT 1

#endif /* _CONFIG_H */
