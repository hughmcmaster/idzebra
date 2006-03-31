/* $Id: t9.c,v 1.8 2006-03-31 15:58:05 adam Exp $
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

/** t9.c - test rank-1 */

#include "testlib.h"

#include "rankingrecords.h"

static void tst(int argc, char **argv)
{
    ZebraService zs = tl_start_up(0, argc, argv);
    ZebraHandle zh = zebra_open(zs, 0);

    YAZ_CHECK(tl_init_data(zh, recs));
    
    YAZ_CHECK(tl_ranking_query(zh, "@attr 1=1016 @attr 2=102 the",
			       3, "first title", 997 ));
    
    YAZ_CHECK(tl_ranking_query(zh, "@attr 1=1016 @attr 2=102 foo",
			       3, "second title", 850 ));
    
    /* get the record with the most significant hit, that is the 'bar' */
    /* as that is the rarest of my search words */
    YAZ_CHECK(tl_ranking_query(zh, "@attr 1=1016 @attr 2=102 @or @or the foo bar",
			       3, "third title", 940 ));
    
    YAZ_CHECK(tl_close_down(zh, zs));
}

TL_MAIN
