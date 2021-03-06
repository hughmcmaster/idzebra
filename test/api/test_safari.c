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

/** \file
    \brief test safari filter
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif
#include "testlib.h"

const char *myrec[] =
{
    "1234\n"  /* ID first record */
    /* chunk owner seq idx term */
    "00024338 125060 0 1 any the\n"
    "00024338 125060 0 2 any art\n"
    "00024338 125060 0 3 any mand\n"
    "s 00024338 125060 0 3 ti a b c\n"
    ,
    "5678\n"  /* other record - same owner id */
    "00024339 125060 0 1 any den\n"
    "00024339 125060 0 2 any gamle\n"
    "00024339 125060 0 3 any mand\n"
    "s 00024339 125060 0 3 ti d e f\n"
    ,
    "5678\n"  /* same record chunk id as before .. */
    "00024339 125060 0 1 any the\n"
    "00024339 125060 0 2 any gamle\n"
    "00024339 125060 0 3 any mand\n"
    "s 00024339 125060 0 3 ti g h i\n"
    ,
    "1000\n"  /* separate record */
    "00024339 125061 0 1 any the\n"
    "00024339 125061 0 2 any gamle\n"
    "00024339 125061 0 3 any mand\n"
    "w 00024339 125661 0 4 any Hello\n" /* index type given */
    "s 00024339 125061 0 3 ti j k l\n"
    ,
    "1001\n"  /* separate record */
    "00024340 125062 0 1 any the\n"
    "00024340 125062 0 1 any the\n" /* DUP KEY, bug #432 */
    "00024340 125062 0 2 any old\n"
    "00024340 125062 0 3 any mand\n"
    "s 00024340 125062 0 3 ti m n o\n"
    ,
    "1002\n"  /* segment testing record */
    "00024341 125062 0 1 title a\n"
    "00024341 125062 0 2 title b\n"

    "00024341 125062 1 1024 title b\n"
    "00024341 125062 1 1025 title c\n"
    "00024341 125062 1 1026 title d\n"
    "00024341 125062 1 1027 title e\n"
    "00024341 125062 1 1028 title f\n"

    "00024341 125062 2 2048 title g\n"
    "00024341 125062 2 2049 title c\n"
    ,

    0
};

static void tst(int argc, char **argv)
{
    zint ids[3];
    zint limits[3];
    ZebraService zs = tl_start_up("test_safari.cfg", argc, argv);

    ZebraHandle zh = zebra_open(zs, 0);

    YAZ_CHECK(tl_init_data(zh, myrec));

    YAZ_CHECK(tl_query(zh, "@attr 4=3 @attr 1=any the", 3));
    YAZ_CHECK(tl_query(zh, "@attr 4=3 @attr 1=any den", 0));
    YAZ_CHECK(tl_query(zh, "@attr 4=3 @attr 1=any @and the art", 1));
    YAZ_CHECK(tl_query(zh, "@attr 4=3 @attr 1=any @and den gamle", 0));
    YAZ_CHECK(tl_query(zh, "@attr 4=3 @attr 1=any @and the gamle", 1));
    YAZ_CHECK(tl_query(zh, "@attr 4=3 @attr 1=any @and the of", 0));

    YAZ_CHECK(tl_query(zh, "@attr 4=3 @attr 1=any @prox 0 3 1 2 k 2 the gamle", 1));
    YAZ_CHECK(tl_query(zh, "@attr 4=3 @attr 1=any @prox 0 3 1 2 k 2 den gamle", 0));
    YAZ_CHECK(tl_query(zh, "@attr 4=3 @attr 1=any @prox 0 3 1 2 k 2 the art", 1));
    YAZ_CHECK(tl_query(zh, "@attr 4=3 @attr 1=any @prox 0 3 1 2 k 2 den gamle", 0));
    YAZ_CHECK(tl_query(zh, "@attr 4=3 @attr 1=any @prox 0 3 1 2 k 2 old mand", 1));

    YAZ_CHECK(tl_query(zh, "@attr 1=any hello", 1));

    /* verify that we get these records exactly */
    ids[0] = 24338;
    ids[1] = 24339;
    ids[2] = 24340;
    YAZ_CHECK(tl_meta_query(zh, "@attr 4=3 @attr 1=any mand", 3, ids));

    YAZ_CHECK_EQ(tl_fetch_compare(zh, 1, "R", yaz_oid_recsyn_sutrs,
                                  myrec[0]), ZEBRA_OK);
    YAZ_CHECK_EQ(tl_fetch_compare(zh, 2, "R", yaz_oid_recsyn_sutrs,
                                  myrec[2]), ZEBRA_OK);
    YAZ_CHECK_EQ(tl_fetch_compare(zh, 3, "R", yaz_oid_recsyn_sutrs,
                                  myrec[4]), ZEBRA_OK);
    YAZ_CHECK_EQ(tl_fetch_compare(zh, 1, "zebra::facet::any:0",
                                  yaz_oid_recsyn_xml,
                                  "<record xmlns=\"http://www.indexdata.com/zebra/\">\n"
                                  "  <facet type=\"0\" index=\"any\">\n"
                                  "    <term coccur=\"4\" occur=\"3\">mand</term>\n"
                                  "    <term coccur=\"4\" occur=\"3\">the</term>\n"
                                  "    <term coccur=\"1\" occur=\"1\">art</term>\n"
                                  "    <term coccur=\"2\" occur=\"1\">gamle</term>\n"
                                  "    <term coccur=\"1\" occur=\"1\">old</term>\n"
                                  "  </facet>\n"
                                  "</record>\n"), ZEBRA_OK);
    YAZ_CHECK_EQ(tl_fetch_compare(zh, 1, "zebra::facet::any:0:2",
                                  yaz_oid_recsyn_xml,
                                  "<record xmlns=\"http://www.indexdata.com/zebra/\">\n"
                               "  <facet type=\"0\" index=\"any\">\n"
                                  "    <term coccur=\"4\" occur=\"3\">mand</term>\n"
                                  "    <term coccur=\"4\" occur=\"3\">the</term>\n"
                                  "  </facet>\n"
                                  "</record>\n"), ZEBRA_OK);


    YAZ_CHECK_EQ(tl_fetch_compare(zh, 1, "zebra::facet::ti:s",
                                  yaz_oid_recsyn_xml,
                                  "<record xmlns=\"http://www.indexdata.com/zebra/\">\n"
                                  "  <facet type=\"s\" index=\"ti\">\n"
                                  "    <term coccur=\"1\" occur=\"1\">a b c</term>\n"
                                  "    <term coccur=\"1\" occur=\"1\">j k l</term>\n"
                                  "    <term coccur=\"1\" occur=\"1\">m n o</term>\n"
                                  "  </facet>\n"
                                  "</record>\n"), ZEBRA_OK);

    /* limit to 125061 */
    limits[0] = 125061;
    limits[1] = 0;
    zebra_set_limit(zh, 0, limits);
    ids[0] = 24339;
    YAZ_CHECK(tl_meta_query(zh, "@attr 4=3 @attr 1=any mand", 1, ids));

    /* limit to 125060, 125061 */
    limits[0] = 125061;
    limits[1] = 125060;
    limits[2] = 0;
    zebra_set_limit(zh, 0, limits);
    ids[0] = 24338;
    ids[1] = 24339;
    YAZ_CHECK(tl_meta_query(zh, "@attr 4=3 @attr 1=any mand", 2, ids));

    /* all except 125062 */
    limits[0] = 125062;
    limits[1] = 0;
    zebra_set_limit(zh, 1, limits);

    ids[0] = 24338;
    ids[1] = 24339;
    YAZ_CHECK(tl_meta_query(zh, "@attr 4=3 @attr 1=any mand", 2, ids));

    /* no limit */
    zebra_set_limit(zh, 1, 0);
    YAZ_CHECK(tl_query(zh, "@attr 4=3 @attr 1=any mand", 3));

    /* test segments */
    YAZ_CHECK(tl_query(zh, "@attr 4=3 @attr 1=title a", 1));
    YAZ_CHECK(tl_query(zh, "@attr 4=3 @attr 1=title b", 1));
    YAZ_CHECK(tl_query(zh, "@attr 4=3 @attr 1=title c", 1));

    YAZ_CHECK(tl_query(zh, "@attr 4=3 @attr 1=title @and a b", 1));
    YAZ_CHECK(tl_query(zh, "@attr 4=3 @attr 1=title @and a c", 1));

    YAZ_CHECK(tl_query(zh, "@attr 4=3 @attr 1=title @and c d", 1));
    YAZ_CHECK(tl_query(zh, "@attr 4=3 @attr 1=title @and b f", 1));
    YAZ_CHECK(tl_query(zh, "@attr 4=3 @attr 1=title @and f g", 0));
    YAZ_CHECK(tl_query(zh, "@attr 4=3 @attr 1=title @and g f", 0));
    YAZ_CHECK(tl_query(zh, "@attr 4=3 @attr 1=title @and d g", 0));
    YAZ_CHECK(tl_query(zh, "@attr 4=3 @attr 1=title @and g c", 0));
    YAZ_CHECK(tl_query(zh, "@attr 4=3 @attr 1=title @and c g", 0));
    YAZ_CHECK(tl_query(zh, "@attr 4=3 @attr 1=title @and c c", 1));

    YAZ_CHECK(tl_close_down(zh, zs));
}

TL_MAIN
/*
 * Local variables:
 * c-basic-offset: 4
 * c-file-style: "Stroustrup"
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */

