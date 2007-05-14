/* $Id: t8.c,v 1.14 2007-05-14 12:33:33 adam Exp $
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

/** t8: test numeric attributes */

#include "testlib.h"

const char *recs[] = {
        "<gils>\n"
        "  <title>My title</title>\n"
        "  <abstract>test record with single coordset, negatives</abstract>\n"
        "  <Spatial-Domain><Bounding-Coordinates>\n"
        "    <West-Bounding-Coordinate> -120 </West-Bounding-Coordinate>\n"
        "    <East-Bounding-Coordinate> -102 </East-Bounding-Coordinate>\n"
        "    <North-Bounding-Coordinate>  49 </North-Bounding-Coordinate>\n"
        "    <South-Bounding-Coordinate>  31 </South-Bounding-Coordinate>\n"
        "  </Bounding-Coordinates></Spatial-Domain>"
        "</gils>\n",

        "<gils>\n"
        "  <title>Another title</title>\n"
        "  <abstract>second test with two coord sets</abstract>\n"
        "  <Spatial-Domain><Bounding-Coordinates>\n"
        "    <West-Bounding-Coordinate> -120 </West-Bounding-Coordinate>\n"
        "    <East-Bounding-Coordinate> -102 </East-Bounding-Coordinate>\n"
        "    <North-Bounding-Coordinate>  49 </North-Bounding-Coordinate>\n"
        "    <South-Bounding-Coordinate>  31 </South-Bounding-Coordinate>\n"
        "  </Bounding-Coordinates>"
        "  <Bounding-Coordinates>\n"
        "    <West-Bounding-Coordinate> -125 </West-Bounding-Coordinate>\n"
        "    <East-Bounding-Coordinate> -108 </East-Bounding-Coordinate>\n"
        "    <North-Bounding-Coordinate>  41 </North-Bounding-Coordinate>\n"
        "    <South-Bounding-Coordinate>  25 </South-Bounding-Coordinate>\n"
        "  </Bounding-Coordinates></Spatial-Domain>"
        "</gils>\n",
        0};
        

static void tst(int argc, char **argv)
{
    ZebraService zs = tl_start_up("zebra8.cfg", argc, argv);
    ZebraHandle zh = zebra_open (zs, 0);

    YAZ_CHECK(tl_init_data(zh, recs));

    /* couple of simple queries just to see that we have indexed the stuff */
    YAZ_CHECK(tl_query(zh,  "@attr 1=4 title", 2));
    
    /* 1=2038: West-Bounding-Coordinate 2039: East: 2040: North: 2041 South*/
    /* 4=109: numeric string */
    /* 2=3: equal  2=1: less, 2=4: greater or equal 2=5 greater */

    /* N>=25, search attributes work */
    YAZ_CHECK(tl_query(zh,  "@attr 2=4 @attr gils 1=2040 @attr 4=109 25", 2));

    /* N>49, search attributes work */
    YAZ_CHECK(tl_query(zh,  "@attr 2=5 @attr gils 1=2040 @attr 4=109 49", 0));

    /* N>=49, search attributes work */
    YAZ_CHECK(tl_query(zh,  "@attr 2=4 @attr gils 1=2040 @attr 4=109 49", 2));

    /* N>48, search attributes work */
    YAZ_CHECK(tl_query(zh,  "@attr 2=5 @attr gils 1=2040 @attr 4=109 48", 2));

    /* N<48, search attributes work */
    YAZ_CHECK(tl_query(zh,  "@attr 2=1 @attr gils 1=2040 @attr 4=109 48", 1));

    /* N<=48, search attributes work */
    YAZ_CHECK(tl_query(zh,  "@attr 2=2 @attr gils 1=2040 @attr 4=109 48", 1));

    /* N=41, get rec1 only */
    YAZ_CHECK(tl_query(zh,  "@attr 2=3 @attr gils 1=2040 @attr 4=109 41", 1));

    /* N=49, get both records */
    YAZ_CHECK(tl_query(zh,  "@attr 2=3 @attr gils 1=2040 @attr 4=109 49", 2));

    /* W=-120 get both records */
    YAZ_CHECK(tl_query(zh,  "@attr 2=3 @attr gils 1=2038 @attr 4=109 -120", 2));

    /* W<-122 get only rec1 */
    YAZ_CHECK(tl_query(zh,  "@attr 2=1 @attr gils 1=2038 @attr 4=109 '-120' ", 1));

    /* N=41 and N=49 get only rec2 */
    YAZ_CHECK(tl_query(zh, "@attr 2=3 @attr gils 1=2040 @attr 4=109 \"41 49\" ", 1));

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

