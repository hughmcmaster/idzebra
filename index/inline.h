/* This file is part of the Zebra server.
   Copyright (C) Index Data

Zebra is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 2, or (at your option) any later
version.

Zebra is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

*/
#ifndef INLINE_H
#define INLINE_H

#include "marcomp.h"

#ifdef __cplusplus
extern "C" {
#endif


typedef struct inline_field
{
    char *name;
    char *ind1;
    char *ind2;
    struct inline_subfield *list;
} inline_field;
typedef struct inline_subfield
{
    char *name;
    char *data;
    struct inline_subfield *next;
    struct inline_subfield *parent;
} inline_subfield;

inline_field *inline_mk_field(void);
int inline_parse(inline_field *pf, const char *tag, const char *s);
void inline_destroy_field(inline_field *p);

#ifdef __cplusplus
}
#endif

#endif
/*
 * Local variables:
 * c-basic-offset: 4
 * c-file-style: "Stroustrup"
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */

