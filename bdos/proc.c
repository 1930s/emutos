/*
 * proc.c - process management routines
 *
 * Copyright (c) 2001 Lineo, Inc. and Authors:
 *
 *  KTB     Karl T. Braun (kral)
 *  MAD     Martin Doering
 *  ACH     ???
 *
 * This file is distributed under the GPL, version 2 or at your
 * option any later version.  See doc/license.txt for details.
 */



#include "portab.h"
#include "fs.h"
#include "bios.h"
#include "mem.h"
#include "proc.h"
#include "gemerror.h"
#include "string.h"
#include "../bios/kprint.h"

#define DBGPROC 0

/*
 * forward prototypes
 */

static void ixterm( PD *r );
static WORD envsize( char *env );
static void init_pd(PD *p, char *t, long max, MD *env);
static MD *alloc_env(char *v);
static void proc_go(PD *p);

/*
 * global variables
 */

PD      *run;           /* ptr to PD for current process */
WORD    supstk[SUPSIZ]; /* common sup stack for all processes*/

/*
 * internal variables
 */

static jmp_buf bakbuf;         /* longjmp buffer */


/**
 * ixterm - terminate a process
 *
 * terminate process with PD 'r'.
 *
 * @r: PD of process to terminate
 */

static void     ixterm( PD *r )
{
    register MD *m, **q;
    register WORD h;
    register WORD i;

    /* check the standard devices in both file tables  */

    for( i = 0 ; i < NUMSTD ; i++ )
        if( (h = r->p_uft[i]) > 0 )
            xclose(h);

    for (i = 0; i < OPNFILES; i++)
        if (r == sft[i].f_own)
            xclose(i+NUMSTD);


    /* check directory usage  M01.01.07 */

    for( i = 0 ; i < NUMCURDIR ; i++ )
    {
        if( (h = r->p_curdir[i]) != 0 )
            diruse[h]-- ;
    }

    /* free each item in the allocated list, that is owned by 'r' */

    for( m = *( q = &pmd.mp_mal ) ; m ; m = *q )
        {
            if (m->m_own == r)
            {
                *q = m->m_link;
                freeit(m,&pmd);
            }
            else
                q = &m->m_link;
        }
}

/* change memory owner based on the block address
 * this should really go in the memory sub-part.
 */
static void set_owner(void *addr, PD *p)
{
    MD *m;
    for( m = pmd.mp_mal ; m ; m = m->m_link ) {
        if(m->m_start == (long)addr) {
            m->m_own = p;
            return;
        }  
    }
}

/*
 * envsize - determine size of env area
 *
 * counts bytes starting at 'env' upto and including the terminating
 * double null.
 */

static  WORD envsize( char *env )
{
    register char       *e ;
    register WORD       cnt ;

    for( e = env, cnt = 0 ; !(*e == NULL && *(e+1) == NULL) ; ++e, ++cnt )
        ;

    return( cnt + 2 ) ;         /*  count terminating double null  */
}



/** xexec - (p_exec - 0x4b) execute a new process
 *
 *      
 * load&go(cmdlin,cmdtail), load/nogo(cmdlin,cmdtail), justgo(psp)
 * create psp - user receives a memory partition
 *
 * @flg: 0: load&go, 3: load/nogo, 4: justgo, 5: create psp, 6: ???
 * @s:   command
 * @t:   tail
 * @v:   environment
 */

/* LVL - this avoids warnings using gcc. The offending function parameters 
 * are transformed into static variables, ensuring that they do not have
 * 'undetermined' value when coming back from longjmp.
 * This will fail only if two processes call xexec at the same time, which
 * is not allowed in bdos anyway.
 */

static WORD flg;
static char *t, *v;
static long do_xexec(char *);
 
long    xexec(WORD fflg, char *s, char *tt, char *vv)
{
    flg = fflg;
    t = tt;
    v = vv;
    return do_xexec(s);
}

static long do_xexec(char *s)
{
    PD *p;
    PGMHDR01 hdr;
    MD *m, *env;
    ERROR rc;
    FH fh;
    long max, needed;

#if     DBGPROC
    kprintf("BDOS: xexec - flag or mode = %d\n", flg);
#endif

    /* first branch - actions that do not require loading files */
    switch(flg) {
    case PE_BASEPAGE:
    {
        MD *mm;
        long mmax;

        /* just create a basepage */
        env = alloc_env(v);
        if(env == NULL) {
#if DBGPROC
            kprintf("xexec: Not Enough Memory!\n") ;
#endif
            return(ENSMEM) ;
        }
        mmax = (long) ffit(-1L, &pmd); 
        if(mmax >= sizeof(PD)) {
            /* not even enough memory for basepage */
            mm = ffit(mmax, &pmd);
            p = (PD *) mm->m_start;
        } else {
            freeit(env, &pmd);
#if DBGPROC
            kprintf("xexec: No memory for TPA\n") ;
#endif
            return(ENSMEM);
        }
        /* memory ownership */
        mm->m_own = env->m_own = run;

        /* initialize the PD */
        init_pd(p, t, mmax, env);

        return (long) p;
    }
    case PE_GOTHENFREE:
        /* set the owner of the memory to be this process */
        p = (PD *)t;
        set_owner(p, p);
        set_owner(p->p_env, p);
        /* fall through */
    case PE_GO:
        proc_go((PD *)t);
        /* should not return ? */
        return (long)t;
    case PE_LOADGO:
    case PE_LOAD:
        break;
    default:
        return EINVFN;
    }
    
    /* we now need to load a file */
#if DBGPROC
    kprintf("BDOS: xexec - trying to find the command ...\n");
#endif
    if (ixsfirst(s,0,0L)) {
#if DBGPROC
        kprintf("BDOS: Command %s not found!!!\n", s);
#endif
        return(EFILNF);     /*  file not found      */
    }

    /* error handling */
    memcpy(bakbuf, errbuf, sizeof(errbuf));
    if ( setjmp(errbuf) )
    {
        /* Free any memory allocated to this program. */
        if (flg != 4)           /* did we allocate any memory? */
            ixterm((PD*)t);             /*  yes - free it */

        longjmp(bakbuf,1);
    }
    
    /* avoid warnings */    
    max = 0;
    m = NULL;

    /* load the header */
    rc = xpgmhdrld(s, &hdr, &fh);
    if(rc) {
#if DBGPROC
        kprintf("BDOS: xexec - error returned from xpgmld = %ld (0x%lx)\n",rc , rc);
#endif
        return(rc);
    }

    /* allocate the environment first, always in ST RAM */
    env = alloc_env(v);
    if ( !env ) {
#if DBGPROC
        kprintf("xexec: Not Enough Memory!\n") ;
#endif
        return(ENSMEM) ;
    }
    
    /* allocate the basepage depending on memory policy */
    needed = hdr.h01_tlen + hdr.h01_dlen + hdr.h01_blen + sizeof(PD);
        
    /* first try */
    p = NULL;
    if(has_ttram && (hdr.h01_flags & PF_TTRAMLOAD)) {
        /* use ttram preferably */
        max = (long) ffit(-1L, &pmdtt); 
        if(max >= needed) {
            m = ffit(max, &pmdtt);
            p = (PD *) m->m_start;
        } 
    }
    /* second try */
    if(p == NULL) {
        max = (long) ffit(-1L, &pmd); 
        if(max >= needed) {
            m = ffit(max, &pmd);
            p = (PD *) m->m_start;
        } 
    }
    /* still failed? free env and return */
    if(p == NULL) {
        freeit(env,&pmd);
#if DBGPROC
        kprintf("xexec: No memory for TPA\n") ;
#endif
        return(ENSMEM);
    }
    assert(m != NULL);

    /* memory ownership - the owner is either the new process being created,
     * or the parent 
     */
    if(flg == PE_LOADGO) {
        m->m_own = env->m_own = p;
    } else {
        m->m_own = env->m_own = run;
    }   

    /* initialize the PD */
    init_pd(p, t, max, env);

    /* set the flags (must be done after init_pd) */
    p->p_flags = hdr.h01_flags;

    /* now, read in the rest of the program and perform relocation */
    rc = xpgmld(s, p, fh, &hdr);
    if ( rc ) {
#if DBGPROC
        kprintf("BDOS: xexec - error returned from xpgmld = %ld (0x%lx)\n",rc , rc);
#endif
        freeit(env, &pmd);
        
        xmfree((long)p);
        return(rc);
    }

    if(flg != PE_LOAD) 
        proc_go(p);
    return (long) p;
}

static void init_pd(PD *p, char *t, long max, MD *env)
{
    int i;
    WORD h;
    char *b;
    
    /* first, zero it out */
    bzero(p, sizeof(PD)) ;

    /* memory values */
    p->p_lowtpa = (long) p ;                /*  M01.01.06   */
    p->p_hitpa  = (long) p  +  max ;        /*  M01.01.06   */
    p->p_xdta = &p->p_cmdlin[0] ;   /* default p_xdta is p_cmdlin */
    p->p_env = (char *) env->m_start ;

    /* now inherit standard files from me */
    for (i = 0; i < NUMSTD; i++) {
        h = run->p_uft[i];
        if ( h > 0 )
            ixforce(i,run->p_uft[i],p);
        else
            p->p_uft[i] = h;
    }

    /* and current directory set */
    for (i = 0; i < 16; i++)
        ixdirdup(i,run->p_curdir[i],p);

    /* and current drive */
    p->p_curdrv = run->p_curdrv;

    /* copy tail */
    b = &p->p_cmdlin[0] ;
    for( i = 0 ; (i < PDCLSIZE)  && (*t) ; i++ )
        *b++ = *t++;

    *b++ = 0;
    t = (char *) p;
}

/* allocate the environment, always in ST RAM */
static MD *alloc_env(char *v)
{
    MD *env;
    int i;

    /* determine the env size */
    if (!v)
        v = run->p_env;
    i = envsize( v ) ;
    if ( i & 1 )                     /*  must be even        */
        ++i ;

    /* allocate it */
    env = ffit((long) i,&pmd);
    if ( !env ) {
        return NULL;
    }

    /* copy it */
    memcpy((void *)(env->m_start), v, i);
    
    return env;
}

static void proc_go(PD *p)
{
    int i;
    long *spl;

#if DBGPROC
    kprintf("BDOS: xexec - trying to load (and execute) a command ...\n");
#endif
    p->p_parent = run;
    spl = (long *) p->p_hitpa;

    /* TODO, fill the values using the field names !!! */
    *--spl = (long) p;
    *--spl = 0L;            /* bogus retadd */

    /* 10 regs (40 bytes) of zeroes  */

    for (i = 0; i < 10; i++)
        *--spl = 0L;

    *--spl = p->p_tbase;    /* text start */

    {
        WORD *spw = (WORD *) spl;
        *--spw = 0;             /* startup status reg */
        spl = (long *) spw;
    }

    *--spl = (long) &supstk[SUPSIZ];
    p->p_areg[6-3] = p->p_areg[7-3] = (long) spl;
    p->p_areg[5-3] = p->p_dbase;
    p->p_areg[4-3] = p->p_bbase;
    run = (PD *) p;

    gouser() ;
}

/*
 * the old_ implementation is for reference only, I do not guarantee that it
 * works.
 */

extern ERROR old_xpgmld(char *s, PD *p);
long old_do_xexec(char *s)
{       
    PD  *p;
    char *b, *e;
    WORD i, h;                  /*  M01.01.04           */
    long rc, max;
    MD  *m, *env;
    long *spl;
#if     !M0101082703
    WORD        *spw;
#endif

    m = env = 0L ;

    /*
     * check validity of flg - 1,2 or >5 is not allowed
     */

#if     DBGPROC
    kprintf("BDOS: xexec - flag or mode = %d\n", flg);
#endif

    if (flg == 6)   /* (not really) implement newer mode 6 */
        flg = 4;
    
    if(   flg && (      flg < 3 || flg > 5  )    )
        return(EINVFN);

    if ((flg == 0) || (flg == 3))       /* load (execute) a file */
    {
#if     DBGPROC
        kprintf("BDOS: xexec - trying to find the command ...\n");
#endif
        if (ixsfirst(s,0,0L)) {
#if     DBGPROC
            kprintf("BDOS: Command %s not found!!!\n", s);
#endif
            return(EFILNF);     /*  file not found      */
        }
    }

    memcpy(bakbuf, errbuf, sizeof(errbuf));

    if ( setjmp(errbuf) )
    {
        /* Free any memory allocated to this program. */
        if (flg != 4)           /* did we allocate any memory? */
            ixterm((PD*)t);             /*  yes - free it */

        longjmp(bakbuf,1);
    }

    /* will we need memory and a psp ? */

    if (flg != 4)
    {
        /* get largest memory partition available */

        if (!v)
            v = run->p_env;

        /* determine minimum */

        i = envsize( v ) ;
        if( i & 1 )                     /*  must be even        */
            ++i ;
        /* allocate environment */

        env = ffit((long) i,&pmd);
        if ( !env )
        {
#if     DBGPROC
            kprintf("xexec: Not Enough Memory!\n") ;
#endif
            return(ENSMEM) ;
        }

        e = (char *) env->m_start;

        /* now copy it */

        /* LVL bmove( v , e , i ) ; */
        memcpy(e, v, i);

        /* allocate base page */

        max = (long) ffit( -1L , &pmd ) ;       /*  amount left */

        if( max < sizeof(PD) )
        {
            /* not enough even for PD  */
            freeit(env,&pmd);
#if     DBGPROC
            kprintf("xexec: No Room For Base Pg\n") ;
#endif
            return(ENSMEM);
        }

        /*
         * Allocate the base page.  The owner of it is either the
         * new process being created, or the parent
         */

        m = ffit(max,&pmd);

        p = (PD *) m->m_start;          /*  PD is first in bp   */

        env->m_own =  flg == 0 ? p : run ;
        m->m_own = env->m_own ;

        max = m->m_length;              /*  length of tpa       */

        /*
         * We know we have at least enough room for the PD (room
         * for the rest of the pgm checked for in pgmld)
         * initialize the PD (first, by zero'ing it out)
         */

        bzero( (char *) p , sizeof(PD)  ) ;

        p->p_lowtpa = (long) p ;                /*  M01.01.06   */
        p->p_hitpa  = (long) p  +  max ;        /*  M01.01.06   */
        p->p_xdta = &p->p_cmdlin[0] ;   /* default p_xdta is p_cmdlin */
        p->p_env = (char *) env->m_start ;


        /* now inherit standard files from me */

        for (i = 0; i < NUMSTD; i++)
        {
            h = run->p_uft[i];
            if ( h > 0 )
                ixforce(i,run->p_uft[i],p);
            else
                p->p_uft[i] = h;
        }

        /* and current directory set */

        for (i = 0; i < 16; i++)
            ixdirdup(i,run->p_curdir[i],p);

        /* and current drive */

        p->p_curdrv = run->p_curdrv;

        /* copy tail */

        b = &p->p_cmdlin[0] ;
        for( i = 0 ; (i < PDCLSIZE)  && (*t) ; i++ )
            *b++ = *t++;

        *b++ = 0;
        t = (char *) p;
    }

    /*
     * for 3 or 0, need to load, supply baspage containing:
     * tpa limits, filled in with start addrs,lens
     */

    if((flg == 0) || (flg == 3)) {
        rc = old_xpgmld(s, (PD *)t);
        if ( rc )
        {
#if     DBGPROC
            kprintf("BDOS: xexec - error returned from xpgmld = %ld (0x%lx)\n",rc , rc);
#endif
            ixterm((PD*)t);
            return(rc);
        }
    }

    if ((flg == 0) || (flg == 4))
    {
#if     DBGPROC
        kprintf("BDOS: xexec - trying to load (and execute) a command ...\n");
#endif
        p = (PD *) t;
        p->p_parent = run;
        p->p_flags = 0; /* TODO, handle PRG flags */
        spl = (long *) p->p_hitpa;
        *--spl = (long) p;
        *--spl = 0L;            /* bogus retadd */

        /* 10 regs (40 bytes) of zeroes  */

        for (i = 0; i < 10; i++)
            *--spl = 0L;

        *--spl = p->p_tbase;    /* text start */
#if     !M0101082703
        spw = (WORD *) spl;
        *--spw = 0;             /* startup status reg */
        spl = (long *) spw;
#else
        *--(WORD *)spl = 0 ;
#endif
        *--spl = (long) &supstk[SUPSIZ];
        p->p_areg[6-3] = p->p_areg[7-3] = (long) spl;
        p->p_areg[5-3] = p->p_dbase;
        p->p_areg[4-3] = p->p_bbase;
        run = (PD *) p;

        gouser() ;
    }

    /* sub-func 3 and 5 return here */

#if     DBGPROC
    kprintf("BDOS: xexec - return code = 0x%lx \n", (long) t);
#endif
    return( (long) t );
}



/*
 * [1]  The limit on this loop should probably be changed to use sizeof(PD)
 */



/*
 * x0term - (p_term0 - 0x00)Terminate Current Process
 *
 * terminates the calling process and returns to the parent process
 * without a return code
 */

void    x0term(void)
{
    xterm(0);
}

/*
 * xterm - terminate a process
 *
 * terminate the current process and transfer control to the colling
 * process.  All files opened by the terminating process are closed.
 *
 * Function 0x4C        p_term
 */

void    xterm(UWORD rc)
{
    PD *r;

    (* (WORD(*)()) trap13(5,0x102,-1L))() ;     /*  call user term handler */

    run = (r = run)->p_parent;
    ixterm( r );
    run->p_dreg[0] = rc;
    gouser();
}


/*      
 * xtermres - Function 0x31   p_termres
 */

WORD    xtermres(long blkln, WORD rc)
{
    MD *m,**q;

    xsetblk(0,run,blkln);

    for (m = *(q = &pmd.mp_mal); m ; m = *q)
        if (m->m_own == run)
        {
            *q = m->m_link; /* pouf ! like magic */
            xmfreblk(m);
        }
        else
            q = &m->m_link;

    xterm(rc);
}

