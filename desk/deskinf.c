/*      DESKINF.C       09/03/84 - 05/29/85     Gregg Morris            */
/*      for 3.0 & 2.1   5/5/86                  MDF                     */
/*      merge source    5/27/87  - 5/28/87      mdf                     */

/*
*       Copyright 1999, Caldera Thin Clients, Inc.
*                 2002-2014 The EmuTOS development team
*
*       This software is licenced under the GNU Public License.
*       Please see LICENSE.TXT for further information.
*
*                  Historical Copyright
*       -------------------------------------------------------------
*       GEM Desktop                                       Version 2.3
*       Serial No.  XXXX-0000-654321              All Rights Reserved
*       Copyright (C) 1985 - 1987               Digital Research Inc.
*       -------------------------------------------------------------
*/

#include "config.h"
#include "portab.h"
#include "string.h"
#include "obdefs.h"
#include "dos.h"
#include "deskapp.h"
#include "deskfpd.h"
#include "deskwin.h"
#include "gembind.h"
#include "deskbind.h"

#include "gemdos.h"
#include "optimize.h"
#include "optimopt.h"
#include "aesbind.h"
#include "deskmain.h"
#include "deskglob.h"
#include "deskgraf.h"
#include "deskdir.h"
#include "deskrsrc.h"
#include "deskinf.h"


/*
 * NOTE: this structure is used to access a subset of the fields
 * in the FNODE structure, so the fields MUST be the same size and
 * sequence as those in the FNODE structure!
 */
typedef struct sfcb
{
    BYTE sfcb_junk;
    BYTE sfcb_attr;
    WORD sfcb_time;
    WORD sfcb_date;
    LONG sfcb_size;
    BYTE sfcb_name[LEN_ZFNAME];
} SFCB;


/*
 * Routine to format DOS style time
 *
 *  15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
 *  <     hh     > <    mm    > <   xx  >
 *  hh = binary 0-23
 *  mm = binary 0-59
 *  xx = binary seconds / 2
 *
 * Outputs 6 bytes, left aligned, filled with spaces:
 *  1245pm (12-hour clock)
 *  0045   (24-hour clock)
 */
static void fmt_time(UWORD time, BYTE *ptime)
{
    BOOL pm;
    WORD hh, mm;

    hh = (time >> 11) & 0x001f;
    mm = (time >> 5) & 0x003f;
    pm = FALSE;

    if (G.g_ctimeform)
    {
        if (hh >= 12)
        {
            hh -= 12;
            pm = TRUE;
        }
        if (hh == 0)
            hh = 12;
        sprintf(ptime,"%02d%02d%s",hh,mm,pm?gl_pmstr:gl_amstr);
    }
    else
    {
        sprintf(ptime,"%02d%02d%s",hh,mm,"  ");
    }
}


/*
 * Routine to format DOS style date
 *
 *  15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
 *  <     yy          > < mm  > <  dd   >
 *  yy = 0 - 119 (1980 - 2099)
 *  mm = 1 - 12
 *  dd = 1 - 31
 *
 * Outputs 6 bytes:
 *  ddmmyy      (English style)
 *  mmddyy      (US style)
 */
static void fmt_date(UWORD date, BYTE *pdate)
{
    WORD dd, mm, yy;

    yy = (1980 + ((date >> 9) & 0x007f)) % 100;
    mm = (date >> 5) & 0x000f;
    dd = date & 0x001f;

    if (G.g_cdateform)
    {
        /* MM-DD-YY */
        sprintf(pdate,"%02d%02d%02d",mm,dd,yy);
    }
    else
    {
        /* DD-MM-YY */
        sprintf(pdate,"%02d%02d%02d",dd,mm,yy);
    }
}


static WORD ob_sfcb(LONG psfcb, BYTE *pfmt)
{
    SFCB sf;
    BYTE *pdst, *psrc;
    BYTE pdate_str[7], ptime_str[7], psize_str[9];
    WORD cnt;

    memcpy(&sf.sfcb_junk, (SFCB *)psfcb, sizeof(SFCB));
    pdst = pfmt;
    psrc = &sf.sfcb_name[0];
    *pdst++ = ' ';
    *pdst++ = (sf.sfcb_attr & F_SUBDIR) ? 0x07 : ' ';
    *pdst++ = ' ';
    while( (*psrc) &&
               (*psrc != '.') )
        *pdst++ = *psrc++;
    while( (pdst - pfmt) < 12 )
        *pdst++ = ' ';
    if (*psrc)
        psrc++;
    while (*psrc)
        *pdst++ = *psrc++;
    while( (pdst - pfmt) < 16 )
        *pdst++ = ' ';
    psrc = &psize_str[0];
    if (sf.sfcb_attr & F_SUBDIR)
        *psrc = 0;
    else
    {
        ULONG size = sf.sfcb_size;
        static const char *fix[4] = { "", "K", "M", "G" };
        int fi = 0;
        while (size >= 10000000L && fi <= 3)
        {
            size = (size + 1023) / 1024;
            fi += 1;
        }
        sprintf(psize_str, "%lu", size);
        strcat(psize_str, fix[fi]);
    }
    for (cnt = 8 - strlen(psrc); cnt--; *pdst++ = ' ')
        ;
    while (*psrc)
        *pdst++ = *psrc++;
    *pdst++ = ' ';
    *pdst++ = ' ';
    fmt_date(sf.sfcb_date, &pdate_str[0]);
    psrc = &pdate_str[0];
    for(cnt = 3; cnt--; )
    {
        *pdst++ = *psrc++;
        *pdst++ = *psrc++;
        if (cnt)
            *pdst++ = '-';
    }
    *pdst++ = ' ';
    *pdst++ = ' ';
    fmt_time(sf.sfcb_time, &ptime_str[0]);
    psrc = &ptime_str[0];
    for (cnt = 2; cnt--; )
    {
        *pdst++ = *psrc++;
        *pdst++ = *psrc++;
        if (cnt)
            *pdst++ = ':';
    }
    *pdst++ = ' ';
    strcpy(pdst, &ptime_str[4]); /* am or pm */
    pdst += 2;

    return (pdst-pfmt);
}


static WORD dr_fnode(UWORD last_state, UWORD curr_state, WORD x, WORD y,
            WORD w, WORD h, LONG psfcb)
{
    WORD len;

    if ((last_state ^ curr_state) & SELECTED)
        bb_fill(MD_XOR, FIS_SOLID, IP_SOLID, x, y, w, h);
    else
    {
        len = ob_sfcb(psfcb, &G.g_tmppth[0]);
        gsx_attr(TRUE, MD_REPLACE, BLACK);
        expand_string(intin, G.g_tmppth);
        gsx_tblt(IBM, x, y, len);
        gsx_attr(FALSE, MD_XOR, BLACK);
    }

    return curr_state;
}


WORD dr_code(PARMBLK *pparms)
{
    GRECT oc;
    WORD  state;

    gsx_gclip(&oc);
    gsx_sclip((GRECT *)&pparms->pb_xc);
    state = dr_fnode(pparms->pb_prevstate, pparms->pb_currstate,
                    pparms->pb_x, pparms->pb_y, pparms->pb_w, pparms->pb_h, pparms->pb_parm);
    gsx_sclip(&oc);

    return state;
}


/*
 * Put up dialog box & call form_do
 */
WORD inf_show(LONG tree, WORD start)
{
    WORD   xd, yd, wd, hd;
    OBJECT *obtree = (OBJECT *)tree;

    form_center(tree, &xd, &yd, &wd, &hd);
    form_dial(FMD_START, 0, 0, 0, 0, xd, yd, wd, hd);
    objc_draw(obtree, ROOT, MAX_DEPTH, xd, yd, wd, hd);
    form_do(tree, start);
    form_dial(FMD_FINISH, 0, 0, 0, 0, xd, yd, wd, hd);

    return TRUE;
}


/*
 * Routine for finishing off a simple ok-only dialog box
 */
static void inf_finish(LONG tree, WORD dl_ok)
{
    OBJECT *obj;

    inf_show(tree, 0);
    obj = (OBJECT *)tree + dl_ok;
    obj->ob_state = NORMAL;
}


/*
 * Routine to put number of files, folders and size into
 * a dialog box
 */
static void inf_fifosz(LONG tree, WORD dl_fi, WORD dl_fo, WORD dl_sz)
{
    OBJECT *obj;

    G.g_ndirs--;    /* reproduce TOS's folder count in this situation */

    obj = (OBJECT *)tree + dl_fi;
    obj->ob_state &= ~DISABLED;
    inf_numset(tree, dl_fi, G.g_nfiles);

    obj = (OBJECT *)tree + dl_fo;
    obj->ob_state &= ~DISABLED;
    inf_numset(tree, dl_fo, G.g_ndirs);

    obj = (OBJECT *)tree + dl_sz;
    obj->ob_state &= ~DISABLED;
    inf_numset(tree, dl_sz, G.g_size);
}


static void inf_dttm(LONG tree, FNODE *pf, WORD dl_dt, WORD dl_tm)
{
    BYTE str[11];

    fmt_date(pf->f_date, str);
    inf_sset(tree, dl_dt, str);

    fmt_time(pf->f_time, str);
    inf_sset(tree, dl_tm, str);
}


/*
 * Count files, folders, and total filesize in a given path
 * Values are stored in G.g_nfiles & friends
 */
static WORD count_ffs(BYTE *path)
{
    G.g_nfiles = G.g_ndirs = G.g_size = 0L;

    return d_doop(0, OP_COUNT, path, path, NULL, NULL, NULL);
}


/************************************************************************/
/* i n f _ f i l e _ f o l d e r                                        */
/************************************************************************/
WORD inf_file_folder(BYTE *ppath, FNODE *pf)
{
    LONG tree;
    WORD more, nmidx, title;
    BYTE attr;
    BYTE srcpth[MAXPATHLEN];
    BYTE dstpth[MAXPATHLEN];
    BYTE poname[LEN_ZFNAME], pnname[LEN_ZFNAME];
    OBJECT *obj;

    tree = G.a_trees[ADFFINFO];
    title = (pf->f_attr & F_SUBDIR) ? STFOINFO : STFIINFO;
    obj = (OBJECT *)tree + FFTITLE;
    obj->ob_spec = (LONG) ini_str(title);
    centre_title(tree);

    strcpy(srcpth, ppath);
    strcpy(dstpth, ppath);
    nmidx = 0;
    while (srcpth[nmidx] != '*')
        nmidx++;

    /*
     * for folders, count the contents & insert the values in the
     * dialog; for files, blank out the corresponding dialog fields
     */
    if (pf->f_attr & F_SUBDIR)
    {
        graf_mouse(HGLASS, 0x0L);
        strcpy(srcpth+nmidx, pf->f_name);
        strcat(srcpth, "\\*.*");
        more = count_ffs(srcpth);
        graf_mouse(ARROW, 0x0L);

        if (!more)
            return FALSE;

        inf_fifosz(tree, FFNUMFIL, FFNUMFOL, FFSIZE);
    }
    else
    {
        obj = (OBJECT *)tree + FFNUMFIL;
        obj->ob_state |= DISABLED;
        inf_sset(tree,FFNUMFIL,"");
        obj = (OBJECT *)tree + FFNUMFOL;
        obj->ob_state |= DISABLED;
        inf_sset(tree,FFNUMFOL,"");
        inf_numset(tree,FFSIZE,pf->f_size);
    }

    fmt_str(pf->f_name, poname);
    inf_sset(tree, FFNAME, poname);
    inf_dttm(tree, pf, FFDATE, FFTIME);

    /*
     * initialise the attributes display
     */
    obj = (OBJECT *)tree + FFRONLY;
    if (pf->f_attr & F_SUBDIR)
        obj->ob_state = DISABLED;
    else if (pf->f_attr & F_RDONLY)
        obj->ob_state = SELECTED;
    else
        obj->ob_state = NORMAL;

    obj = (OBJECT *)tree + FFRWRITE;
    if (pf->f_attr & F_SUBDIR)
        obj->ob_state = DISABLED;
    else if (!(pf->f_attr & F_RDONLY))
        obj->ob_state = SELECTED;
    else
        obj->ob_state = NORMAL;

    inf_show(tree, 0);
    if (inf_what(tree, FFOK, FFCNCL) != 1)
        return FALSE;

    /*
     * user selected OK - we rename and/or change attributes
     */
    graf_mouse(HGLASS, 0x0L);

    more = TRUE;
    inf_sget(tree, FFNAME, pnname);

    /* unformat the strings */
    unfmt_str(poname, srcpth+nmidx);
    unfmt_str(pnname, dstpth+nmidx);

    /*
     * if user has changed the name, do the DOS rename
     */
    if (strcmp(srcpth+nmidx, dstpth+nmidx))
    {
        dos_rename(srcpth, dstpth);
        if ((more = d_errmsg()) != 0)
            strcpy(pf->f_name, dstpth+nmidx);
    }

    /*
     * if user has changed the attributes, tell DOS to change them
     */
    if (!(pf->f_attr & F_SUBDIR))
    {
        attr = pf->f_attr;
        obj = (OBJECT *)tree + FFRONLY;
        if (obj->ob_state & SELECTED)
            attr |= F_RDONLY;
        else
            attr &= ~F_RDONLY;
        if (attr != pf->f_attr)
        {
            dos_chmod(dstpth, F_SETMOD, attr);
            if ((more = d_errmsg()) != 0)
                pf->f_attr = attr;
        }
    }

    graf_mouse(ARROW, 0x0L);

    return more;
}


/************************************************************************/
/* i n f _ d i s k                                                      */
/************************************************************************/
WORD inf_disk(BYTE dr_id)
{
    LONG tree;
    LONG total, avail;
    WORD more;
    BYTE srcpth[MAXPATHLEN];
    BYTE str[12];
    BYTE drive[2];

    graf_mouse(HGLASS, 0x0L);
    tree = G.a_trees[ADDISKIN];

    drive[0] = dr_id;
    drive[1] = NULL;
    srcpth[0] = dr_id;
    srcpth[1] = ':';
    strcpy(srcpth+2, "\\*.*");
    more = count_ffs(srcpth);
    graf_mouse(ARROW, 0x0L);

    if (!more)
        return FALSE;

    dos_space(dr_id - 'A' + 1, &total, &avail);
    dos_label(dr_id - 'A' + 1, str);

    inf_sset(tree, DIDRIVE, drive);
    inf_sset(tree, DIVOLUME, str);

    inf_fifosz(tree, DINFILES, DINFOLDS, DIUSED);
    inf_numset(tree, DIAVAIL, avail);

    inf_finish(tree, DIOK);

    return TRUE;
}


/*
 * Set preferences dialog
 */
WORD inf_pref(void)
{
    OBJECT *tree;
    WORD cyes, cno, i;
    WORD sndefpref;
    WORD rbld;

    tree = (OBJECT *)G.a_trees[ADSETPRE];
    rbld = FALSE;

    cyes = (G.g_cdelepref) ? SELECTED : NORMAL;
    cno = (G.g_cdelepref) ? NORMAL : SELECTED;
    tree[SPCDYES].ob_state = cyes;
    tree[SPCDNO].ob_state = cno;

    cyes = (G.g_ccopypref) ? SELECTED : NORMAL;
    cno = (G.g_ccopypref) ? NORMAL : SELECTED;
    tree[SPCCYES].ob_state = cyes;
    tree[SPCCNO].ob_state = cno;

    cyes = (G.g_covwrpref) ? SELECTED : NORMAL;
    cno = (G.g_covwrpref) ? NORMAL : SELECTED;
    tree[SPCOWYES].ob_state = cyes;
    tree[SPCOWNO].ob_state = cno;

    cyes = (G.g_cmclkpref) ? SELECTED : NORMAL;
    cno = (G.g_cmclkpref) ? NORMAL : SELECTED;
    tree[SPMNCLKY].ob_state = cyes;
    tree[SPMNCLKN].ob_state = cno;

    cyes = (G.g_ctimeform) ? SELECTED : NORMAL;
    cno = (G.g_ctimeform) ? NORMAL : SELECTED;
    tree[SPTF12HR].ob_state = cyes;
    tree[SPTF24HR].ob_state = cno;

    cyes = (G.g_cdateform) ? SELECTED : NORMAL;
    cno = (G.g_cdateform) ? NORMAL : SELECTED;
    tree[SPDFMMDD].ob_state = cyes;
    tree[SPDFDDMM].ob_state = cno;

    for (i = 0; i < 5; i++)
        tree[SPDC1+i].ob_state = NORMAL;

    G.g_cdclkpref = evnt_dclick(0, FALSE);
    tree[SPDC1+G.g_cdclkpref].ob_state = SELECTED;

    sndefpref = !sound(FALSE, 0xFFFF, 0);

    cyes = (sndefpref) ? SELECTED : NORMAL;
    cno = (sndefpref) ? NORMAL : SELECTED;
    tree[SPSEYES].ob_state = cyes;
    tree[SPSENO].ob_state = cno;

    inf_show((LONG)tree, 0);

    if ( inf_what((LONG)tree, SPOK, SPCNCL) )
    {
        G.g_cdelepref = inf_what((LONG)tree, SPCDYES, SPCDNO);
        G.g_ccopypref = inf_what((LONG)tree, SPCCYES, SPCCNO);
        G.g_covwrpref = inf_what((LONG)tree, SPCOWYES, SPCOWNO);
        G.g_cmclkpref = inf_what((LONG)tree, SPMNCLKY, SPMNCLKN);
        G.g_cmclkpref = menu_click(G.g_cmclkpref, TRUE);
        G.g_cdclkpref = inf_gindex((LONG)tree, SPDC1, 5);
        G.g_cdclkpref = evnt_dclick(G.g_cdclkpref, TRUE);
        sndefpref = inf_what((LONG)tree, SPSEYES, SPSENO);

        /* changes if file display? */
        cyes = inf_what((LONG)tree, SPTF12HR, SPTF24HR);
        if (G.g_ctimeform != cyes)
        {
            rbld = (G.g_iview == V_TEXT);
            G.g_ctimeform = cyes;
        }
        cyes = inf_what((LONG)tree, SPDFMMDD, SPDFDDMM);
        if (G.g_cdateform != cyes)
        {
            rbld |= (G.g_iview == V_TEXT);
            G.g_cdateform = cyes;
        }
        sound(FALSE, !sndefpref, 0);
    }

    return rbld;
}


/*
 *       Open application icon
 */
WORD opn_appl(BYTE *papname, BYTE *papparms, BYTE *pcmd, BYTE *ptail)
{
    LONG tree;
    BYTE poname[LEN_ZFNAME];

    tree = G.a_trees[ADOPENAP];

    fmt_str(papname, &poname[0]);
    inf_sset(tree, APPLNAME, &poname[0]);
    inf_sset(tree, APPLPARM, papparms);
    inf_show(tree, APPLPARM);

    /* now find out what happened */
    if ( inf_what(tree, APPLOK, APPLCNCL) )
    {
        inf_sget(tree, APPLNAME, &poname[0]);
        unfmt_str(&poname[0], pcmd);
        inf_sget(tree, APPLPARM, ptail);
        return TRUE;
    }
    else
        return FALSE;
}
