/* $Id: dom1.c,v 1.1 2007-02-07 12:08:54 adam Exp $
   Copyright (C) 1995-2007
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
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

*/

#include <yaz/test.h>
#include "testlib.h"

ZebraHandle index_some(ZebraService zs,
                       const char *filter, const char *file)
{
    char path[256];
    char profile_path[256];

    ZebraHandle zh = zebra_open(zs, 0);

    tl_check_filter(zs, "dom");

    YAZ_CHECK(zebra_select_database(zh, "Default") == ZEBRA_OK);

    zebra_init(zh);

    sprintf(profile_path, "%.80s:%.80s/../../tab", 
            tl_get_srcdir(), tl_get_srcdir());
    zebra_set_resource(zh, "profilePath", profile_path);

    zebra_set_resource(zh, "recordType", filter);

    YAZ_CHECK(zebra_begin_trans(zh, 1) == ZEBRA_OK);
    sprintf(path, "%.80s/%.80s", tl_get_srcdir(), file);

    YAZ_CHECK(zebra_repository_update(zh, path) == ZEBRA_OK);
    YAZ_CHECK(zebra_end_trans(zh) == ZEBRA_OK);
    zebra_commit(zh);
    return zh;
}

void tst(int argc, char **argv)
{
    ZebraHandle zh;
    
    ZebraService zs = tl_start_up(0, argc, argv);

    zh = index_some(zs, "dom.bad.xml", "marc-col.xml");
    zebra_close(zh);
    
    zh = index_some(zs, "dom.dom-config-col.xml", "marc-col.xml");
    YAZ_CHECK(tl_query(zh, "@attr 1=title computer", 3));
    YAZ_CHECK(tl_query(zh, "@attr 1=control 11224466", 1));
    YAZ_CHECK(tl_query_x(zh, "@attr 1=titl computer", 0, 114));
    YAZ_CHECK(tl_query_x(zh, "@attr 1=4 computer", 0, 121));
    zebra_close(zh);

    zh = index_some(zs, "dom.dom-config-one.xml", "marc-one.xml");
    YAZ_CHECK(tl_query(zh, "@attr 1=title computer", 1));
    YAZ_CHECK(tl_query(zh, "@attr 1=control 11224466", 1));
    YAZ_CHECK(tl_query_x(zh, "@attr 1=titl computer", 0, 114));
    YAZ_CHECK(tl_query_x(zh, "@attr 1=4 computer", 0, 121));
    zebra_close(zh);

    zh = index_some(zs, "dom.dom-config-marc.xml", "marc-col.mrc");
    YAZ_CHECK(tl_query(zh, "@attr 1=title computer", 3));
    YAZ_CHECK(tl_query(zh, "@attr 1=control 11224466", 1));
    YAZ_CHECK(tl_query_x(zh, "@attr 1=titl computer", 0, 114));
    YAZ_CHECK(tl_query_x(zh, "@attr 1=4 computer", 0, 121));
    zebra_close(zh);

    YAZ_CHECK(tl_close_down(0, zs));
}

TL_MAIN

/*
 * Local variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */
