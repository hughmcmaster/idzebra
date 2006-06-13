/* $Id: zebraapi.c,v 1.222 2006-06-13 12:02:12 adam Exp $
   Copyright (C) 1995-2006
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

#include <assert.h>
#include <stdio.h>
#include <limits.h>
#ifdef WIN32
#include <io.h>
#include <process.h>
#include <direct.h>
#endif
#if HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <yaz/diagbib1.h>
#include <yaz/pquery.h>
#include <yaz/sortspec.h>
#include "index.h"
#include "rank.h"
#include "orddict.h"
#include <charmap.h>
#include <idzebra/api.h>

#define DEFAULT_APPROX_LIMIT 2000000000

/* simple asserts to validate the most essential input args */
#define ASSERTZH assert(zh && zh->service)
#define ASSERTZHRES assert(zh && zh->service && zh->res)
#define ASSERTZS assert(zs)

static int log_level = 0;
static int log_level_initialized = 0;

static void zebra_open_res(ZebraHandle zh);
static void zebra_close_res(ZebraHandle zh);

static ZEBRA_RES zebra_check_handle(ZebraHandle zh)
{
    if (zh)
        return ZEBRA_OK;
    return ZEBRA_FAIL;
}

#define ZEBRA_CHECK_HANDLE(zh) if (zebra_check_handle(zh) != ZEBRA_OK) return ZEBRA_FAIL

static void zebra_chdir (ZebraService zs)
{
    const char *dir ;
    ASSERTZS;
    yaz_log(log_level, "zebra_chdir");
    dir = res_get (zs->global_res, "chdir");
    if (!dir)
	return;
    yaz_log (YLOG_DEBUG, "chdir %s", dir);
#ifdef WIN32
    _chdir(dir);
#else
    chdir (dir);
#endif
}

static ZEBRA_RES zebra_flush_reg (ZebraHandle zh)
{
    ZEBRA_CHECK_HANDLE(zh);
    yaz_log(log_level, "zebra_flush_reg");
    zebraExplain_flush (zh->reg->zei, zh);
    
    extract_flushWriteKeys (zh, 1 /* final */);
    zebra_index_merge (zh );
    return ZEBRA_OK;
}

static struct zebra_register *zebra_register_open(ZebraService zs, 
						  const char *name,
						  int rw, int useshadow,
						  Res res,
						  const char *reg_path);
static void zebra_register_close(ZebraService zs, struct zebra_register *reg);

ZebraHandle zebra_open(ZebraService zs, Res res)
{
    ZebraHandle zh;
    const char *default_encoding;
    if (!log_level_initialized)
    {
        log_level = yaz_log_module_level("zebraapi");
        log_level_initialized = 1;
    }

    yaz_log(log_level, "zebra_open");

    if (!zs)
        return 0;

    zh = (ZebraHandle) xmalloc(sizeof(*zh));
    yaz_log (YLOG_DEBUG, "zebra_open zs=%p returns %p", zs, zh);

    zh->service = zs;
    zh->reg = 0;          /* no register attached yet */
    zh->sets = 0;
    zh->destroyed = 0;
    zh->errCode = 0;
    zh->errString = 0;
    zh->res = 0; 
    zh->session_res = res_open(zs->global_res, res);
    zh->user_perm = 0;
    zh->dbaccesslist = 0;

    zh->reg_name = xstrdup ("");
    zh->path_reg = 0;
    zh->num_basenames = 0;
    zh->basenames = 0;

    zh->approx_limit = DEFAULT_APPROX_LIMIT;
    zh->trans_no = 0;
    zh->trans_w_no = 0;

    zh->lock_normal = 0;
    zh->lock_shadow = 0;

    zh->shadow_enable = 1;
    zh->m_staticrank = 0;

    default_encoding = res_get_def(zh->session_res, "encoding", "ISO-8859-1");

    zh->iconv_to_utf8 =
        yaz_iconv_open ("UTF-8", default_encoding);
    if (zh->iconv_to_utf8 == 0)
        yaz_log (YLOG_WARN, "iconv: %s to UTF-8 unsupported",
           default_encoding);
    zh->iconv_from_utf8 =
        yaz_iconv_open (default_encoding, "UTF-8");
    if (zh->iconv_to_utf8 == 0)
        yaz_log (YLOG_WARN, "iconv: UTF-8 to %s unsupported",
           default_encoding);

    zh->record_encoding = 0;

    zebra_mutex_cond_lock (&zs->session_lock);

    zh->next = zs->sessions;
    zs->sessions = zh;

    zebra_mutex_cond_unlock (&zs->session_lock);

    zh->store_data_buf = 0;

    zh->m_limit = zebra_limit_create(1, 0);

    zh->nmem_error = nmem_create();

    return zh;
}

ZebraService zebra_start (const char *configName)
{
    return zebra_start_res(configName, 0, 0);
}

ZebraService zebra_start_res (const char *configName, Res def_res, Res over_res)
{
    Res res;

    if (!log_level_initialized)
    {
        log_level = yaz_log_module_level("zebraapi");
        log_level_initialized = 1;
    }
    
    yaz_log(YLOG_LOG, "zebra_start %s %s", ZEBRAVER,
	    configName ? configName : "");

    if ((res = res_open(def_res, over_res)))
    {
	const char *passwd_plain = 0;
	const char *passwd_encrypt = 0;
	const char *dbaccess = 0;
        ZebraService zh = 0;

	if (configName)
	{
	    ZEBRA_RES ret = res_read_file(res, configName);
	    if (ret != ZEBRA_OK)
	    {
		res_close(res);
		return 0;
	    }
	}
	zh = xmalloc(sizeof(*zh));
        zh->global_res = res;
        zh->sessions = 0;
        
        zebra_chdir (zh);
        
        zebra_mutex_cond_init (&zh->session_lock);
	passwd_plain = res_get (zh->global_res, "passwd");
	passwd_encrypt = res_get (zh->global_res, "passwd.c");
	dbaccess = res_get (zh->global_res, "dbaccess");

        if (!passwd_plain && !passwd_encrypt)
            zh->passwd_db = NULL;
        else 
        {
            zh->passwd_db = passwd_db_open();
            if (!zh->passwd_db)
                yaz_log (YLOG_WARN|YLOG_ERRNO, "passwd_db_open failed");
            else
	    {
		if (passwd_plain)
		    passwd_db_file_plain(zh->passwd_db, passwd_plain);
		if (passwd_encrypt)
		    passwd_db_file_crypt(zh->passwd_db, passwd_encrypt);
	    }
        }

	if (!dbaccess)
	    zh->dbaccess = NULL;
	else {
	    zh->dbaccess = res_open(NULL, NULL);
	    if (res_read_file(zh->dbaccess, dbaccess) != ZEBRA_OK) {
		yaz_log(YLOG_FATAL, "Failed to read %s", dbaccess);
		return NULL;
	    }
	}

        zh->path_root = res_get (zh->global_res, "root");
	zh->nmem = nmem_create();
	zh->record_classes = recTypeClass_create (zh->global_res, zh->nmem);

	if (1)
	{
	    const char *module_path = res_get(res, "modulePath");
	    if (module_path)
		recTypeClass_load_modules(&zh->record_classes, zh->nmem,
					  module_path);
	}
        return zh;
    }
    return 0;
}

void zebra_filter_info(ZebraService zs, void *cd,
			void (*cb)(void *cd, const char *name))
{
    ASSERTZS;
    assert(cb);
    recTypeClass_info(zs->record_classes, cd, cb);
}

void zebra_pidfname(ZebraService zs, char *path)
{
    ASSERTZS;
    zebra_lock_prefix (zs->global_res, path);
    strcat(path, "zebrasrv.pid");
}

Dict dict_open_res (BFiles bfs, const char *name, int cache, int rw,
		    int compact_flag, Res res)
{
    int page_size = 4096;
    char resource_str[200];
    sprintf (resource_str, "dict.%.100s.pagesize", name);
    assert(bfs);
    assert(name);

    if (res_get_int(res, resource_str, &page_size) == ZEBRA_OK)
	yaz_log(YLOG_LOG, "Using custom dictionary page size %d for %s",
		page_size, name);
    return dict_open(bfs, name, cache, rw, compact_flag, page_size);
}

static
struct zebra_register *zebra_register_open(ZebraService zs, const char *name,
					   int rw, int useshadow, Res res,
					   const char *reg_path)
{
    struct zebra_register *reg;
    int record_compression = REC_COMPRESS_NONE;
    const char *recordCompression = 0;
    const char *profilePath;
    char cwd[1024];
    ZEBRA_RES ret = ZEBRA_OK;

    ASSERTZS;
    
    reg = xmalloc(sizeof(*reg));

    assert (name);
    reg->name = xstrdup (name);

    reg->seqno = 0;
    reg->last_val = 0;

    assert (res);

    yaz_log (YLOG_DEBUG, "zebra_register_open rw=%d useshadow=%d p=%p n=%s rp=%s",
             rw, useshadow, reg, name, reg_path ? reg_path : "(none)");
    
    reg->dh = data1_create();
    if (!reg->dh)
    {
	xfree(reg->name);
	xfree(reg);
        return 0;
    }
    reg->bfs = bfs_create (res_get (res, "register"), reg_path);
    if (!reg->bfs)
    {
        data1_destroy(reg->dh);
	xfree(reg->name);
	xfree(reg);
        return 0;
    }
    if (useshadow)
    {
        if (bf_cache (reg->bfs, res_get (res, "shadow")) == ZEBRA_FAIL)
	{
	    bfs_destroy(reg->bfs);
	    data1_destroy(reg->dh);
	    xfree(reg->name);
	    xfree(reg);
	    return 0;
	}
    }

    getcwd(cwd, sizeof(cwd)-1);
    profilePath = res_get_def(res, "profilePath", DEFAULT_PROFILE_PATH);
    yaz_log(YLOG_DEBUG, "profilePath=%s cwd=%s", profilePath, cwd);

    data1_set_tabpath (reg->dh, profilePath);
    data1_set_tabroot (reg->dh, reg_path);
    reg->recTypes = recTypes_init (zs->record_classes, reg->dh);

    reg->zebra_maps =
	zebra_maps_open(res, reg_path, profilePath);
    if (!reg->zebra_maps)
    {
	recTypes_destroy(reg->recTypes);
	bfs_destroy(reg->bfs);
	data1_destroy(reg->dh);
	xfree(reg->name);
	xfree(reg);
	return 0;
    }
    reg->rank_classes = NULL;

    reg->key_buf = 0;

    reg->keys = zebra_rec_keys_open();

    reg->sortKeys = zebra_rec_keys_open();

    reg->records = 0;
    reg->dict = 0;
    reg->sortIdx = 0;
    reg->isams = 0;
    reg->matchDict = 0;
    reg->isamc = 0;
    reg->isamb = 0;
    reg->zei = 0;
    reg->key_file_no = 0;
    reg->ptr_i = 0;
    
    /* installing rank classes */
    zebraRankInstall (reg, rank_1_class);
    zebraRankInstall (reg, rank_similarity_class);
    zebraRankInstall (reg, rank_static_class);

    recordCompression = res_get_def (res, "recordCompression", "none");
    if (!strcmp (recordCompression, "none"))
	record_compression = REC_COMPRESS_NONE;
    if (!strcmp (recordCompression, "bzip2"))
	record_compression = REC_COMPRESS_BZIP2;

    if (1)
    {
	const char *index_fname = res_get_def(res, "index", "default.idx");
	if (index_fname && *index_fname)
	{
	    if (zebra_maps_read_file(reg->zebra_maps, index_fname) != ZEBRA_OK)
		ret = ZEBRA_FAIL;
	}
    }

    if (!(reg->records = rec_open (reg->bfs, rw, record_compression)))
    {
	yaz_log (YLOG_WARN, "rec_open failed");
	ret = ZEBRA_FAIL;
    }
    if (rw)
    {
        reg->matchDict = dict_open_res (reg->bfs, GMATCH_DICT, 20, 1, 0, res);
    }
    if (!(reg->dict = dict_open_res (reg->bfs, FNAME_DICT, 40, rw, 0, res)))
    {
	yaz_log (YLOG_WARN, "dict_open failed");
	ret = ZEBRA_FAIL;
    }
    if (!(reg->sortIdx = sortIdx_open (reg->bfs, rw)))
    {
	yaz_log (YLOG_WARN, "sortIdx_open failed");
	ret = ZEBRA_FAIL;
    }
    if (res_get_match (res, "isam", "s", ISAM_DEFAULT))
    {
	struct ISAMS_M_s isams_m;
	if (!(reg->isams = isams_open (reg->bfs, FNAME_ISAMS, rw,
				      key_isams_m(res, &isams_m))))
	{
	    yaz_log (YLOG_WARN, "isams_open failed");
	    ret = ZEBRA_FAIL;
	}
    }
    if (res_get_match (res, "isam", "c", ISAM_DEFAULT))
    {
	struct ISAMC_M_s isamc_m;
	if (!(reg->isamc = isamc_open (reg->bfs, FNAME_ISAMC,
				    rw, key_isamc_m(res, &isamc_m))))
	{
	    yaz_log (YLOG_WARN, "isamc_open failed");
	    ret = ZEBRA_FAIL;
	}
    }
    if (res_get_match (res, "isam", "b", ISAM_DEFAULT))
    {
	struct ISAMC_M_s isamc_m;
	
	if (!(reg->isamb = isamb_open (reg->bfs, "isamb",
                                       rw, key_isamc_m(res, &isamc_m), 0)))
	{
	    yaz_log (YLOG_WARN, "isamb_open failed");
	    ret = ZEBRA_FAIL;
	}
    }
    if (res_get_match (res, "isam", "bc", ISAM_DEFAULT))
    {
	struct ISAMC_M_s isamc_m;
	
	if (!(reg->isamb = isamb_open (reg->bfs, "isamb",
                                       rw, key_isamc_m(res, &isamc_m), 1)))
	{
	    yaz_log (YLOG_WARN, "isamb_open failed");
	    ret = ZEBRA_FAIL;
	}
    }
    if (res_get_match (res, "isam", "null", ISAM_DEFAULT))
    {
	struct ISAMC_M_s isamc_m;
	
	if (!(reg->isamb = isamb_open (reg->bfs, "isamb",
                                       rw, key_isamc_m(res, &isamc_m), -1)))
	{
	    yaz_log (YLOG_WARN, "isamb_open failed");
	    ret = ZEBRA_FAIL;
	}
    }
    if (ret == ZEBRA_OK)
    {
	reg->zei = zebraExplain_open(reg->records, reg->dh,
				     res, rw, reg,
				     zebra_extract_explain);
	if (!reg->zei)
	{
	    yaz_log (YLOG_WARN, "Cannot obtain EXPLAIN information");
	    ret = ZEBRA_FAIL;
	}
    }
    
    if (ret != ZEBRA_OK)
    {
	zebra_register_close(zs, reg);
	return 0;
    }
    yaz_log (YLOG_DEBUG, "zebra_register_open ok p=%p", reg);
    return reg;
}

ZEBRA_RES zebra_admin_shutdown (ZebraHandle zh)
{
    ZEBRA_CHECK_HANDLE(zh);
    yaz_log(log_level, "zebra_admin_shutdown");

    zebra_mutex_cond_lock (&zh->service->session_lock);
    zh->service->stop_flag = 1;
    zebra_mutex_cond_unlock (&zh->service->session_lock);
    return ZEBRA_OK;
}

ZEBRA_RES zebra_admin_start (ZebraHandle zh)
{
    ZebraService zs;
    ZEBRA_CHECK_HANDLE(zh);
    yaz_log(log_level, "zebra_admin_start");
    zs = zh->service;
    zebra_mutex_cond_lock (&zs->session_lock);
    zebra_mutex_cond_unlock (&zs->session_lock);
    return ZEBRA_OK;
}

static void zebra_register_close(ZebraService zs, struct zebra_register *reg)
{
    ASSERTZS;
    assert(reg);
    yaz_log(YLOG_DEBUG, "zebra_register_close p=%p", reg);
    reg->stop_flag = 0;
    zebra_chdir (zs);
    
    zebraExplain_close (reg->zei);
    dict_close (reg->dict);
    if (reg->matchDict)
	dict_close (reg->matchDict);
    sortIdx_close (reg->sortIdx);
    if (reg->isams)
	isams_close (reg->isams);
    if (reg->isamc)
	isamc_close (reg->isamc);
    if (reg->isamb)
	isamb_close (reg->isamb);
    rec_close (&reg->records);

    recTypes_destroy (reg->recTypes);
    zebra_maps_close (reg->zebra_maps);
    zebraRankDestroy (reg);
    bfs_destroy (reg->bfs);
    data1_destroy (reg->dh);

    zebra_rec_keys_close(reg->keys);
    zebra_rec_keys_close(reg->sortKeys);

    xfree(reg->key_buf);
    xfree(reg->name);
    xfree(reg);
}

ZEBRA_RES zebra_stop(ZebraService zs)
{
    if (!zs)
        return ZEBRA_OK;
    yaz_log (log_level, "zebra_stop");

    while (zs->sessions)
    {
        zebra_close (zs->sessions);
    }
        
    zebra_mutex_cond_destroy (&zs->session_lock);

    if (zs->passwd_db)
	passwd_db_close (zs->passwd_db);

    recTypeClass_destroy(zs->record_classes);
    nmem_destroy(zs->nmem);
    res_close (zs->global_res);
    xfree(zs);
    return ZEBRA_OK;
}

ZEBRA_RES zebra_close(ZebraHandle zh)
{
    ZebraService zs;
    struct zebra_session **sp;
    int i;

    yaz_log(log_level, "zebra_close");
    ZEBRA_CHECK_HANDLE(zh);

    zh->errCode = 0;
    
    zs = zh->service;
    yaz_log (YLOG_DEBUG, "zebra_close zh=%p", zh);
    resultSetDestroy (zh, -1, 0, 0);

    if (zh->reg)
        zebra_register_close(zh->service, zh->reg);
    zebra_close_res (zh);
    res_close(zh->session_res);

    xfree(zh->record_encoding);

    xfree(zh->dbaccesslist);

    for (i = 0; i < zh->num_basenames; i++)
        xfree(zh->basenames[i]);
    xfree(zh->basenames);

    if (zh->iconv_to_utf8 != 0)
        yaz_iconv_close (zh->iconv_to_utf8);
    if (zh->iconv_from_utf8 != 0)
        yaz_iconv_close (zh->iconv_from_utf8);

    zebra_mutex_cond_lock (&zs->session_lock);
    zebra_lock_destroy (zh->lock_normal);
    zebra_lock_destroy (zh->lock_shadow);
    sp = &zs->sessions;
    while (1)
    {
	assert (*sp);
	if (*sp == zh)
	{
	    *sp = (*sp)->next;
	    break;
	}
	sp = &(*sp)->next;
    }
    zebra_mutex_cond_unlock (&zs->session_lock);
    xfree(zh->reg_name);
    xfree(zh->user_perm);
    zh->service = 0; /* more likely to trigger an assert */

    zebra_limit_destroy(zh->m_limit);

    nmem_destroy(zh->nmem_error);

    xfree(zh->path_reg);
    xfree(zh);
    return ZEBRA_OK;
}

struct map_baseinfo {
    ZebraHandle zh;
    NMEM mem;
    int num_bases;
    char **basenames;
    int new_num_bases;
    char **new_basenames;
    int new_num_max;
};

static void zebra_open_res(ZebraHandle zh)
{
    char fname[512];
    ASSERTZH;
    zh->errCode = 0;

    if (zh->path_reg)
    {
        sprintf(fname, "%.200s/zebra.cfg", zh->path_reg);
        zh->res = res_open(zh->session_res, 0);
	res_read_file(zh->res, fname);
    }
    else if (*zh->reg_name == 0)
    {
        zh->res = res_open(zh->session_res, 0);
    }
    else
    {
        yaz_log (YLOG_WARN, "no register root specified");
        zh->res = 0;  /* no path for register - fail! */
    }
}

static void zebra_close_res (ZebraHandle zh)
{
    ASSERTZH;
    zh->errCode = 0;
    res_close (zh->res);
    zh->res = 0;
}

static void zebra_select_register (ZebraHandle zh, const char *new_reg)
{
    ASSERTZH;
    zh->errCode = 0;
    if (zh->res && strcmp (zh->reg_name, new_reg) == 0)
        return;
    if (!zh->res)
    {
        assert (zh->reg == 0);
        assert (*zh->reg_name == 0);
    }
    else
    {
        if (zh->reg)
        {
            resultSetInvalidate (zh);
            zebra_register_close(zh->service, zh->reg);
            zh->reg = 0;
        }
        zebra_close_res(zh);
    }
    xfree(zh->reg_name);
    zh->reg_name = xstrdup (new_reg);

    xfree(zh->path_reg);
    zh->path_reg = 0;
    if (zh->service->path_root)
    {
        zh->path_reg = xmalloc(strlen(zh->service->path_root) + 
                                strlen(zh->reg_name) + 3);
        strcpy (zh->path_reg, zh->service->path_root);
        if (*zh->reg_name)
        {
            strcat (zh->path_reg, "/");
            strcat (zh->path_reg, zh->reg_name);
        }
    }
    zebra_open_res(zh);
    
    if (zh->lock_normal)
        zebra_lock_destroy (zh->lock_normal);
    zh->lock_normal = 0;

    if (zh->lock_shadow)
        zebra_lock_destroy (zh->lock_shadow);
    zh->lock_shadow = 0;

    if (zh->res)
    {
        char fname[512];
        const char *lock_area = res_get (zh->res, "lockDir");
        
        if (!lock_area && zh->path_reg)
            res_set (zh->res, "lockDir", zh->path_reg);
        sprintf (fname, "norm.%s.LCK", zh->reg_name);
        zh->lock_normal =
            zebra_lock_create (res_get(zh->res, "lockDir"), fname);
        
        sprintf (fname, "shadow.%s.LCK", zh->reg_name);
        zh->lock_shadow =
            zebra_lock_create (res_get(zh->res, "lockDir"), fname);

	if (!zh->lock_normal || !zh->lock_shadow)
	{
	    if (zh->lock_normal)
	    {
		zebra_lock_destroy(zh->lock_normal);
		zh->lock_normal = 0;
	    }
	    if (zh->lock_shadow)
	    {
		zebra_lock_destroy(zh->lock_shadow);
		zh->lock_shadow = 0;
	    }
	    zebra_close_res(zh);
	}
    }
    if (zh->res)
    {
	int approx = 0;
	if (res_get_int(zh->res, "estimatehits", &approx) == ZEBRA_OK)
	    zebra_set_approx_limit(zh, approx);
    }
    if (zh->res)
    {
	if (res_get_int(zh->res, "staticrank", &zh->m_staticrank) == ZEBRA_OK)
	    yaz_log(YLOG_LOG, "static rank set and is %d", zh->m_staticrank);
    }
}

void map_basenames_func (void *vp, const char *name, const char *value)
{
    struct map_baseinfo *p = (struct map_baseinfo *) vp;
    int i, no;
    char fromdb[128], todb[8][128];

    assert(value);
    assert(name);
    assert(vp);
    
    no =
	sscanf (value, "%127s %127s %127s %127s %127s %127s %127s %127s %127s",
		fromdb,	todb[0], todb[1], todb[2], todb[3], todb[4],
		todb[5], todb[6], todb[7]);
    if (no < 2)
	return ;
    no--;
    for (i = 0; i<p->num_bases; i++)
	if (p->basenames[i] && !STRCASECMP (p->basenames[i], fromdb))
	{
	    p->basenames[i] = 0;
	    for (i = 0; i < no; i++)
	    {
		if (p->new_num_bases == p->new_num_max)
		    return;
		p->new_basenames[(p->new_num_bases)++] = 
		    nmem_strdup (p->mem, todb[i]);
	    }
	    return;
	}
}

int zebra_select_default_database(ZebraHandle zh)
{
    if (!zh->res)
    {
	/* no database has been selected - so we select based on
	   resource setting (including group)
	*/
	const char *group = res_get(zh->session_res, "group");
	const char *v = res_get_prefix(zh->session_res,
				       "database", group, "Default");
	return zebra_select_database(zh, v);
    }
    return 0;
}

void map_basenames (ZebraHandle zh, ODR stream,
		    int *num_bases, char ***basenames)
{
    struct map_baseinfo info;
    struct map_baseinfo *p = &info;
    int i;
    ASSERTZH;
    yaz_log(log_level, "map_basenames ");
    assert(stream);

    info.zh = zh;

    info.num_bases = *num_bases;
    info.basenames = *basenames;
    info.new_num_max = 128;
    info.new_num_bases = 0;
    info.new_basenames = (char **)
	odr_malloc (stream, sizeof(*info.new_basenames) * info.new_num_max);
    info.mem = stream->mem;

    res_trav (zh->session_res, "mapdb", &info, map_basenames_func);
    
    for (i = 0; i<p->num_bases; i++)
	if (p->basenames[i] && p->new_num_bases < p->new_num_max)
	{
	    p->new_basenames[(p->new_num_bases)++] = 
		nmem_strdup (p->mem, p->basenames[i]);
	}
    *num_bases = info.new_num_bases;
    *basenames = info.new_basenames;
    for (i = 0; i<*num_bases; i++)
	yaz_log (YLOG_DEBUG, "base %s", (*basenames)[i]);
}

ZEBRA_RES zebra_select_database (ZebraHandle zh, const char *basename)
{
    ZEBRA_CHECK_HANDLE(zh);

    yaz_log(log_level, "zebra_select_database %s",basename);
    assert(basename);
    return zebra_select_databases (zh, 1, &basename);
}

ZEBRA_RES zebra_select_databases (ZebraHandle zh, int num_bases,
				  const char **basenames)
{
    int i;
    const char *cp;
    int len = 0;
    char *new_reg = 0;

    ZEBRA_CHECK_HANDLE(zh);
    assert(basenames);

    yaz_log(log_level, "zebra_select_databases n=%d [0]=%s",
		    num_bases,basenames[0]);
    zh->errCode = 0;
    
    if (num_bases < 1)
    {
        zh->errCode = YAZ_BIB1_COMBI_OF_SPECIFIED_DATABASES_UNSUPP;
        return ZEBRA_FAIL;
    }

    /* Check if the user has access to all databases (Seb) */
    /* You could argue that this should happen later, after we have
     * determined that the database(s) exist. */
    if (zh->dbaccesslist) {
	for (i = 0; i < num_bases; i++) {
	    const char *db = basenames[i];
	    char *p, *pp;
	    for (p = zh->dbaccesslist; p && *p; p = pp) {
		int len;
		if ((pp = strchr(p, '+'))) {
		    len = pp - p;
		    pp++;
		}
		else
		    len = strlen(p);
		if (len == strlen(db) && !strncmp(db, p, len))
		    break;
	    }
	    if (!p) {
		zh->errCode = YAZ_BIB1_ACCESS_TO_SPECIFIED_DATABASE_DENIED;
		return ZEBRA_FAIL;
	    }
	}
    }

    for (i = 0; i < zh->num_basenames; i++)
        xfree(zh->basenames[i]);
    xfree(zh->basenames);
    
    zh->num_basenames = num_bases;
    zh->basenames = xmalloc(zh->num_basenames * sizeof(*zh->basenames));
    for (i = 0; i < zh->num_basenames; i++)
        zh->basenames[i] = xstrdup (basenames[i]);

    cp = strrchr(basenames[0], '/');
    if (cp)
    {
        len = cp - basenames[0];
        new_reg = xmalloc(len + 1);
        memcpy (new_reg, basenames[0], len);
        new_reg[len] = '\0';
    }
    else
        new_reg = xstrdup ("");
    for (i = 1; i<num_bases; i++)
    {
        const char *cp1;

        cp1 = strrchr (basenames[i], '/');
        if (cp)
        {
            if (!cp1)
            {
                zh->errCode = YAZ_BIB1_COMBI_OF_SPECIFIED_DATABASES_UNSUPP;
                return -1;
            }
            if (len != cp1 - basenames[i] ||
                memcmp (basenames[i], new_reg, len))
            {
                zh->errCode = YAZ_BIB1_COMBI_OF_SPECIFIED_DATABASES_UNSUPP;
                return -1;
            }
        }
        else
        {
            if (cp1)
            {
                zh->errCode = YAZ_BIB1_COMBI_OF_SPECIFIED_DATABASES_UNSUPP;
                return ZEBRA_FAIL;
            }
        }
    }
    zebra_select_register (zh, new_reg);
    xfree(new_reg);
    if (!zh->res)
    {
        zh->errCode = YAZ_BIB1_DATABASE_UNAVAILABLE;
        return ZEBRA_FAIL;
    }
    if (!zh->lock_normal || !zh->lock_shadow)
    {
        zh->errCode = YAZ_BIB1_TEMPORARY_SYSTEM_ERROR;
	return ZEBRA_FAIL;
    }
    return ZEBRA_OK;
}

ZEBRA_RES zebra_set_approx_limit(ZebraHandle zh, zint approx_limit)
{
    if (approx_limit == 0)
	approx_limit = DEFAULT_APPROX_LIMIT;
    zh->approx_limit = approx_limit;
    return ZEBRA_OK;
}

ZEBRA_RES zebra_search_RPN(ZebraHandle zh, ODR o, Z_RPNQuery *query,
			   const char *setname, zint *hits)
{
    ZEBRA_RES r;
    
    ZEBRA_CHECK_HANDLE(zh);

    assert(o);
    assert(query);
    assert(hits);
    assert(setname);
    yaz_log(log_level, "zebra_search_rpn");
    zh->hits = 0;
    *hits = 0;

    if (zebra_begin_read(zh) == ZEBRA_FAIL)
	return ZEBRA_FAIL;

    r = resultSetAddRPN(zh, odr_extract_mem(o), query, 
			zh->num_basenames, zh->basenames, setname);
    zebra_end_read(zh);
    *hits = zh->hits;
    return r;
}

ZEBRA_RES zebra_records_retrieve(ZebraHandle zh, ODR stream,
				 const char *setname,
				 Z_RecordComposition *comp,
				 oid_value input_format, int num_recs,
				 ZebraRetrievalRecord *recs)
{
    ZebraMetaRecord *poset;
    int i;
    ZEBRA_RES ret = ZEBRA_OK;
    zint *pos_array;

    ZEBRA_CHECK_HANDLE(zh);
    assert(stream);
    assert(setname);
    assert(recs);
    assert(num_recs>0);

    yaz_log(log_level, "zebra_records_retrieve n=%d", num_recs);

    if (!zh->res)
    {
	zebra_setError(zh, YAZ_BIB1_SPECIFIED_RESULT_SET_DOES_NOT_EXIST,
		       setname);
        return ZEBRA_FAIL;
    }
    
    if (zebra_begin_read (zh) == ZEBRA_FAIL)
	return ZEBRA_FAIL;

    pos_array = (zint *) xmalloc(num_recs * sizeof(*pos_array));
    for (i = 0; i<num_recs; i++)
	pos_array[i] = recs[i].position;
    poset = zebra_meta_records_create(zh, setname, num_recs, pos_array);
    if (!poset)
    {
        yaz_log (YLOG_DEBUG, "zebraPosSetCreate error");
	zebra_setError(zh, YAZ_BIB1_SPECIFIED_RESULT_SET_DOES_NOT_EXIST,
		       setname);
	ret = ZEBRA_FAIL;
    }
    else
    {
	for (i = 0; i<num_recs; i++)
	{
	    if (poset[i].term)
	    {
		recs[i].errCode = 0;
		recs[i].format = VAL_SUTRS;
		recs[i].len = strlen(poset[i].term);
		recs[i].buf = poset[i].term;
		recs[i].base = poset[i].db;
	    }
	    else if (poset[i].sysno)
	    {
		char *buf;
		int len;
		zebra_snippets *hit_snippet = zebra_snippets_create();

		zebra_snippets_hit_vector(zh, setname, poset[i].sysno, 
					  hit_snippet);

		recs[i].errCode =
		    zebra_record_fetch(zh, poset[i].sysno, poset[i].score,
				       hit_snippet,
				       stream, input_format, comp,
				       &recs[i].format, &buf, &len,
				       &recs[i].base, &recs[i].errString);
		
		recs[i].len = len;
		if (len > 0)
		{
		    recs[i].buf = (char*) odr_malloc(stream, len);
		    memcpy(recs[i].buf, buf, len);
		}
		else
		    recs[i].buf = buf;
                recs[i].score = poset[i].score;
                recs[i].sysno = poset[i].sysno;
		zebra_snippets_destroy(hit_snippet);
	    }
	    else
	    {
	        /* only need to set it once */
		if (pos_array[i] < zh->approx_limit && ret == ZEBRA_OK)
		{
		    zebra_setError_zint(zh,
					YAZ_BIB1_PRESENT_REQUEST_OUT_OF_RANGE,
					pos_array[i]);
		    ret = ZEBRA_FAIL;
		    break;
		}
		recs[i].buf = 0;  /* no record and no error issued */
		recs[i].len = 0;
		recs[i].errCode = 0;
		recs[i].format = VAL_NONE;
		recs[i].sysno = 0;
	    }
	}
	zebra_meta_records_destroy(zh, poset, num_recs);
    }
    zebra_end_read (zh);
    xfree(pos_array);
    return ret;
}

ZEBRA_RES zebra_scan_PQF(ZebraHandle zh, ODR stream, const char *query,
			 int *position,
			 int *num_entries, ZebraScanEntry **entries,
			 int *is_partial,
			 const char *setname)
{
    YAZ_PQF_Parser pqf_parser = yaz_pqf_create ();
    Z_AttributesPlusTerm *zapt;
    int *attributeSet;
    ZEBRA_RES res;
    
    if (!(zapt = yaz_pqf_scan(pqf_parser, stream, &attributeSet, query)))
    {
	res = ZEBRA_FAIL;
	zh->errCode = YAZ_BIB1_SCAN_MALFORMED_SCAN;
    }
    else
	res = zebra_scan(zh, stream, zapt, VAL_BIB1,
			 position, num_entries, entries, is_partial,
			 setname);
    yaz_pqf_destroy (pqf_parser);
    return res;
}

ZEBRA_RES zebra_scan(ZebraHandle zh, ODR stream, Z_AttributesPlusTerm *zapt,
		     oid_value attributeset,
		     int *position,
		     int *num_entries, ZebraScanEntry **entries,
		     int *is_partial,
		     const char *setname)
{
    ZEBRA_RES res;
    RSET limit_rset = 0;

    ZEBRA_CHECK_HANDLE(zh);

    assert(stream);
    assert(zapt);
    assert(position);
    assert(num_entries);
    assert(is_partial);
    assert(entries);
    yaz_log(log_level, "zebra_scan");

    if (zebra_begin_read (zh) == ZEBRA_FAIL)
    {
	*entries = 0;
	*num_entries = 0;
	return ZEBRA_FAIL;
    }
    if (setname)
    {
	limit_rset = resultSetRef(zh, setname);
	if (!limit_rset)
	{
	    zebra_setError(zh, 
			   YAZ_BIB1_SPECIFIED_RESULT_SET_DOES_NOT_EXIST,
			   setname);
	    zebra_end_read (zh);
	    return ZEBRA_FAIL;
	}
    }
    res = rpn_scan (zh, stream, zapt, attributeset,
		    zh->num_basenames, zh->basenames, position,
		    num_entries, entries, is_partial, limit_rset, 0);
    zebra_end_read (zh);
    return res;
}

ZEBRA_RES zebra_sort (ZebraHandle zh, ODR stream,
		      int num_input_setnames, const char **input_setnames,
		      const char *output_setname,
		      Z_SortKeySpecList *sort_sequence,
		      int *sort_status)
{
    ZEBRA_RES res;
    ZEBRA_CHECK_HANDLE(zh);
    assert(stream);
    assert(num_input_setnames>0);
    assert(input_setnames);
    assert(sort_sequence);
    assert(sort_status);
    yaz_log(log_level, "zebra_sort");

    if (zebra_begin_read(zh) == ZEBRA_FAIL)
	return ZEBRA_FAIL;
    res = resultSetSort(zh, stream->mem, num_input_setnames, input_setnames,
			output_setname, sort_sequence, sort_status);
    zebra_end_read(zh);
    return res;
}

int zebra_deleteResultSet(ZebraHandle zh, int function,
			  int num_setnames, char **setnames,
			  int *statuses)
{
    int i, status;
    ASSERTZH;
    assert(statuses);
    yaz_log(log_level, "zebra_deleteResultSet n=%d",num_setnames);

    if (zebra_begin_read(zh))
	return Z_DeleteStatus_systemProblemAtTarget;
    switch (function)
    {
    case Z_DeleteResultSetRequest_list:
	assert(num_setnames>0);
	assert(setnames);
	resultSetDestroy (zh, num_setnames, setnames, statuses);
	break;
    case Z_DeleteResultSetRequest_all:
	resultSetDestroy (zh, -1, 0, statuses);
	break;
    }
    zebra_end_read (zh);
    status = Z_DeleteStatus_success;
    for (i = 0; i<num_setnames; i++)
	if (statuses[i] == Z_DeleteStatus_resultSetDidNotExist)
	    status = statuses[i];
    return status;
}

int zebra_errCode (ZebraHandle zh)
{
    if (zh)
    {
        yaz_log(log_level, "zebra_errCode: %d",zh->errCode);
        return zh->errCode;
    }
    yaz_log(log_level, "zebra_errCode: o");
    return 0; 
}

const char *zebra_errString (ZebraHandle zh)
{
    const char *e = 0;
    if (zh)
        e= diagbib1_str (zh->errCode);
    yaz_log(log_level, "zebra_errString: %s",e);
    return e;
}

char *zebra_errAdd (ZebraHandle zh)
{
    char *a = 0;
    if (zh)
        a= zh->errString;
    yaz_log(log_level, "zebra_errAdd: %s",a);
    return a;
}

ZEBRA_RES zebra_auth (ZebraHandle zh, const char *user, const char *pass)
{
    const char *p;
    const char *astring;
    char u[40];
    ZebraService zs;

    ZEBRA_CHECK_HANDLE(zh);

    zs = zh->service;
    
    sprintf(u, "perm.%.30s", user ? user : "anonymous");
    p = res_get(zs->global_res, u);
    xfree(zh->user_perm);
    zh->user_perm = xstrdup(p ? p : "r");

    /* Determine database access list */
    astring = res_get(zs->dbaccess, user ? user : "anonymous");
    if (astring)
	zh->dbaccesslist = xstrdup(astring);
    else
	zh->dbaccesslist = 0;

    /* users that don't require a password .. */
    if (zh->user_perm && strchr(zh->user_perm, 'a'))
	return ZEBRA_OK;
    
    if (!zs->passwd_db || !passwd_db_auth (zs->passwd_db, user, pass))
	return ZEBRA_OK;
    return ZEBRA_FAIL;
}

ZEBRA_RES zebra_admin_import_begin (ZebraHandle zh, const char *database,
                               const char *record_type)
{
    yaz_log(log_level, "zebra_admin_import_begin db=%s rt=%s", 
		     database, record_type);
    if (zebra_select_database(zh, database) == ZEBRA_FAIL)
        return ZEBRA_FAIL;
    return zebra_begin_trans(zh, 1);
}

ZEBRA_RES zebra_admin_import_end (ZebraHandle zh)
{
    ZEBRA_CHECK_HANDLE(zh);
    yaz_log(log_level, "zebra_admin_import_end");
    return zebra_end_trans(zh);
}

ZEBRA_RES zebra_admin_import_segment (ZebraHandle zh, Z_Segment *segment)
{
    ZEBRA_RES res = ZEBRA_OK;
    SYSNO sysno;
    int i;
    ZEBRA_CHECK_HANDLE(zh);
    yaz_log(log_level, "zebra_admin_import_segment");

    for (i = 0; i<segment->num_segmentRecords; i++)
    {
	Z_NamePlusRecord *npr = segment->segmentRecords[i];

	if (npr->which == Z_NamePlusRecord_intermediateFragment)
	{
	    Z_FragmentSyntax *fragment = npr->u.intermediateFragment;
	    if (fragment->which == Z_FragmentSyntax_notExternallyTagged)
	    {
		Odr_oct *oct = fragment->u.notExternallyTagged;
		sysno = 0;
		
		if (zebra_update_record(zh, 
					0, /* record Type */
					&sysno,
					0, /* match */
					0, /* fname */
					(const char *) oct->buf, oct->len,
					0) == ZEBRA_FAIL)
		    res = ZEBRA_FAIL;
	    }
	}
    }
    return res;
}

ZEBRA_RES zebra_admin_exchange_record(ZebraHandle zh,
				      const char *rec_buf,
				      size_t rec_len,
				      const char *recid_buf, size_t recid_len,
				      int action)
    /* 1 = insert. Fail it already exists */
    /* 2 = replace. Fail it does not exist */
    /* 3 = delete. Fail if does not exist */
    /* 4 = update. Insert/replace */
{
    ZEBRA_RES res;
    SYSNO sysno = 0;
    char *rinfo = 0;
    char recid_z[256];
    int db_ord;
    ZEBRA_CHECK_HANDLE(zh);
    assert(action>0 && action <=4);
    assert(rec_buf);

    yaz_log(log_level, "zebra_admin_exchange_record ac=%d", action);

    if (!recid_buf || recid_len <= 0 || recid_len >= sizeof(recid_z))
    {
	zebra_setError(zh, YAZ_BIB1_ES_IMMEDIATE_EXECUTION_FAILED,
		       "no record ID or empty record ID");
        return ZEBRA_FAIL;
    }

    memcpy (recid_z, recid_buf, recid_len);
    recid_z[recid_len] = 0;

    if (zebra_begin_trans(zh, 1) == ZEBRA_FAIL)
	return ZEBRA_FAIL;

    db_ord = zebraExplain_get_database_ord(zh->reg->zei);
    rinfo = dict_lookup_ord(zh->reg->matchDict, db_ord, recid_z);
    if (rinfo)
    {
        if (action == 1)  /* fail if insert */
        {
	    if (zebra_end_trans(zh) != ZEBRA_OK)
		yaz_log(YLOG_WARN, "zebra_end_trans failed");
	    zebra_setError(zh, YAZ_BIB1_ES_IMMEDIATE_EXECUTION_FAILED,
			   "Cannot insert record: already exist");
	    return ZEBRA_FAIL;
	}

        memcpy (&sysno, rinfo+1, sizeof(sysno));
    }
    else
    {
        if (action == 2 || action == 3) /* fail if delete or update */
        {
	    if (zebra_end_trans(zh) != ZEBRA_OK)
		yaz_log(YLOG_WARN, "zebra_end_trans failed");
	    zebra_setError(zh, YAZ_BIB1_ES_IMMEDIATE_EXECUTION_FAILED,
			   "Cannot delete/update record: does not exist");
            return ZEBRA_FAIL;
	}
	action = 1;  /* make it an insert (if it's an update).. */
    }
    res = zebra_buffer_extract_record(zh, rec_buf, rec_len,
                                      action == 3 ? 1 : 0 /* delete flag */,
                                      0, /* test mode */
                                      0, /* recordType */
                                      &sysno, 
                                      0, /* match */
                                      0, /* fname */
                                      0, /* force update */
                                      1  /* allow update */
	);
    if (res == ZEBRA_FAIL)
    {
	zebra_setError(zh, YAZ_BIB1_ES_IMMEDIATE_EXECUTION_FAILED,
		       "Unable to parse record");
    }
    if (action == 1)
    {
        dict_insert_ord(zh->reg->matchDict, db_ord, recid_z,
			sizeof(sysno), &sysno);
    }
    else if (action == 3)
    {
        dict_delete_ord(zh->reg->matchDict, db_ord, recid_z);
    }
    if (zebra_end_trans(zh) != ZEBRA_OK)
    {
	yaz_log(YLOG_WARN, "zebra_end_trans failed");
	res = ZEBRA_FAIL;
    }
    return res;
}

int delete_w_handle(const char *info, void *handle)
{
    ZebraHandle zh = (ZebraHandle) handle;
    ISAM_P pos;
    ASSERTZH;

    if (*info == sizeof(pos))
    {
	memcpy (&pos, info+1, sizeof(pos));
	isamb_unlink(zh->reg->isamb, pos);
    }
    return 0;
}

static int delete_SU_handle(void *handle, int ord)
{
    ZebraHandle zh = (ZebraHandle) handle;
    char ord_buf[20];
    int ord_len;

    ord_len = key_SU_encode (ord, ord_buf);
    ord_buf[ord_len] = '\0';

    assert (zh->reg->isamb);
    dict_delete_subtree(zh->reg->dict, ord_buf,
			zh, delete_w_handle);
    return 0;
}

ZEBRA_RES zebra_drop_database(ZebraHandle zh, const char *db)
{
    ZEBRA_RES ret = ZEBRA_OK;

    yaz_log(log_level, "zebra_drop_database %s", db);
    ZEBRA_CHECK_HANDLE(zh);

    if (zebra_select_database (zh, db) == ZEBRA_FAIL)
        return ZEBRA_FAIL;
    if (zebra_begin_trans (zh, 1) == ZEBRA_FAIL)
        return ZEBRA_FAIL;
    if (zh->reg->isamb)
    {
	int db_ord;
	zebraExplain_curDatabase (zh->reg->zei, db);
	db_ord = zebraExplain_get_database_ord(zh->reg->zei);
	dict_delete_subtree_ord(zh->reg->matchDict, db_ord,
				0 /* handle */, 0 /* func */);
	zebraExplain_trav_ord(zh->reg->zei, zh, delete_SU_handle);
	zebraExplain_removeDatabase(zh->reg->zei, zh);
    }
    else
    {
	yaz_log(YLOG_WARN, "drop database only supported for isam:b");
	zebra_setError(zh, YAZ_BIB1_ES_IMMEDIATE_EXECUTION_FAILED,
		       "drop database only supported for isam:b");
	ret = ZEBRA_FAIL;
    }
    if (zebra_end_trans (zh) != ZEBRA_OK)
    {
	yaz_log(YLOG_WARN, "zebra_end_trans failed");
	ret = ZEBRA_FAIL;
    }
    return ret;
}

ZEBRA_RES zebra_create_database (ZebraHandle zh, const char *db)
{
    yaz_log(log_level, "zebra_create_database %s", db);
    ZEBRA_CHECK_HANDLE(zh);
    assert(db);

    if (zebra_select_database (zh, db) == ZEBRA_FAIL)
        return ZEBRA_FAIL;
    if (zebra_begin_trans (zh, 1))
        return ZEBRA_FAIL;

    /* announce database */
    if (zebraExplain_newDatabase (zh->reg->zei, db, 0 
                                  /* explainDatabase */))
    {
        if (zebra_end_trans (zh) != ZEBRA_OK)
	{
	    yaz_log(YLOG_WARN, "zebra_end_trans failed");
	}
	zebra_setError(zh, YAZ_BIB1_ES_IMMEDIATE_EXECUTION_FAILED, db);
	return ZEBRA_FAIL;
    }
    return zebra_end_trans (zh);
}

