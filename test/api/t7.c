/* $Id: t7.c,v 1.1 2004-09-22 11:59:25 heikki Exp $
   Copyright (C) 2004
   Index Data Aps

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

#include <yaz/log.h>
#include <yaz/pquery.h>
#include <yaz/sortspec.h>
#include <idzebra/api.h>

/* read zebra.cfg from env var srcdir if it exists; otherwise current dir */
static ZebraService start_service()
{
    char cfg[256];
    char *srcdir = getenv("srcdir");
    sprintf(cfg, "%.200s%szebra.cfg", srcdir ? srcdir : "", srcdir ? "/" : "");
    return zebra_start(cfg);
}
	
int main(int argc, char **argv)
{
    int i;
    ZebraService zs;
    ZebraHandle zh;
    const char *myrec =
        "<gils>\n"
        "  <title>My title</title>\n"
        "</gils>\n";
    const char *setname1="set1";
    const char *setname2="set2";
    const char *setname3="set3";
    int status;
    int rc;
    ODR odr_input = odr_createmem (ODR_DECODE);    
    ODR odr_output = odr_createmem (ODR_ENCODE);    
    YAZ_PQF_Parser parser = yaz_pqf_create();
    Z_RPNQuery *query = yaz_pqf_parse(parser, odr_input, 
                                      "@attr 1=4 my");
    Z_SortKeySpecList *spec = 
          yaz_sort_spec (odr_output, "@attr 1=4 id");
    int hits;

    yaz_log_init_file("t7.log");

    nmem_init ();
    
    zs = start_service();
    zh = zebra_open (zs);
    zebra_select_database(zh, "Default");
    zebra_init(zh);

    zebra_begin_trans (zh, 1);
    zebra_add_record (zh, myrec, strlen(myrec));
    zebra_end_trans (zh);

    zebra_begin_trans (zh, 0);
        
    zebra_search_RPN (zh, odr_input, query, setname1, &hits);

    rc=zebra_sort(zh, odr_output, 1, &setname1, setname2, spec, &status);
    if (rc) { printf("sort A returned %d %d \n",rc,status); exit(1);}
    rc=zebra_sort(zh, odr_output, 1, &setname2, setname3, spec, &status);
    if (rc) { printf("sort B returned %d %d \n",rc,status); exit(1);}

    zebra_end_trans (zh);
    yaz_pqf_destroy(parser);

    /*
     zebra_deleleResultSet(zh, Z_DeleteRequest_list,
                          1, &setnamep, &status);
    */
    odr_destroy (odr_input);
    odr_destroy (odr_output);

    zebra_commit (zh);
    zebra_close (zh);
    zebra_stop (zs);

    nmem_exit ();
    xmalloc_trav ("x");
    exit (0);
}