/*
 * asm.h - Assembler help routines
 *
 * Copyright (c) 2001 EmuTOS development team
 *
 * Authors:
 *  LVL   Laurent Vogel
 *
 * This file is distributed under the GPL, version 2 or at your
 * option any later version.  See doc/license.txt for details.
 */

/*
 * This file contains utility routines (macros) to 
 * manipulate special registers from C language.
 * 
 * available macros:
 *
 * WORD set_sr(WORD new); 
 *   sets sr to the new value, and return the old sr value 
 * WORD get_sr(void);
 *   returns the current value of sr. the CCR bits are not meaningful.
 * void stop2300(void);
 * void stop2500(void);
 *   the STOP immediate instruction
 * void regsafe_call(void *addr);
 *   Do a subroutine call with saving/restoring the CPU registers
 *
 * For clarity, please add such two lines above when adding 
 * new macros below.
 */

#ifndef ASM_H
#define ASM_H


/* OS entry points implemented in util/miscasm.S */
extern long trap1(int, ...);
extern long trap1_pexec(short mode, const char * path, 
  const void * tail, const char * env);


/*
 * WORD swpw(WORD val); 
 *   swap endianess of val, 16 bits only.
 */

#ifdef __mcoldfire__
#define swpw(a)                           \
  __extension__                           \
  ({long _tmp;                            \
    __asm__ __volatile__                  \
    ("move.w  %0,%1\n\t"                  \
     "lsl.l   #8,%0\n\t"                  \
     "lsr.l   #8,%1\n\t"                  \
     "move.b  %1,%0"                      \
    : "=d"(a), "=d"(_tmp) /* outputs */   \
    : "0"(a)     /* inputs  */            \
    : "cc"       /* clobbered */          \
    );                                    \
  })
#else
#define swpw(a)                           \
  __asm__ __volatile__                    \
  ("ror   #8,%0"                          \
  : "=d"(a)          /* outputs */        \
  : "0"(a)           /* inputs  */        \
  : "cc"             /* clobbered */      \
  )
#endif


/*
 * WORD swpl(LONG val); 
 *   swap endianess of val, 32 bits only.
 */

#ifdef __mcoldfire__
#define swpl(a)                           \
  __extension__                           \
  ({long _tmp;                            \
    __asm__ __volatile__                  \
    ("move.b  (%1),%0\n\t"                \
     "move.b  3(%1),(%1)\n\t"             \
     "move.b  %0,3(%1)\n\t"               \
     "move.b  1(%1),%0\n\t"               \
     "move.b  2(%1),1(%1)\n\t"            \
     "move.b  %0,2(%1)"                   \
    : "=d"(_tmp)      /* outputs */       \
    : "a"(&a)        /* inputs  */        \
    : "cc", "memory" /* clobbered */      \
    );                                    \
  })
#else
#define swpl(a)                           \
  __asm__ __volatile__                    \
  ("ror   #8,%0\n\t"                      \
   "swap  %0\n\t"                         \
   "ror   #8,%0"                          \
  : "=d"(a)          /* outputs */        \
  : "0"(a)           /* inputs  */        \
  : "cc"             /* clobbered */      \
  )
#endif


/*
 * Warning: The following macros use "memory" in the clobber list,
 * even if the memory is not modified. On ColdFire, this is necessary
 * to avoid these instructions to be reordered by the compiler.
 * This may be a GCC bug.
 */


/*
 * WORD set_sr(WORD new); 
 *   sets sr to the new value, and return the old sr value 
 */

#define set_sr(a)                         \
__extension__                             \
({short _r, _a = (a);                     \
  __asm__ __volatile__                    \
  ("move.w sr,%0\n\t"                     \
   "move.w %1,sr"                         \
  : "=&dm"(_r)       /* outputs */        \
  : "ndm"(_a)        /* inputs  */        \
  : "cc", "memory"   /* clobbered */      \
  );                                      \
  _r;                                     \
})


/*
 * WORD get_sr(void); 
 *   returns the current value of sr. 
 */

#define get_sr()                          \
__extension__                             \
({short _r;                               \
  __asm__ volatile                        \
  ("move.w sr,%0"                         \
  : "=dm"(_r)        /* outputs */        \
  :                  /* inputs  */        \
  : "cc", "memory"   /* clobbered */      \
  );                                      \
  _r;                                     \
})



/*
 * void stop2x00(void)
 *   the m68k STOP immediate instruction
 */

#define stop2300()                        \
__extension__                             \
({__asm__ volatile                        \
  ("stop #0x2300"                         \
  :                  /* outputs */        \
  :                  /* inputs  */        \
  : "cc", "memory"   /* clobbered */      \
  );                                      \
})

#define stop2500()                        \
__extension__                             \
({__asm__ volatile                        \
  ("stop #0x2500"                         \
  :                  /* outputs */        \
  :                  /* inputs  */        \
  : "cc", "memory"   /* clobbered */      \
  );                                      \
})



/*
 * void regsafe_call(void *addr)
 *   Saves all registers to the stack, calls the function
 *   that addr points to, and restores the registers afterwards.
 */
#ifdef __mcoldfire__
#define regsafe_call(addr)                         \
__extension__                                      \
({__asm__ volatile ("lea     -60(sp),sp\n\t"       \
                    "movem.l d0-d7/a0-a6,(sp)");   \
  ((void (*)(void))addr)();                        \
  __asm__ volatile ("movem.l (sp),d0-d7/a0-a6\n\t" \
                    "lea     60(sp),sp");          \
})
#else
#define regsafe_call(addr)                         \
__extension__                                      \
({__asm__ volatile ("movem.l d0-d7/a0-a6,-(sp)");  \
  ((void (*)(void))addr)();                        \
  __asm__ volatile ("movem.l (sp)+,d0-d7/a0-a6");  \
})
#endif

/*
 * No operation opcode (just waits for some few cycles)
 */
#define nop()                             \
__extension__                             \
({__asm__ volatile                        \
  ("nop"                                  \
  :                  /* outputs */        \
  :                  /* inputs  */        \
  : "memory"         /* clobbered */      \
  );                                      \
})

#endif /* ASM_H */