int zebra_string_norm (ZebraHandle zh, unsigned reg_id,
		       const char *input_str, int input_len,
		       char *output_str, int output_len)
{
    WRBUF wrbuf;
    ASSERTZH;
    assert(input_str);
    assert(output_str);
    yaz_log(log_level, "zebra_string_norm ");

    if (!zh->reg->zebra_maps)
	return -1;
    wrbuf = zebra_replace(zh->reg->zebra_maps, reg_id, "",
			  input_str, input_len);
    if (!wrbuf)
	return -2;
    if (wrbuf_len(wrbuf) >= output_len)
	return -3;
    if (wrbuf_len(wrbuf))
	memcpy (output_str, wrbuf_buf(wrbuf), wrbuf_len(wrbuf));
    output_str[wrbuf_len(wrbuf)] = '\0';
    return wrbuf_len(wrbuf);
}

static void zebra_set_state (ZebraHandle zh, int val, int seqno)
{
    char state_fname[256];
    char *fname;
    long p = getpid();
    FILE *f;
    ASSERTZH;
    yaz_log(log_level, "zebra_set_state v=%d seq=%d", val, seqno);

    sprintf (state_fname, "state.%s.LCK", zh->reg_name);
    fname = zebra_mk_fname (res_get(zh->res, "lockDir"), state_fname);
    f = fopen (fname, "w");

    yaz_log (YLOG_DEBUG, "zebra_set_state: %c %d %ld", val, seqno, p);
    fprintf (f, "%c %d %ld\n", val, seqno, p);
    fclose (f);
    xfree(fname);
}

static void zebra_get_state (ZebraHandle zh, char *val, int *seqno)
{
    char state_fname[256];
    char *fname;
    FILE *f;

    ASSERTZH;
    yaz_log(log_level, "zebra_get_state ");

    sprintf (state_fname, "state.%s.LCK", zh->reg_name);
    fname = zebra_mk_fname (res_get(zh->res, "lockDir"), state_fname);
    f = fopen (fname, "r");
    *val = 'o';
    *seqno = 0;

    if (f)
    {
        fscanf (f, "%c %d", val, seqno);
        fclose (f);
    }
    xfree(fname);
}

ZEBRA_RES zebra_begin_read (ZebraHandle zh)
{
    return zebra_begin_trans(zh, 0);
}

ZEBRA_RES zebra_end_read (ZebraHandle zh)
{
    return zebra_end_trans(zh);
}

static void read_res_for_transaction(ZebraHandle zh)
{
    const char *group = res_get(zh->res, "group");
    const char *v;
    /* FIXME - do we still use groups ?? */
    
    zh->m_group = group;
    v = res_get_prefix(zh->res, "followLinks", group, "1");
    zh->m_follow_links = atoi(v);

    zh->m_record_id = res_get_prefix(zh->res, "recordId", group, 0);
    zh->m_record_type = res_get_prefix(zh->res, "recordType", group, 0);

    v = res_get_prefix(zh->res, "storeKeys", group, "1");
    zh->m_store_keys = atoi(v);

    v = res_get_prefix(zh->res, "storeData", group, "1");
    zh->m_store_data = atoi(v);

    v = res_get_prefix(zh->res, "explainDatabase", group, "0");
    zh->m_explain_database = atoi(v);

    v = res_get_prefix(zh->res, "openRW", group, "1");
    zh->m_flag_rw = atoi(v);

    v = res_get_prefix(zh->res, "fileVerboseLimit", group, "100000");
    zh->m_file_verbose_limit = atoi(v);
}

ZEBRA_RES zebra_begin_trans(ZebraHandle zh, int rw)
{
    ZEBRA_CHECK_HANDLE(zh);
    zebra_select_default_database(zh);
    if (!zh->res)
    {
	zebra_setError(zh, YAZ_BIB1_TEMPORARY_SYSTEM_ERROR,
		       "zebra_begin_trans: no database selected");
        return ZEBRA_FAIL;
    }
    ASSERTZHRES;
    yaz_log(log_level, "zebra_begin_trans rw=%d",rw);

    if (zh->user_perm)
    {
	if (rw && !strchr(zh->user_perm, 'w'))
	{
	    zebra_setError(
		zh,
		YAZ_BIB1_ES_PERMISSION_DENIED_ON_ES_CANNOT_MODIFY_OR_DELETE,
		0);
	    return ZEBRA_FAIL;
	}
    }

    assert (zh->res);
    if (rw)
    {
        int pass;
        int seqno = 0;
        char val = '?';
        const char *rval = 0;
        
        (zh->trans_no++);
        if (zh->trans_w_no)
	{
	    read_res_for_transaction(zh);
            return 0;
	}
        if (zh->trans_no != 1)
        {
	    zebra_setError(zh, YAZ_BIB1_TEMPORARY_SYSTEM_ERROR,
			   "zebra_begin_trans: no write trans within read");
            return ZEBRA_FAIL;
        }
        if (zh->reg)
	{
            resultSetInvalidate (zh);
            zebra_register_close(zh->service, zh->reg);
	}
        zh->trans_w_no = zh->trans_no;

        zh->records_inserted = 0;
        zh->records_updated = 0;
        zh->records_deleted = 0;
        zh->records_processed = 0;
        
#if HAVE_SYS_TIMES_H
        times (&zh->tms1);
#endif
        /* lock */
        if (zh->shadow_enable)
            rval = res_get (zh->res, "shadow");
        
        for (pass = 0; pass < 2; pass++)
        {
            if (rval)
            {
                zebra_lock_r (zh->lock_normal);
                zebra_lock_w (zh->lock_shadow);
            }
            else
            {
                zebra_lock_w (zh->lock_normal);
                zebra_lock_w (zh->lock_shadow);
            }
            
            zebra_get_state (zh, &val, &seqno);
            if (val == 'c')
            {
                yaz_log (YLOG_WARN, "previous transaction didn't finish commit");
                zebra_unlock (zh->lock_shadow);
                zebra_unlock (zh->lock_normal);
                zebra_commit (zh);
                continue;
            }
            else if (val == 'd')
            {
                if (rval)
                {
                    BFiles bfs = bfs_create (res_get (zh->res, "shadow"),
                                             zh->path_reg);
                    yaz_log (YLOG_WARN, "previous transaction didn't reach commit");
                    bf_commitClean (bfs, rval);
                    bfs_destroy (bfs);
		}
                else
                {
                    yaz_log (YLOG_WARN, "your previous transaction didn't finish");
                }
            }
            break;
        }
        if (pass == 2)
        {
            yaz_log (YLOG_FATAL, "zebra_begin_trans couldn't finish commit");
            abort();
            return ZEBRA_FAIL;
        }
        zebra_set_state (zh, 'd', seqno);
        
        zh->reg = zebra_register_open(zh->service, zh->reg_name,
				      1, rval ? 1 : 0, zh->res,
				      zh->path_reg);
        if (zh->reg)
            zh->reg->seqno = seqno;
        else
        {
            zebra_set_state (zh, 'o', seqno);
            
            zebra_unlock (zh->lock_shadow);
            zebra_unlock (zh->lock_normal);

            zh->trans_no--;
            zh->trans_w_no = 0;

	    zebra_setError(zh, YAZ_BIB1_TEMPORARY_SYSTEM_ERROR,
			   "zebra_begin_trans: cannot open register");
            yaz_log(YLOG_FATAL, "%s", zh->errString);
            return ZEBRA_FAIL;
        }
	zebraExplain_curDatabase(zh->reg->zei, zh->basenames[0]);
    }
    else
    {
        int dirty = 0;
        char val;
        int seqno;
        
        (zh->trans_no)++;
        
        if (zh->trans_no != 1)
        {
            return zebra_flush_reg (zh);
        }
#if HAVE_SYS_TIMES_H
        times (&zh->tms1);
#endif
        if (!zh->res)
        {
            (zh->trans_no)--;
            zh->errCode = YAZ_BIB1_DATABASE_UNAVAILABLE;
            return ZEBRA_FAIL;
        }
        if (!zh->lock_normal || !zh->lock_shadow)
        {
            (zh->trans_no)--;
            zh->errCode = YAZ_BIB1_TEMPORARY_SYSTEM_ERROR;
            return ZEBRA_FAIL;
        }
        zebra_get_state (zh, &val, &seqno);
        if (val == 'd')
            val = 'o';
        
        if (!zh->reg)
            dirty = 1;
        else if (seqno != zh->reg->seqno)
        {
            yaz_log (YLOG_DEBUG, "reopen seqno cur/old %d/%d",
                     seqno, zh->reg->seqno);
            dirty = 1;
        }
        else if (zh->reg->last_val != val)
        {
            yaz_log (YLOG_DEBUG, "reopen last cur/old %d/%d",
                     val, zh->reg->last_val);
            dirty = 1;
        }
        if (!dirty)
            return ZEBRA_OK;
        
        if (val == 'c')
            zebra_lock_r (zh->lock_shadow);
        else
            zebra_lock_r (zh->lock_normal);
        
        if (zh->reg)
	{
            resultSetInvalidate (zh);
            zebra_register_close(zh->service, zh->reg);
	}
        zh->reg = zebra_register_open(zh->service, zh->reg_name,
                                      0, val == 'c' ? 1 : 0,
                                      zh->res, zh->path_reg);
        if (!zh->reg)
        {
            zebra_unlock (zh->lock_normal);
            zebra_unlock (zh->lock_shadow);
            zh->trans_no--;
            zh->errCode = YAZ_BIB1_DATABASE_UNAVAILABLE;
            return ZEBRA_FAIL;
        }
        zh->reg->last_val = val;
        zh->reg->seqno = seqno;
    }
    read_res_for_transaction(zh);
    return ZEBRA_OK;
}

