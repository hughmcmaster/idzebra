/* $Id: attrfind.h,v 1.1 2006-05-19 13:49:34 adam Exp $
   Copyright (C) 2005-2006
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

#ifndef ATTRFIND_H
#define ATTRFIND_H

#include <yaz/yconfig.h>
#include <yaz/z-core.h>
#include <yaz/oid.h>

YAZ_BEGIN_CDECL

typedef struct {
    int type;
    int major;
    int minor;
    Z_AttributeElement **attributeList;
    int num_attributes;
} AttrType;

void attr_init_APT(AttrType *src, Z_AttributesPlusTerm *zapt, int type);

void attr_init_AttrList(AttrType *src, Z_AttributeList *list, int type);

int attr_find_ex(AttrType *src, oid_value *attributeSetP,
                 const char **string_value);
int attr_find(AttrType *src, oid_value *attributeSetP);
    

YAZ_END_CDECL

#endif
/*
 * Local variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */
