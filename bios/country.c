/*
 * country.c - _AKP, _IDT and country-dependant configuration
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
 * read doc/country.txt for information about country-related issues.
 */

#include "config.h"
#include "portab.h"
#include "cookie.h"
#include "country.h"
#include "nvram.h"
#include "tosvars.h"
#include "i18nconf.h"
#include "header.h"  /* contains the default country number */

/*
 * country tables - we define the data structures here, then include the
 * actual tables. The code reading these tables is below.
 */

struct country_record {
    int country;      /* country code */
    char *lang_name;  /* name used to retrieve translations */
    int keyboard;     /* keyboard layout code */
    int charset;      /* charset code */
    int idt;          /* international date and time */
};

struct kbd_record {
    int number;
    const struct keytbl *keytbl;
};

struct charset_fonts {
    int charset;
    const struct font_head *f6x6;
    const struct font_head *f8x8;
    const struct font_head *f8x16;
};

/* 
 * ctables.h contains the following tables: 
 * static const struct country_record countries[];
 * static const struct kbd_record avail_kbd[];
 * static const struct charset_fonts font_sets[];
 */

#include "ctables.h"

/*
 * 
 */

long cookie_idt;
long cookie_akp;

#if CONF_UNIQUE_COUNTRY
const char *get_lang_name(void)
{
    return CONF_LANG;
}

void detect_akp_idt(void)
{
    cookie_akp = (OS_COUNTRY << 8) | CONF_KEYB;
    cookie_idt = IDT_24H | IDT_DDMMYY | '/';
}

#else

static int get_country_index(int country_code)
{
    int i;
    for(i = 0 ; i < sizeof(countries)/sizeof(*countries) ; i++) {
        if(countries[i].country == country_code) {
            return i;
        }
    }
    return 0; /* default is US */
}

void detect_akp_idt(void)
{
#if CONF_WITH_NVRAM
    char buf[4];
    int err;
  
    err = nvmaccess(0, 6, 4, (PTR) buf);
    if(err == 0) { 
        cookie_akp = (buf[0] << 8) | buf[1];
        cookie_idt = (buf[2] << 8) | buf[3]; 
    }
    else
#endif
    {
        /* either no NVRAM, or the NVRAM is corrupt (low battery, 
         * bad cksum), interpret the os_pal flag in header 
         */
        int i = get_country_index(os_pal >> 1);
    
        cookie_akp = (countries[i].country << 8) | countries[i].keyboard;
        cookie_idt = countries[i].idt;
    }
}

static int get_kbd_number(void)
{
    int i = get_country_index(cookie_akp & 0xff);
    return countries[i].keyboard;
}

const char *get_lang_name(void)
{
    int i = get_country_index((cookie_akp >> 8) & 0xFF);
    return countries[i].lang_name;
}

static int get_charset(void)
{
    int i = get_country_index((cookie_akp >> 8) & 0xFF);
    return countries[i].charset;
}

#endif

/*
 * get_keytbl - initialize country dependant keyboard layouts
 */

void get_keytbl(const struct keytbl **tbl)
{
    int j;
#if ! CONF_UNIQUE_COUNTRY
    int i;
    int goal = get_kbd_number();   

    for (i = j = 0 ; i < sizeof(avail_kbd) / sizeof(*avail_kbd) ; i++) {
        if (avail_kbd[i].number == goal) {
            j = i;
            break;
        }
    }
#else
    /* use the unique keyboard anyway */
    j = 0;
#endif
    *tbl = avail_kbd[j].keytbl;
}

/*
 * get_fonts - initialize country dependant font tables
 */

void get_fonts(struct font_head **f6x6, 
               struct font_head **f8x8, 
               struct font_head **f8x16)
{
    int j = 0;

#if ! CONF_UNIQUE_COUNTRY
    int i;
    int charset = get_charset();
    
    /* find the index of the required charset in our font table */
    for(i = 0; i < sizeof(font_sets)/sizeof(*font_sets) ; i++) {
        if( font_sets[i].charset == charset ) {
            j = i; 
            break;
        }
    }
#endif

    *f6x6 = (struct font_head *) font_sets[j].f6x6;
    *f8x8 = (struct font_head *) font_sets[j].f8x8;  
    *f8x16 = (struct font_head *) font_sets[j].f8x16;
}

