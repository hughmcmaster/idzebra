/* $Id: index.h,v 1.191 2007-01-16 15:01:15 adam Exp $
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

#ifndef ZEBRA_INDEX_H
#define ZEBRA_INDEX_H

#include <time.h>
#include <stdlib.h>
#include <idzebra/version.h>
#include <idzebra/util.h>
#include <idzebra/flock.h>
#include <sortidx.h>

#if HAVE_SYS_TIMES_H
#include <sys/times.h>
#endif
#include <sys/stat.h>

#include <yaz/timing.h>
#include <idzebra/dict.h>
#include <idzebra/isams.h>
#include <idzebra/isamc.h>
#include <idzebra/isamb.h>
#include <d1_absyn.h>
#include <idzebra/recgrs.h>
#include "recindex.h"
#include <idzebra/api.h>
#include "zinfo.h"
#include <passwddb.h>
#include <rset.h>
#include <zebramap.h>

#include <it_key.h>
#include <su_codec.h>

YAZ_BEGIN_CDECL

#define ISAM_DEFAULT "b"

enum dirsKind { dirs_dir, dirs_file };

struct dir_entry {
    enum dirsKind kind;
    char *name;
    time_t mtime;
};

struct dirs_entry {
    enum dirsKind kind;
    char path[256];
    zint sysno;
    time_t mtime;
};

void getFnameTmp(Res res, char *fname, int no);
        
struct dirs_info *dirs_open(Dict dict, const char *rep, int rw);
struct dirs_info *dirs_fopen(Dict dict, const char *path, int rw);
struct dirs_entry *dirs_read(struct dirs_info *p);
struct dirs_entry *dirs_last(struct dirs_info *p);
void dirs_mkdir(struct dirs_info *p, const char *src, time_t mtime);
void dirs_rmdir(struct dirs_info *p, const char *src);
void dirs_add(struct dirs_info *p, const char *src, zint sysno, time_t mtime);
void dirs_del(struct dirs_info *p, const char *src);
void dirs_free(struct dirs_info **pp);

struct dir_entry *dir_open(const char *rep, const char *base,
                           int follow_links);
void dir_sort(struct dir_entry *e);
void dir_free(struct dir_entry **e_p);

void repositoryUpdate(ZebraHandle zh, const char *path);
void repositoryAdd(ZebraHandle zh, const char *path);
void repositoryDelete(ZebraHandle zh, const char *path);
void repositoryShow(ZebraHandle zh, const char *path);

void inv_prstat(ZebraHandle zh);
void inv_compact(BFiles bfs);
void key_input(ZebraHandle zh, int nkeys, int cache, Res res);
ISAMS_M *key_isams_m(Res res, ISAMS_M *me);
ISAMC_M *key_isamc_m(Res res, ISAMC_M *me);

#define FNAME_DICT "dict"
#define FNAME_ISAM "isam"
#define FNAME_ISAMC "isamc"
#define FNAME_ISAMS "isams"
#define FNAME_CONFIG "zebra.cfg"

#define GMATCH_DICT "gmatch"
#define FMATCH_DICT "fmatch%d"

void zebra_lock_prefix(Res res, char *dst);

#define FNAME_MAIN_LOCK   "zebraidx.LCK"
#define FNAME_COMMIT_LOCK "zebracmt.LCK"
#define FNAME_ORG_LOCK    "zebraorg.LCK"
#define FNAME_TOUCH_TIME  "zebraidx.time"

typedef struct zebra_set *ZebraSet;

typedef struct zebra_rank_class {
    struct rank_control *control;
    int init_flag;
    void *class_handle;
    struct zebra_rank_class *next;
} *ZebraRankClass;

#include "reckeys.h"
#include "key_block.h"

struct zebra_register {
    char *name;
    
    ISAMS isams;
    ISAMC isamc;
    ISAMB isamb;
    Dict dict;
    Dict matchDict;
    zebra_sort_index_t sort_index;
    int registerState; /* 0 (no commit pages), 1 (use commit pages) */
    time_t registerChange;
    BFiles bfs;
    Records records;
    ZebraExplainInfo zei;

    char *server_path_prefix;
    data1_handle dh;
    ZebraMaps zebra_maps;
    ZebraRankClass rank_classes;
    RecTypes recTypes;
    int seqno;
    int last_val;
    int stop_flag;

    zebra_rec_keys_t keys;
    zebra_rec_keys_t sortKeys;

    zebra_key_block_t key_block;
};

