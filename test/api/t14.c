/* $Id: t14.c,v 1.2 2006-02-09 08:31:02 adam Exp $
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
    ZEBRA_RES res;

    res = zebra_create_database (zh, "Default");
    TL_ASSERT(res == ZEBRA_OK);

    /* bug #447 */
    res = zebra_admin_exchange_record (
	zh, rec, strlen(rec),
	opaque_id, strlen(opaque_id),
	1); /* insert */
    TL_ASSERT(res == ZEBRA_OK);

    res = zebra_admin_exchange_record (
	zh, rec, strlen(rec),
	opaque_id, strlen(opaque_id),
	4); /* update/insert */

    TL_ASSERT(res == ZEBRA_OK);
    do_query(__LINE__, zh, "@attr 1=4 some", 1);

    zebra_drop_database(zh, "Default");

    do_query_x(__LINE__, zh, "@attr 1=4 some", 0, 109);

}

int main(int argc, char **argv)
{
    ZebraService zs = start_up("zebra.cfg", argc, argv);
    ZebraHandle zh = zebra_open(zs, 0);
    ZEBRA_RES res;

    res = zebra_select_database(zh, "Default");
    TL_ASSERT(res == ZEBRA_OK);

    zebra_init(zh);

    create_search_drop(zh);
    /* bug #447 */
    create_search_drop(zh);

    return close_down(zh, zs, 0);
}

