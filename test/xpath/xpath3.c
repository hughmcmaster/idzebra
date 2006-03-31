
/* $Id: xpath3.c,v 1.6 2006-03-31 15:58:10 adam Exp $
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

#include "../api/testlib.h"

/** xpath3.c - attributes, with Danish characters */

const char *myrec[] = {
    "<root> \n"
    "  <!-- Space in attribute --> \n"
    "  <first attr=\"danish\">content</first> \n"
    "  <second attr=\"danish lake\">content</second> \n"
    "  <!-- Oslash in Latin-1 encoded below.. --> \n"
    "  <third attr=\"dansk s�\">content<third> \n"
    "</root> \n",
    0};


static void tst(int argc, char **argv)
{
    ZebraService zs = tl_start_up(0, argc, argv);
    ZebraHandle zh = zebra_open(zs, 0);
    YAZ_CHECK(tl_init_data(zh, myrec));

    YAZ_CHECK(tl_query(zh, "@attr 1=/root content",1));
    YAZ_CHECK(tl_query(zh, "@attr 1=/root/first content",1));
    YAZ_CHECK(tl_query(zh, "@attr {1=/root/first[@attr='danish']} content",1));
    YAZ_CHECK(tl_query(zh, "@attr {1=/root/second[@attr='danish lake']} content",1));
    YAZ_CHECK(tl_query(zh, "@attr {1=/root/third[@attr='dansk s\xc3\xb8']} content",1)); 
    /* FIXME - This triggers bug200 */

    YAZ_CHECK(tl_close_down(zh, zs));
}

TL_MAIN
