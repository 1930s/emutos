/*
 * EmuTOS aes
 *
 * Copyright (c) 2002-2014 The EmuTOS development team
 *
 * This file is distributed under the GPL, version 2 or at your
 * option any later version.  See doc/license.txt for details.
 */

#ifndef OPTIMIZE_H
#define OPTIMIZE_H

WORD sound(WORD isfreq, WORD freq, WORD dura);
void fmt_str(BYTE *instr, BYTE *outstr);
void unfmt_str(BYTE *instr, BYTE *outstr);
void inf_sset(LONG tree, WORD obj, BYTE *pstr);
void inf_numset(LONG tree, WORD obj, ULONG value);
void inf_sget(LONG tree, WORD obj, BYTE *pstr);
WORD inf_gindex(LONG tree, WORD baseobj, WORD numobj);
WORD inf_what(LONG tree, WORD ok, WORD cncl);
WORD wildcmp(BYTE *pwld, BYTE *ptst);
void ins_char(BYTE *str, WORD pos, BYTE chr, WORD tot_len);

#endif
