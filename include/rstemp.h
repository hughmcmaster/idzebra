/*
 * Copyright (C) 1994-1997, Index Data I/S 
 * All rights reserved.
 * Sebastian Hammer, Adam Dickmeiss
 *
 * $Log: rstemp.h,v $
 * Revision 1.3  1997-09-05 15:30:05  adam
 * Changed prototype for chr_map_input - added const.
 * Added support for C++, headers uses extern "C" for public definitions.
 *
 * Revision 1.2  1995/09/04 15:20:13  adam
 * More work on temp sets. is_open member removed.
 *
 * Revision 1.1  1994/11/04  13:21:23  quinn
 * Working.
 *
 */

#ifndef RSET_TEMP_H
#define RSET_TEMP_H

#include <rset.h>

#ifdef __cplusplus
extern "C" {
#endif

extern const rset_control *rset_kind_temp;

typedef struct rset_temp_parms
{
    int     key_size;
} rset_temp_parms;

#ifdef __cplusplus
}
#endif

#endif
