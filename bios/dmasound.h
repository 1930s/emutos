/*
 * dmasound.h - STe/TT/Falcon DMA sound routines
 *
 * Copyright (c) 2011 The EmuTOS development team
 *
 * Authors:
 *  VRI   Vincent Rivière
 *
 * This file is distributed under the GPL, version 2 or at your
 * option any later version.  See doc/license.txt for details.
 */

#ifndef DMASOUND_H
#define DMASOUND_H

#if CONF_WITH_DMASOUND

void detect_dmasound(void);
void dmasound_init(void);

/* XBIOS DMA sound functions */
LONG locksnd(void);
LONG unlocksnd(void);
LONG soundcmd(WORD mode, WORD data);
LONG setbuffer(UWORD mode, ULONG startaddr, ULONG endaddr);
LONG setsndmode(UWORD mode);
LONG setinterrupt(UWORD mode, WORD cause);
LONG buffoper(WORD mode);
LONG devconnect(WORD source, WORD dest, WORD clk, WORD prescale, WORD protocol);
LONG sndstatus(WORD reset);
LONG buffptr(LONG sptr);

#endif /* CONF_WITH_DMASOUND */

#endif /* DMASOUND_H */
