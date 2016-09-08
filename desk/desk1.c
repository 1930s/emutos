/*      DESK1.C         */
/*      Routines specific to Desktop 1.x */
/*
*       Copyright 1999 Caldera Thin Clients, Inc. / Lineo Inc.
*                 2001 John Elliott
*                 2003-2016 The EmuTOS development team
*
*       This software is licenced under the GNU Public License.
*       Please see LICENSE.TXT for further information.
*
*                  Historical Copyright
*       -------------------------------------------------------------
*       GEM Desktop                                       Version 2.3
*       Serial No.  XXXX-0000-654321              All Rights Reserved
*       Copyright (C) 1987                      Digital Research Inc.
*       -------------------------------------------------------------
*/

/* #define ENABLE_KDEBUG */

#include "config.h"
#include "portab.h"
#include "string.h"
#include "deskapp.h"
#include "deskfpd.h"
#include "gembind.h"
#include "obdefs.h"
#include "gsxdefs.h"
#include "deskwin.h"
#include "deskdir.h"
#include "deskbind.h"
#include "deskglob.h"
#include "aesbind.h"
#include "desksupp.h"
#include "deskfun.h"
#include "deskmain.h"
#include "../aes/rectfunc.h"
#include "dos.h"
#include "desk1.h"
#include "deskins.h"
#include "biosdefs.h"
#include "kprint.h"


static void zoom_closed(WORD close, WORD w_id, WORD xicon, WORD yicon)
{
    GRECT rc;

    wind_get(w_id, WF_WXYWH, &rc.g_x, &rc.g_y, &rc.g_w, &rc.g_h);

    if (close)
        wind_close(w_id);

    graf_shrinkbox(xicon, yicon, G.g_wicon, G.g_hicon,
                    rc.g_x, rc.g_y, rc.g_w, rc.g_h);
}


static void w_setpath(WNODE *pw, BYTE *pathname)
{
    WORD icx, icy;
    GRECT rc;

    wind_get(pw->w_id,WF_WXYWH, &rc.g_x, &rc.g_y, &rc.g_w, &rc.g_h);
    icx = rc.g_x + (rc.g_w / 2) - (G.g_wicon / 2);
    icy = rc.g_y + (rc.g_h / 2) - (G.g_hicon / 2);
    zoom_closed(0, pw->w_id, icx, icy);
    do_fopen(pw, 0, pathname, TRUE);
}


void true_closewnd(WNODE *pw)
{
    GRECT rc;

    wind_get(pw->w_id,WF_WXYWH, &rc.g_x, &rc.g_y, &rc.g_w, &rc.g_h);
    zoom_closed(1, pw->w_id, G.g_screen[pw->w_obid].ob_x,
                    G.g_screen[pw->w_obid].ob_y);
    pn_close(pw->w_path);
    win_free(pw);
}


/*
 * full or partial close of desktop window
 */
void fun_close(WNODE *pw, WORD closetype)
{
    BYTE pathname[MAXPATHLEN];
    BYTE *fname;

    if (!pw->w_path)
    {
        KDEBUG(("Invalid WNODE passed to fun_close()\n"));
        return;
    }

    graf_mouse(HGLASS, NULL);

    /*
     * handle CLOSE_FOLDER and CLOSE_TO_ROOT
     *
     * if already in the root, change CLOSE_FOLDER to CLOSE_WINDOW
     * (but don't change CLOSE_TO_ROOT!)
     */
    if (closetype != CLOSE_WINDOW)
    {
        strcpy(pathname,pw->w_path->p_spec);
        fname = filename_start(pathname);
        if (closetype == CLOSE_TO_ROOT)
            strcpy(pathname+3,fname);
        else if (pathname+3 == fname)
            closetype = CLOSE_WINDOW;
        else    /* we need to go up one level */
            remove_one_level(pathname);
    }

    if (closetype == CLOSE_WINDOW)
        true_closewnd(pw);
    else
        w_setpath(pw,pathname);

    graf_mouse(ARROW, NULL);
}


