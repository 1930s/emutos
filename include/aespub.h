/*
 * aespub.h - Public AES functions
 *
 * Copyright (c) 2016 The EmuTOS development team
 *
 * Authors:
 *  VRI   Vincent Rivière
 *
 * This file is distributed under the GPL, version 2 or at your
 * option any later version.  See doc/license.txt for details.
 */

#ifndef AESPUB_H
#define AESPUB_H

#include "portab.h"

/* AES entry point */
void ui_start(void) NORETURN;   /* found in aes/gemstart.S */

#endif /* AESPUB_H */
