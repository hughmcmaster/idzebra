/* $Id: t14.c,v 1.4 2006-05-10 08:13:35 adam Exp $
   Copyright (C) 2004-2005
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

#include "testlib.h"

static void create_search_drop(ZebraHandle zh)
{
    const char *rec = "<gils><title>some</title></gils>";
    const char *opaque_id = "9";

    YAZ_CHECK(zebra_create_database (zh, "Default") == ZEBRA_OK);

    /* bug #447 */
    YAZ_CHECK(zebra_admin_exchange_record (
		  zh, rec, strlen(rec),
		  opaque_id, strlen(opaque_id),
		  1) == ZEBRA_OK); /* insert */

    YAZ_CHECK(zebra_admin_exchange_record (
	zh, rec, strlen(rec),
	opaque_id, strlen(opaque_id),
	4) == ZEBRA_OK); /* update/insert */

    YAZ_CHECK(tl_query(zh, "@attr 1=4 some", 1));

    zebra_drop_database(zh, "Default");

    YAZ_CHECK(tl_query_x(zh, "@attr 1=4 some", 0, 109));

}

static void tst(int argc, char **argv)
{
    ZebraService zs = tl_start_up("zebra.cfg", argc, argv);
    ZebraHandle zh = zebra_open(zs, 0);

    YAZ_CHECK(zebra_select_database(zh, "Default") == ZEBRA_OK);

    zebra_init(zh);

    create_search_drop(zh);
    /* bug #447 */
    create_search_drop(zh);

    YAZ_CHECK(tl_close_down(zh, zs));
}

TL_MAIN
/*
 * Local variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */
