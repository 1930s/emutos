/*
 * country.h - _AKP, _IDT and all that stuff
 *
 * Copyright (c) 2001 EmuTOS development team.
 *
 * Authors:
 *  LVL     Laurent Vogel
 *
 * This file is distributed under the GPL, version 2 or at your
 * option any later version.  See doc/license.txt for details.
 */

/*
 * dependencies: this header file must be included after:
 *
 * (no dependency)
 */
 
/* a list of country codes */

/* these are documented in the compendium */
#define COUNTRY_US  0   /* USA */
#define COUNTRY_DE  1   /* Germany */
#define COUNTRY_FR  2   /* France */
#define COUNTRY_UK  3   /* United Kingdom */
#define COUNTRY_ES  4   /* Spain */
#define COUNTRY_IT  5   /* Italy */
#define COUNTRY_SE  6   /* Sweden */
#define COUNTRY_SF  7   /* Switzerland (French) */
#define COUNTRY_SG  8   /* Switzerland (German), NOT Singapore! */

/* the other below were given by Petr */
#define COUNTRY_TR  9   /* Turkey */
#define COUNTRY_FI 10   /* Finland */
#define COUNTRY_NO 11   /* Norway */
#define COUNTRY_DK 12   /* Denmark */
#define COUNTRY_SA 13   /* Saudi Arabia */
#define COUNTRY_NL 14   /* Holland */
#define COUNTRY_CZ 15   /* Czech Republic */
#define COUNTRY_HU 16   /* Hungary */
#define COUNTRY_SK 17   /* Slovak Republic */
 
/* a list of keyboard layout codes */
#define KEYB_US 0
#define KEYB_DE 1
#define KEYB_FR 2

/* charset codes - names in [brackets] are understood by GNU recode */
#define CHARSET_ST 0   /* original [atarist] */
#define CHARSET_CZ 1   /* [Kamenicky] charset, used in CZ */

/* IDT flag 24 hour: 0 = 12am/pm or 1 = 24 hour */
#define IDT_12H   0x0000
#define IDT_24H   0x1000
#define IDT_TMASK 0x1000  /* time mask */

/* IDT format for printing date */
#define IDT_MMDDYY 0x000
#define IDT_DDMMYY 0x100
#define IDT_YYMMDD 0x200
#define IDT_YYDDMM 0x300
#define IDT_DMASK  0x300
#define IDT_SMASK   0xFF  /* date mask */

/* cookies */

extern long cookie_idt;
extern long cookie_akp;

/* used by machine.c */
void detect_akp_idt(void);

/* used by ikbd.c */
int get_kbd_number(void);

/* used by nls.c */
const char *get_lang_name(void);

/* used by initlinea.c */
int get_charset(void);

