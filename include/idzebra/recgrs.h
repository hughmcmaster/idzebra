/* $Id: recgrs.h,v 1.3 2005-01-15 19:38:24 adam Exp $
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

#ifndef RECGRS_H
#define RECGRS_H

#include <idzebra/recctrl.h>

YAZ_BEGIN_CDECL

struct grs_read_info {
    void *clientData;
    int (*readf)(void *, char *, size_t);
    off_t (*seekf)(void *, off_t);
    off_t (*tellf)(void *);
    void (*endf)(void *, off_t);
    void *fh;
    off_t offset;
    NMEM mem;
    data1_handle dh;
};

YAZ_EXPORT
int zebra_grs_extract(void *clientData, struct recExtractCtrl *p,
		      data1_node *(*grs_read)(struct grs_read_info *));

YAZ_EXPORT
int zebra_grs_retrieve(void *clientData, struct recRetrieveCtrl *p,
		       data1_node *(*grs_read)(struct grs_read_info *));


YAZ_EXPORT
int grs_extract_tree(struct recExtractCtrl *p, data1_node *n);

YAZ_END_CDECL

#endif
