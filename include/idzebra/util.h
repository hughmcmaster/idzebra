/* $Id: util.h,v 1.5 2005-11-09 11:51:29 adam Exp $
   Copyright (C) 1995-2005
   Index Data ApS

This file is part of the Zebra server.

Zebra is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 2, or (at your option) any later
version.

Zebra is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License
along with Zebra; see the file LICENSE.zebra.  If not, write to the
Free Software Foundation, 59 Temple Place - Suite 330, Boston, MA
02111-1307, USA.
*/

#ifndef ZEBRA_UTIL_H
#define ZEBRA_UTIL_H

#include <yaz/yconfig.h>
#include <yaz/log.h>

#include <idzebra/version.h>

/* check that we don't have all too old yaz */
#ifndef YLOG_ERRNO
#error Need a modern yaz with YLOG_ defines
#endif

YAZ_BEGIN_CDECL

/** \var zint
 * \brief Zebra integer
 *
 * This integer is used in various Zebra APIs to specify record identifires,
 * number of occurrences etc. It is a "large" integer and is usually
 * 64-bit on newer architectures.
 */
#ifdef __GNUC__
typedef long long int zint;
#define ZINT_FORMAT "%lld"
#define ZINT_FORMAT0 "lld"
#else
#ifdef WIN32
typedef __int64 zint;
#define ZINT_FORMAT "%I64d"
#define ZINT_FORMAT0 "I64d"
#else
typedef long zint;
#define ZINT_FORMAT "%ld"
#define ZINT_FORMAT0 "ld"
#endif
#endif

/** \var typedef ZEBRA_RES
 * \brief Common return type for Zebra API
 *
 * This return code indicates success with code ZEBRA_OK and failure
 * with ZEBRA_FAIL
 */
typedef short ZEBRA_RES;
#define ZEBRA_FAIL -1
#define ZEBRA_OK   0

typedef zint SYSNO;

YAZ_EXPORT
zint atoi_zn (const char *buf, zint len);

YAZ_EXPORT
void zebra_zint_encode(char **dst, zint pos);

YAZ_EXPORT
void zebra_zint_decode(const char **src, zint *pos);

YAZ_END_CDECL

#define CAST_ZINT_TO_INT(x) (int)(x)
#define CAST_ZINT_TO_DOUBLE(x) (double)(x)

/* NATTR=1 for string-attribute architecture, =0 for set+use . */
#define NATTR 0

#endif