struct zebra_service {
    int stop_flag;
    Res global_res;
    struct zebra_session *sessions;
    struct zebra_register *regs;
    Zebra_mutex_cond session_lock;
    Passwd_db passwd_db;
    Res dbaccess;
    const char *path_root;
    RecTypeClass record_classes;
    NMEM nmem;
    yaz_timing_t timing;
};


struct zebra_session {
    struct zebra_session *next;
    struct zebra_service *service;
    struct zebra_register *reg;

    char *xadmin_databaseName;

    char **basenames;
    int num_basenames;
    zint approx_limit;
    char *reg_name;
    char *path_reg;

    ZebraLockHandle lock_normal;
    ZebraLockHandle lock_shadow;

    int trans_no;
    int trans_w_no;

    int destroyed;
    ZebraSet sets;
    Res res;
    Res session_res;
    char *user_perm;
    char *dbaccesslist;
    int errCode;
    char *errString;
#if HAVE_SYS_TIMES_H
    struct tms tms1;
    struct tms tms2;    
#endif
    int  shadow_enable;

    int m_staticrank;
    int m_segment_indexing;

    zint records_inserted;
    zint records_updated;
    zint records_deleted;
    zint records_processed;
    char *record_encoding;

    yaz_iconv_t iconv_to_utf8;
    yaz_iconv_t iconv_from_utf8;

    int m_follow_links;
    const char *m_group;
    const char *m_record_id;
    const char *m_record_type;
    int m_store_data;
    int m_store_keys;
    int m_explain_database;
    int m_flag_rw;
    int m_file_verbose_limit;

    void *store_data_buf;
    size_t store_data_size;
    NMEM nmem_error;

    struct zebra_limit *m_limit;
};


struct term_set_entry {
    char *term;
    struct term_set_entry *next;
};

struct term_set_list {
    struct term_set_entry *first;
    struct term_set_entry *last;
};


void zebra_limit_destroy(struct zebra_limit *zl);
struct zebra_limit *zebra_limit_create(int exclude_flag, zint *ids);
void zebra_limit_for_rset(struct zebra_limit *zl,
			  int (**filter_func)(const void *buf, void *data),
			  void (**filter_destroy)(void *data),
			  void **filter_data);

struct rset_key_control *zebra_key_control_create(ZebraHandle zh);

ZEBRA_RES rpn_search_top(ZebraHandle zh, Z_RPNStructure *zs,
			 oid_value attributeSet, 
			 NMEM stream, NMEM rset_nmem,
			 Z_SortKeySpecList *sort_sequence,
			 int num_bases, char **basenames,
			 RSET *result_set);

ZEBRA_RES rpn_get_top_approx_limit(ZebraHandle zh, Z_RPNStructure *zs,
                                   zint *approx_limit);

ZEBRA_RES rpn_scan(ZebraHandle zh, ODR stream, Z_AttributesPlusTerm *zapt,
                   oid_value attributeset,
                   int num_bases, char **basenames,
                   int *position, int *num_entries, ZebraScanEntry **list,
                   int *is_partial, RSET limit_set);

RSET rset_trunc(ZebraHandle zh, ISAM_P *isam_p, int no,
		const char *term, int length_term, const char *flags,
		int preserve_position, int term_type, NMEM rset_nmem,
		struct rset_key_control *kctrl, int scope,
		struct ord_list *ol, int reg_type,
		zint hits_limit, const char *term_ref_id);

void resultSetAddTerm(ZebraHandle zh, ZebraSet s, int reg_type,
                      const char *db, const char *index_name,
                      const char *term);
ZebraSet resultSetAdd(ZebraHandle zh, const char *name, int ov);
ZebraSet resultSetGet(ZebraHandle zh, const char *name);
ZEBRA_RES resultSetAddRPN(ZebraHandle zh, NMEM m, Z_RPNQuery *rpn,
                          int num_bases, char **basenames,
                          const char *setname,
                          zint *hits, int *estimated_hit_count,
                          int *partial_resultset);
RSET resultSetRef(ZebraHandle zh, const char *resultSetId);
void resultSetDestroy(ZebraHandle zh, int num_names, char **names,
		       int *statuses);
ZEBRA_RES resultSetSort(ZebraHandle zh, NMEM nmem,
			 int num_input_setnames, const char **input_setnames,
			 const char *output_setname,
			 Z_SortKeySpecList *sort_sequence, int *sort_status);
ZEBRA_RES resultSetSortSingle(ZebraHandle zh, NMEM nmem,
			       ZebraSet sset, RSET rset,
			       Z_SortKeySpecList *sort_sequence,
			       int *sort_status);
ZEBRA_RES resultSetRank(ZebraHandle zh, ZebraSet zebraSet, RSET rset,
			 NMEM nmem);
void resultSetInvalidate(ZebraHandle zh);

int zebra_record_fetch(ZebraHandle zh, zint sysno, int score, 
			zebra_snippets *hit_snippet, ODR stream,
			oid_value input_format, Z_RecordComposition *comp,
			oid_value *output_format, char **rec_bufp,
			int *rec_lenp, char **basenamep,
			char **addinfo);

void extract_get_fname_tmp(ZebraHandle zh, char *fname, int no);

void zebra_index_merge(ZebraHandle zh);

ZEBRA_RES zebra_buffer_extract_record(ZebraHandle zh, 
                                      const char *buf, size_t buf_size,
                                      int delete_flag,
                                      int test_mode, 
                                      const char *recordType,
                                      zint *sysno,
                                      const char *match_criteria,
                                      const char *fname,
                                      int force_update,
                                      int allow_update);

ZEBRA_RES zebra_extract_record_stream(ZebraHandle zh, 
                                      struct ZebraRecStream *stream,
                                      int delete_flag,
                                      int test_mode, 
                                      const char *recordType,
                                      zint *sysno,
                                      const char *match_criteria,
                                      const char *fname,
                                      int force_update,
                                      int allow_update,
                                      RecType recType,
                                      void *recTypeClientData,
                                      int *more);

YAZ_EXPORT void zebra_create_stream_mem(struct ZebraRecStream *stream,
                                        const char *buf, size_t sz);
YAZ_EXPORT void zebra_create_stream_fd(struct ZebraRecStream *stream,
                                       int fd, off_t start_offset);
void print_rec_keys(ZebraHandle zh, zebra_rec_keys_t reckeys);

ZEBRA_RES zebra_rec_keys_to_snippets(ZebraHandle zh, zebra_rec_keys_t reckeys,
                                     zebra_snippets *snippets);
ZEBRA_RES zebra_snippets_hit_vector(ZebraHandle zh, const char *setname,
				    zint sysno, zebra_snippets *snippets);

ZEBRA_RES zebra_extract_explain(void *handle, Record rec, data1_node *n);

ZEBRA_RES zebra_extract_file(ZebraHandle zh, zint *sysno, const char *fname,
			     int deleteFlag);

ZEBRA_RES zebra_begin_read(ZebraHandle zh);
ZEBRA_RES zebra_end_read(ZebraHandle zh);

int zebra_file_stat(const char *file_name, struct stat *buf,
                     int follow_links);

Dict dict_open_res(BFiles bfs, const char *name, int cache, int rw,
                   int compact_flag, Res res);

void zebra_setError(ZebraHandle zh, int code, const char *addinfo);
void zebra_setError_zint(ZebraHandle zh, int code, zint i);

void zebra_term_untrans_iconv(ZebraHandle zh, NMEM stream, int reg_type,
			      char **dst, const char *src);

ZEBRA_RES zebra_get_hit_vector(ZebraHandle zh, const char *setname, zint sysno);

void zebra_term_untrans(ZebraHandle zh, int reg_type,
			char *dst, const char *src);

ZEBRA_RES zebra_apt_get_ord(ZebraHandle zh,
                            Z_AttributesPlusTerm *zapt,
                            int index_type,
                            const char *xpath_use,
                            oid_value curAttributeSet,
                            int *ord);

ZEBRA_RES zebra_attr_list_get_ord(ZebraHandle zh,
                                  Z_AttributeList *attr_list,
                                  zinfo_index_category_t cat,
                                  int index_type,
                                  oid_value curAttributeSet,
                                  int *ord);

ZEBRA_RES zebra_sort_get_ord(ZebraHandle zh,
                             Z_SortAttributes *sortAttributes,
                             int *ord,
                             int *numerical);

ZEBRA_RES zebra_update_file_match(ZebraHandle zh, const char *path);
ZEBRA_RES zebra_update_from_path(ZebraHandle zh, const char *path);
ZEBRA_RES zebra_delete_from_path(ZebraHandle zh, const char *path);
ZEBRA_RES zebra_remove_file_match(ZebraHandle zh);

struct rpn_char_map_info
{
    ZebraMaps zm;
    int reg_type;
};

void rpn_char_map_prepare(struct zebra_register *reg, int reg_type,
                          struct rpn_char_map_info *map_info);

ZEBRA_RES zapt_term_to_utf8(ZebraHandle zh, Z_AttributesPlusTerm *zapt,
			    char *termz);


int zebra_check_res(Res res);

#define FIRST_IN_FIELD_STR "\001^"
#define FIRST_IN_FIELD_CHAR 1
#define FIRST_IN_FIELD_LEN 2

YAZ_END_CDECL

#endif
/*
 * Local variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */

