/* $Id: testlib.h,v 1.18 2006-03-31 15:58:05 adam Exp $
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

/** testlib - utilities for the api tests */

#include <stdlib.h>
#include <yaz/yconfig.h>
#include <yaz/pquery.h>
#include <yaz/log.h>
#include <idzebra/api.h>
#include <yaz/test.h>

/** 
 * tl_start_up : Does all the usual start functions
 *    - nmem_init
 *    - build the name of logfile from argv[0], and open it
 *      if no argv passed, do not open a log
 *    - read zebra.cfg from env var srcdir if it exists; otherwise current dir 
 *      default to zebra.cfg, if no name is given
 */
ZebraService tl_start_up(char *cfgname, int argc, char **argv);

/**
 * get_srcdir : returns the source dir. Most often ".", but when
 * making distcheck, some other dir 
 */
const char *tl_get_srcdir();

/** 
 * start_log: open a log file 
 */
/*    FIXME - parse command line arguments to set log levels etc */
void tl_start_log(int argc, char **argv);

/** 
 * tl_zebra_start - do a zebra_start with a decent config name 
 * Takes care of checking the environment for srcdir (as needed by distcheck)
 * and uses that if need be. 
 * The name defaults to zebra.cfg, if null or emtpy
 */
ZebraService tl_zebra_start(const char *cfgname);

/** 
 * close_down closes it all down
 * Does a zebra_close on zh, if not null.
 * Does a zebra_stop on zs, if not null 
 * Writes a log message, OK if retcode is zero, error if not
 * closes down nmem and xmalloc
 * returns the retcode, for use in return or exit in main()
 */
int tl_close_down(ZebraHandle zh, ZebraService zs
    ) GCC_ATTRIBUTE((warn_unused_result));    

/** inits the database and inserts test data */
int tl_init_data(ZebraHandle zh, const char **recs
    ) GCC_ATTRIBUTE((warn_unused_result));    

/**
 * tl_query does a simple query, and checks that the number of hits matches
 */
int tl_query(ZebraHandle zh, const char *query, zint exphits
    ) GCC_ATTRIBUTE((warn_unused_result));


/**
 * tl_query does a simple query, and checks that error is what is expected
 */
int tl_query_x(ZebraHandle zh, const char *query, zint exphits,
	       int experror
    ) GCC_ATTRIBUTE((warn_unused_result));
    
/**
 * tl_scan is a utility for scan testing 
 */
int tl_scan(ZebraHandle zh, const char *query,
	    int pos, int num,  /* input params */
	    int exp_pos, int exp_num,  int exp_partial, /* expected result */
	    const char **exp_entries  /* expected entries (or NULL) */
    ) GCC_ATTRIBUTE((warn_unused_result));

int tl_sort(ZebraHandle zh, const char *query, zint hits, zint *exp
    ) GCC_ATTRIBUTE((warn_unused_result));

/** 
 * ranking_query makes a query, checks number of hits, and for 
 * the first hit, that it contains the given string, and that it 
 * gets the right score
 */
int tl_ranking_query(ZebraHandle zh, char *query, 
		     int exphits, char *firstrec, int firstscore
    ) GCC_ATTRIBUTE((warn_unused_result));

/** 
 * meta_query makes a query, checks number of hits, and for 
 * checks that the all records in result set has the proper identifiers (ids)
 */
int tl_meta_query(ZebraHandle zh, char *query, int exphits,
		  zint *ids
    ) GCC_ATTRIBUTE((warn_unused_result));

/**
 * if filter given by name does not exist, exit nicely but warn in log 
 */
void tl_check_filter(ZebraService zs, const char *name);

#define TL_MAIN int main(int argc, char **argv) { \
 YAZ_CHECK_INIT(argc, argv); tst(argc, argv); YAZ_CHECK_TERM; }