/* Align drive icon on a grid */
void snap_disk(WORD x, WORD y, WORD *px, WORD *py)
{
    WORD xgrid, ygrid, icw, ich, xoff, yoff;

    icw  = G.g_icw;
    xgrid  = x / icw;
    xoff = x % icw;

    if (xoff <= (icw / 2))
        *px = xgrid * icw;
    else
        *px = (xgrid+1)*icw;

    *px = min(gl_width - icw, *px);
    if (*px < (gl_width / 2))
        *px += (gl_width % icw);

    y -= G.g_ydesk;
    ich = G.g_ich;
    ygrid  = y / G.g_ich;
    yoff = y % G.g_ich;

    if (yoff <= (ich / 2))
        *py = ygrid * ich;
    else
        *py = (ygrid+1) * ich;

    *py = min(G.g_hdesk - ich, *py);
    if (*py < (G.g_hdesk / 2))
        *py += (G.g_hdesk % ich);
    *py += G.g_ydesk;
}


static WORD fun_file2desk(PNODE *pn_src, WORD icontype_src, ANODE *an_dest, WORD dobj, WORD keystate)
{
    ICONBLK *dicon;
    WNODE *wn;
    BYTE pathname[10];      /* must be long enough for X:\\*.* */
    WORD operation, ret;

    pathname[1] = ':';      /* set up everything except drive letter */
    strcpy(pathname+2, "\\*.*");

    operation = -1;
    if (an_dest)
    {
        switch(an_dest->a_type)
        {
        case AT_ISDISK:
            dicon = (ICONBLK *)G.g_screen[dobj].ob_spec;
            pathname[0] = dicon->ib_char & 0xFF;
            operation = (keystate&MODE_CTRL) ? OP_MOVE : OP_COPY;
            break;
        case AT_ISTRSH:
            if (icontype_src >= 0)      /* source is desktop icon */
                if (wants_to_delete_files() == FALSE)
                    return FALSE;       /* i.e. remove icons or cancel */
            pathname[0] = pn_src->p_spec[0];
            operation = OP_DELETE;
            break;
        }
    }

    if (operation >= 0)
        ret = fun_op(operation, icontype_src, pn_src, pathname);
    else ret = FALSE;

    if (ret)                /* operation succeeded */
    {
        /*
         * rebuild any corresponding open windows
         */
        wn = fold_wind(pathname);
        if (wn)
            fun_rebld(wn);
    }

    return ret;
}


static WORD fun_file2win(PNODE *pn_src, BYTE  *spec, ANODE *an_dest, FNODE *fn_dest)
{
    BYTE *p;
    BYTE pathname[MAXPATHLEN];

    strcpy(pathname, spec);

    p = strchr(pathname, '*');
    if (p)
        *p = '\0';

    if (an_dest && an_dest->a_type == AT_ISFOLD)
    {
        strcpy(p, fn_dest->f_name);
        strcat(p, "\\*.*");
    }
    else
    {
        strcat(p, "*.*");
    }

    return fun_op(OP_COPY, -1, pn_src, pathname);
}


static void fun_win2desk(WORD wh, WORD obj, WORD keystate)
{
    WNODE *wn_src;
    ANODE *an_dest;

    an_dest = app_afind(TRUE, -1, obj, NULL, NULL, NULL);
    wn_src = win_find(wh);
    if (!wn_src)
        return;

    if (fun_file2desk(wn_src->w_path, -1, an_dest, obj, keystate))
        fun_rebld(wn_src);
}


static WORD fun_file2any(WORD sobj, WNODE *wn_dest, ANODE *an_dest, FNODE *fn_dest,
                  WORD dobj, WORD keystate)
{
    WORD icontype, okay = 0;
    FNODE *bp8;
    ICONBLK * ib_src;
    PNODE *pn_src;
    ANODE *an_src;
    BYTE path[10];

    ib_src = (ICONBLK *)G.g_screen[sobj].ob_spec;
    build_root_path(path, ib_src->ib_char);
    strcat(path,"*.*");
    pn_src = pn_open(path, F_SUBDIR);

    if (pn_src)
    {
        okay = pn_active(pn_src);

        if (pn_src->p_flist)
        {
            for (bp8 = pn_src->p_flist; bp8; bp8 = bp8->f_next)
                bp8->f_obid = 0;
            /*
             * if we do not set the root's SELECTED attribute, dir_op()
             * (which is called by fun_file2win() & fun_file2desk())
             * will not process any of these files ...
             */
            G.g_screen->ob_state = SELECTED;
            if (wn_dest)    /* we are dragging a desktop icon to a window */
            {
                okay = fun_file2win(pn_src, wn_dest->w_path->p_spec, an_dest, fn_dest);
            }
            else    /* we are dragging a desktop item to another desktop item */
            {
                an_src = i_find(0, sobj, NULL, NULL);
                icontype = an_src ? an_src->a_type : -1;
                okay = fun_file2desk(pn_src, icontype, an_dest, dobj, keystate);
            }
            G.g_screen->ob_state = 0;
        }
        pn_close(pn_src);
        desk_clear(0);
    }

    return okay;
}


static void fun_desk2win(WORD wh, WORD dobj, WORD keystate)
{
    WNODE *wn_dest;
    FNODE *fn_dest;
    WORD sobj, copied;
    ANODE *an_src, *an_dest;

    wn_dest = win_find(wh);
    if (!wn_dest)
        return;

    an_dest = i_find(wh, dobj, &fn_dest, NULL);
    sobj = 0;
    while ((sobj = win_isel(G.g_screen, 1, sobj)))
    {
        an_src = i_find(0, sobj, NULL, NULL);
        if (an_src && (an_src->a_type == AT_ISTRSH))
        {
            fun_alert(1, STNODRA2);
            continue;
        }
        copied = fun_file2any(sobj, wn_dest, an_dest, fn_dest, dobj, keystate);
        if (copied)
            fun_rebld(wn_dest);
    }
}


static void fun_desk2desk(WORD dobj, WORD keystate)
{
    WORD sobj;
    ANODE *source;
    ANODE *target;

    target = app_afind(TRUE, -1, dobj, NULL, NULL, NULL);
    if (!target)    /* "can't happen" */
        return;

    sobj  = 0;
    while ((sobj = win_isel(G.g_screen, 1, sobj)))
    {
        source = i_find(0, sobj, NULL, NULL);
        if (!source || (source == target))
            continue;
        if (source->a_type == AT_ISTRSH)
        {
            fun_alert(1, STNOSTAK);
            continue;
        }
        fun_file2any(sobj, NULL, target, NULL, dobj, keystate);
    }
}


void desk1_drag(WORD wh, WORD dest_wh, WORD sobj, WORD dobj, WORD mx, WORD my, WORD keystate)
{
    if (wh)
    {
        if (dest_wh)    /* dragging from window to window, */
        {               /* e.g. copy/move files/folders    */
            fun_drag(wh, dest_wh, dobj, mx, my, keystate);
        }
        else            /* dragging from window to desktop */
        {
            if (sobj == dobj)   /* dropping onto desktop surface */
                fun_alert(1, STNODRA1);
            else                /* dropping onto desktop icon */
                fun_win2desk(wh, dobj, keystate);
        }
    }
    else    /* Dragging something from desk */
    {
        if (dest_wh)    /* dragging from desktop to window,  */
        {               /* e.g. copying a disk into a folder */
            fun_desk2win(dest_wh, dobj, keystate);
        }
        else            /* dragging from desktop to desktop,   */
        {               /* e.g. copying a disk to another disk */
            if (sobj != dobj)
                fun_desk2desk(dobj, keystate);
        }
    }
}