ZEBRA_RES zebra_end_trans (ZebraHandle zh)
{
    ZebraTransactionStatus dummy;

    yaz_log(log_level, "zebra_end_trans");
    ZEBRA_CHECK_HANDLE(zh);
    return zebra_end_transaction(zh, &dummy);
}

ZEBRA_RES zebra_end_transaction (ZebraHandle zh, ZebraTransactionStatus *status)
{
    char val;
    int seqno;
    const char *rval;

    ZEBRA_CHECK_HANDLE(zh);

    assert(status);
    yaz_log(log_level, "zebra_end_transaction");

    status->processed = 0;
    status->inserted  = 0;
    status->updated   = 0;
    status->deleted   = 0;
    status->utime     = 0;
    status->stime     = 0;

    if (!zh->res || !zh->reg)
    {
	zebra_setError(zh, YAZ_BIB1_TEMPORARY_SYSTEM_ERROR,
		       "zebra_end_trans: no open transaction");
        return ZEBRA_FAIL;
    }
    if (zh->trans_no != zh->trans_w_no)
    {
        zh->trans_no--;
        if (zh->trans_no != 0)
            return ZEBRA_OK;

        /* release read lock */

        zebra_unlock (zh->lock_normal);
        zebra_unlock (zh->lock_shadow);
    }
    else
    {   /* release write lock */
        zh->trans_no--;
        zh->trans_w_no = 0;
        
        yaz_log (YLOG_DEBUG, "zebra_end_trans");
        rval = res_get (zh->res, "shadow");
        
        zebraExplain_runNumberIncrement (zh->reg->zei, 1);
        
        zebra_flush_reg (zh);
        
        resultSetInvalidate (zh);

        zebra_register_close(zh->service, zh->reg);
        zh->reg = 0;
        
        yaz_log (YLOG_LOG, "Records: "ZINT_FORMAT" i/u/d "
			ZINT_FORMAT"/"ZINT_FORMAT"/"ZINT_FORMAT, 
                 zh->records_processed, zh->records_inserted,
                 zh->records_updated, zh->records_deleted);
        
        status->processed = zh->records_processed;
        status->inserted = zh->records_inserted;
        status->updated = zh->records_updated;
        status->deleted = zh->records_deleted;
        
        zebra_get_state (zh, &val, &seqno);
        if (val != 'd')
        {
            BFiles bfs = bfs_create (rval, zh->path_reg);
            yaz_log (YLOG_DEBUG, "deleting shadow val=%c", val);
            bf_commitClean (bfs, rval);
            bfs_destroy (bfs);
        }
        if (!rval)
            seqno++;
        zebra_set_state (zh, 'o', seqno);
        
        zebra_unlock (zh->lock_shadow);
        zebra_unlock (zh->lock_normal);
        
    }
#if HAVE_SYS_TIMES_H
    times (&zh->tms2);
    yaz_log (log_level, "user/system: %ld/%ld",
          (long) (zh->tms2.tms_utime - zh->tms1.tms_utime),
          (long) (zh->tms2.tms_stime - zh->tms1.tms_stime));
    
    status->utime = (long) (zh->tms2.tms_utime - zh->tms1.tms_utime);
    status->stime = (long) (zh->tms2.tms_stime - zh->tms1.tms_stime);
#endif
    return ZEBRA_OK;
}

ZEBRA_RES zebra_repository_update(ZebraHandle zh, const char *path)
{
    ASSERTZH;
    assert(path);
    yaz_log (log_level, "updating %s", path);

    if (zh->m_record_id && !strcmp (zh->m_record_id, "file"))
        return zebra_update_file_match(zh, path);
    else
        return zebra_update_from_path(zh, path);
}

ZEBRA_RES zebra_repository_delete(ZebraHandle zh, const char *path)
{
    ASSERTZH;
    assert(path);
    yaz_log (log_level, "deleting %s", path);
    return zebra_delete_from_path(zh, path);
}

ZEBRA_RES zebra_repository_show(ZebraHandle zh, const char *path)
{
    ASSERTZH;
    assert(path);
    yaz_log(log_level, "zebra_repository_show");
    repositoryShow (zh, path);
    return ZEBRA_OK;
}

static ZEBRA_RES zebra_commit_ex(ZebraHandle zh, int clean_only)
{
    int seqno;
    char val;
    const char *rval;
    BFiles bfs;
    ASSERTZH;

    zebra_select_default_database(zh);
    if (!zh->res)
    {
        zh->errCode = YAZ_BIB1_DATABASE_UNAVAILABLE;
        return ZEBRA_FAIL;
    }
    rval = res_get (zh->res, "shadow");    
    if (!rval)
    {
        yaz_log (YLOG_WARN, "Cannot perform commit - No shadow area defined");
        return ZEBRA_OK;
    }

    zebra_lock_w (zh->lock_normal);
    zebra_lock_r (zh->lock_shadow);

    bfs = bfs_create (res_get (zh->res, "register"), zh->path_reg);
    if (!bfs)
    {
	zebra_unlock(zh->lock_shadow);
	zebra_unlock(zh->lock_normal);
        return ZEBRA_FAIL;
    }
    zebra_get_state (zh, &val, &seqno);

    if (rval && *rval)
        bf_cache (bfs, rval);
    if (bf_commitExists (bfs))
    {
        if (clean_only)
            zebra_set_state (zh, 'd', seqno);
        else
        {
            zebra_set_state (zh, 'c', seqno);
            
            yaz_log (YLOG_DEBUG, "commit start");
            bf_commitExec (bfs);
#ifndef WIN32
            sync ();
#endif
        }
        seqno++;
        zebra_set_state (zh, 'o', seqno);

	zebra_unlock (zh->lock_shadow);
	zebra_unlock (zh->lock_normal);

	zebra_lock_w(zh->lock_shadow);
        bf_commitClean (bfs, rval);
	zebra_unlock (zh->lock_normal);
    }
    else
    {
	zebra_unlock(zh->lock_shadow);
	zebra_unlock(zh->lock_normal);
        yaz_log (log_level, "nothing to commit");
    }
    bfs_destroy (bfs);

    return ZEBRA_OK;
}

ZEBRA_RES zebra_clean(ZebraHandle zh)
{
    yaz_log(log_level, "zebra_clean");
    ZEBRA_CHECK_HANDLE(zh);
    return zebra_commit_ex(zh, 1);
}

ZEBRA_RES zebra_commit(ZebraHandle zh)
{
    yaz_log(log_level, "zebra_commit");
    ZEBRA_CHECK_HANDLE(zh);
    return zebra_commit_ex(zh, 0);
}


ZEBRA_RES zebra_init(ZebraHandle zh)
{
    const char *rval;
    BFiles bfs = 0;

    yaz_log(log_level, "zebra_init");

    ZEBRA_CHECK_HANDLE(zh);

    zebra_select_default_database(zh);
    if (!zh->res)
    {
	zebra_setError(zh, YAZ_BIB1_TEMPORARY_SYSTEM_ERROR,
		       "cannot select default database");
	return ZEBRA_FAIL;
    }
    rval = res_get (zh->res, "shadow");

    bfs = bfs_create (res_get (zh->res, "register"), zh->path_reg);
    if (!bfs)
    {
	zebra_setError(zh, YAZ_BIB1_TEMPORARY_SYSTEM_ERROR, "bfs_create");
	return ZEBRA_FAIL;
    }
    if (rval && *rval)
        bf_cache (bfs, rval);
    
    bf_reset (bfs);
    bfs_destroy (bfs);
    zebra_set_state (zh, 'o', 0);
    return ZEBRA_OK;
}

ZEBRA_RES zebra_compact(ZebraHandle zh)
{
    BFiles bfs;

    yaz_log(log_level, "zebra_compact");
    ZEBRA_CHECK_HANDLE(zh);
    if (!zh->res)
    {
        zh->errCode = YAZ_BIB1_DATABASE_UNAVAILABLE;
        return ZEBRA_FAIL;
    }
    bfs = bfs_create (res_get (zh->res, "register"), zh->path_reg);
    inv_compact (bfs);
    bfs_destroy (bfs);
    return ZEBRA_OK;
}

void zebra_result(ZebraHandle zh, int *code, char **addinfo)
{
    yaz_log(log_level, "zebra_result");
    if (zh)
    {
	*code = zh->errCode;
	*addinfo = zh->errString;
    }
    else
    {
	*code = YAZ_BIB1_TEMPORARY_SYSTEM_ERROR;
	*addinfo ="ZebraHandle is NULL";
    }
}

void zebra_shadow_enable(ZebraHandle zh, int value)
{
    ASSERTZH;
    yaz_log(log_level, "zebra_shadow_enable");
    zh->shadow_enable = value;
}

ZEBRA_RES zebra_octet_term_encoding(ZebraHandle zh, const char *encoding)
{
    yaz_log(log_level, "zebra_octet_term_encoding %s", encoding);
    ZEBRA_CHECK_HANDLE(zh);
    assert(encoding);

    if (zh->iconv_to_utf8 != 0)
        yaz_iconv_close(zh->iconv_to_utf8);
    if (zh->iconv_from_utf8 != 0)
        yaz_iconv_close(zh->iconv_from_utf8);
    
    zh->iconv_to_utf8 =
        yaz_iconv_open ("UTF-8", encoding);
    if (zh->iconv_to_utf8 == 0)
        yaz_log (YLOG_WARN, "iconv: %s to UTF-8 unsupported", encoding);
    zh->iconv_from_utf8 =
        yaz_iconv_open (encoding, "UTF-8");
    if (zh->iconv_to_utf8 == 0)
        yaz_log (YLOG_WARN, "iconv: UTF-8 to %s unsupported", encoding);

    return ZEBRA_OK;
}

ZEBRA_RES zebra_record_encoding (ZebraHandle zh, const char *encoding)
{
    yaz_log(log_level, "zebra_record_encoding");
    ZEBRA_CHECK_HANDLE(zh);
    xfree(zh->record_encoding);
    zh->record_encoding = 0;
    if (encoding)
	zh->record_encoding = xstrdup (encoding);
    return ZEBRA_OK;
}

void zebra_set_resource(ZebraHandle zh, const char *name, const char *value)
{
    assert(name);
    assert(value);
    yaz_log(log_level, "zebra_set_resource %s:%s", name, value);
    ASSERTZH;
    res_set(zh->res, name, value);
}

const char *zebra_get_resource(ZebraHandle zh,
                               const char *name, const char *defaultvalue)
{
    const char *v;
    ASSERTZH;
    assert(name);
    v = res_get_def (zh->res, name, (char *)defaultvalue);
    yaz_log(log_level, "zebra_get_resource %s:%s", name, v);
    return v;
}

/* moved from zebra_api_ext.c by pop */
/* FIXME: Should this really be public??? -Heikki */

int zebra_trans_no (ZebraHandle zh)
{
    yaz_log(log_level, "zebra_trans_no");
    ASSERTZH;
    return zh->trans_no;
}

int zebra_get_shadow_enable (ZebraHandle zh)
{
    yaz_log(log_level, "zebra_get_shadow_enable");
    ASSERTZH;
    return zh->shadow_enable;
}

void zebra_set_shadow_enable (ZebraHandle zh, int value)
{
    yaz_log(log_level, "zebra_set_shadow_enable %d",value);
    ASSERTZH;
    zh->shadow_enable = value;
}

ZEBRA_RES zebra_add_record(ZebraHandle zh,
                           const char *buf, int buf_size)
{
    return zebra_update_record(zh, 0, 0 /* sysno */, 0, 0, buf, buf_size, 0);
}

ZEBRA_RES zebra_insert_record(ZebraHandle zh, 
			      const char *recordType,
			      SYSNO *sysno, const char *match,
			      const char *fname,
			      const char *buf, int buf_size, int force_update)
{
    ZEBRA_RES res;
    ASSERTZH;
    assert(sysno);
    assert(buf);
    yaz_log(log_level, "zebra_insert_record sysno=" ZINT_FORMAT, *sysno);

    if (buf_size < 1)
	buf_size = strlen(buf);

    if (zebra_begin_trans(zh, 1) == ZEBRA_FAIL)
	return ZEBRA_FAIL;
    res = zebra_buffer_extract_record(zh, buf, buf_size, 
                                      0, /* delete_flag  */
                                      0, /* test_mode */
                                      recordType,
                                      sysno,   
                                      match, fname,
                                      0, 
                                      0); /* allow_update */
    if (zebra_end_trans(zh) != ZEBRA_OK)
    {
	yaz_log(YLOG_WARN, "zebra_end_trans failed");
	res = ZEBRA_FAIL;
    }
    return res; 
}

ZEBRA_RES zebra_update_record (ZebraHandle zh, 
			       const char *recordType,
			       SYSNO* sysno, const char *match,
			       const char *fname,
			       const char *buf, int buf_size,
			       int force_update)
{
    ZEBRA_RES res;

    ZEBRA_CHECK_HANDLE(zh);

    assert(buf);

    yaz_log(log_level, "zebra_update_record");
    if (sysno)
	yaz_log(log_level, " sysno=" ZINT_FORMAT, *sysno);

    if (buf_size < 1) buf_size = strlen(buf);

    if (zebra_begin_trans(zh, 1) == ZEBRA_FAIL)
	return ZEBRA_FAIL;
    res = zebra_buffer_extract_record(zh, buf, buf_size, 
                                      0, /* delete_flag */
                                      0, /* test_mode */
                                      recordType,
                                      sysno,   
                                      match, fname,
                                      force_update, 
                                      1); /* allow_update */
    if (zebra_end_trans(zh) != ZEBRA_OK)
    {
	yaz_log(YLOG_WARN, "zebra_end_trans failed");
	res = ZEBRA_FAIL;
    }
    return res; 
}

ZEBRA_RES zebra_delete_record (ZebraHandle zh, 
			       const char *recordType,
			       SYSNO *sysno, const char *match,
			       const char *fname,
			       const char *buf, int buf_size,
			       int force_update) 
{
    ZEBRA_RES res;

    ZEBRA_CHECK_HANDLE(zh);

    assert(buf);
    yaz_log(log_level, "zebra_delete_record");
    if (sysno)
	yaz_log(log_level, " sysno=" ZINT_FORMAT, *sysno);

    if (buf_size < 1) buf_size = strlen(buf);

    if (zebra_begin_trans(zh, 1) == ZEBRA_FAIL)
	return ZEBRA_FAIL;
    res = zebra_buffer_extract_record(zh, buf, buf_size,
                                      1, /* delete_flag */
                                      0, /* test_mode */
                                      recordType,
                                      sysno,
                                      match,fname,
                                      force_update,
                                      1); /* allow_update */
    if (zebra_end_trans(zh) != ZEBRA_OK)
    {
	yaz_log(YLOG_WARN, "zebra_end_trans failed");
	res = ZEBRA_FAIL;
    }
    return res;
}

/* ---------------------------------------------------------------------------
  Searching 
*/

ZEBRA_RES zebra_search_PQF(ZebraHandle zh, const char *pqf_query,
			   const char *setname, zint *hits)
{
    zint lhits = 0;
    ZEBRA_RES res = ZEBRA_OK;
    Z_RPNQuery *query;
    ODR odr;

    ZEBRA_CHECK_HANDLE(zh);

    odr = odr_createmem(ODR_ENCODE);

    assert(pqf_query);
    assert(setname);

    yaz_log(log_level, "zebra_search_PQF s=%s q=%s", setname, pqf_query);
    
    query = p_query_rpn (odr, PROTO_Z3950, pqf_query);
    
    if (!query)
    {
        yaz_log (YLOG_WARN, "bad query %s\n", pqf_query);
	zh->errCode = YAZ_BIB1_MALFORMED_QUERY;
	res = ZEBRA_FAIL;
    }
    else
        res = zebra_search_RPN(zh, odr, query, setname, &lhits);
    
    odr_destroy(odr);

    yaz_log(log_level, "Hits: " ZINT_FORMAT, lhits);

    if (hits)
	*hits = lhits;

    return res;
}

/* ---------------------------------------------------------------------------
  Sort - a simplified interface, with optional read locks.
*/
int zebra_sort_by_specstr (ZebraHandle zh, ODR stream,
			   const char *sort_spec,
			   const char *output_setname,
			   const char **input_setnames) 
{
    int num_input_setnames = 0;
    int sort_status = 0;
    Z_SortKeySpecList *sort_sequence;

    ZEBRA_CHECK_HANDLE(zh);
    assert(stream);
    assert(sort_spec);
    assert(output_setname);
    assert(input_setnames);
    sort_sequence = yaz_sort_spec (stream, sort_spec);
    yaz_log(log_level, "sort (FIXME) ");
    if (!sort_sequence)
    {
        yaz_log(YLOG_WARN, "invalid sort specs '%s'", sort_spec);
        zh->errCode = YAZ_BIB1_CANNOT_SORT_ACCORDING_TO_SEQUENCE;
	return -1;
    }
    
    /* we can do this, since the perl typemap code for char** will 
       put a NULL at the end of list */
    while (input_setnames[num_input_setnames]) num_input_setnames++;

    if (zebra_begin_read (zh))
        return -1;
    
    resultSetSort (zh, stream->mem, num_input_setnames, input_setnames,
                   output_setname, sort_sequence, &sort_status);
    
    zebra_end_read(zh);
    return sort_status;
}

/* ---------------------------------------------------------------------------
  Get BFS for Zebra system (to make alternative storage methods)
*/
struct BFiles_struct *zebra_get_bfs(ZebraHandle zh)
{
    if (zh && zh->reg)
	return zh->reg->bfs;
    return 0;
}


/* ---------------------------------------------------------------------------
  Set limit for search/scan
*/
ZEBRA_RES zebra_set_limit(ZebraHandle zh, int complement_flag, zint *ids)
{
    ZEBRA_CHECK_HANDLE(zh);
    zebra_limit_destroy(zh->m_limit);
    zh->m_limit = zebra_limit_create(complement_flag, ids);
    return ZEBRA_OK;
}

/*
  Set Error code + addinfo
*/
void zebra_setError(ZebraHandle zh, int code, const char *addinfo)
{
    if (!zh)
	return;
    zh->errCode = code;
    nmem_reset(zh->nmem_error);
    zh->errString = addinfo ? nmem_strdup(zh->nmem_error, addinfo) : 0;
}

void zebra_setError_zint(ZebraHandle zh, int code, zint i)
{
    char vstr[60];
    sprintf(vstr, ZINT_FORMAT, i);

    zh->errCode = code;
    nmem_reset(zh->nmem_error);
    zh->errString = nmem_strdup(zh->nmem_error, vstr);
}

void zebra_lock_prefix (Res res, char *path)
{
    const char *lock_dir = res_get_def (res, "lockDir", "");

    strcpy (path, lock_dir);
    if (*path && path[strlen(path)-1] != '/')
        strcat (path, "/");
}

/*
 * Local variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */

