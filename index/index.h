/*
 * Copyright (C) 1994-1995, Index Data I/S 
 * All rights reserved.
 * Sebastian Hammer, Adam Dickmeiss
 *
 * $Log: index.h,v $
 * Revision 1.26  1995-11-22 17:19:17  adam
 * Record management uses the bfile system.
 *
 * Revision 1.25  1995/11/21  15:29:12  adam
 * Config file 'base' read by default by both indexer and server.
 *
 * Revision 1.24  1995/11/21  15:01:15  adam
 * New general match criteria implemented.
 * New feature: document groups.
 *
 * Revision 1.23  1995/11/20  16:59:45  adam
 * New update method: the 'old' keys are saved for each records.
 *
 * Revision 1.22  1995/11/20  11:56:26  adam
 * Work on new traversal.
 *
 * Revision 1.21  1995/11/16  15:34:55  adam
 * Uses new record management system in both indexer and server.
 *
 * Revision 1.20  1995/11/15  14:46:18  adam
 * Started work on better record management system.
 *
 * Revision 1.19  1995/10/27  14:00:11  adam
 * Implemented detection of database availability.
 *
 * Revision 1.18  1995/10/17  18:02:08  adam
 * New feature: databases. Implemented as prefix to words in dictionary.
 *
 * Revision 1.17  1995/10/13  16:01:49  adam
 * Work on relations.
 *
 * Revision 1.16  1995/10/10  12:24:38  adam
 * Temporary sort files are compressed.
 *
 * Revision 1.15  1995/10/04  16:57:19  adam
 * Key input and merge sort in one pass.
 *
 * Revision 1.14  1995/09/29  14:01:40  adam
 * Bug fixes.
 *
 * Revision 1.13  1995/09/28  14:22:56  adam
 * Sort uses smaller temporary files.
 *
 * Revision 1.12  1995/09/28  12:10:32  adam
 * Bug fixes. Field prefix used in queries.
 *
 * Revision 1.11  1995/09/27  12:22:28  adam
 * More work on extract in record control.
 * Field name is not in isam keys but in prefix in dictionary words.
 *
 * Revision 1.10  1995/09/14  07:48:23  adam
 * Record control management.
 *
 * Revision 1.9  1995/09/11  13:09:33  adam
 * More work on relevance feedback.
 *
 * Revision 1.8  1995/09/08  14:52:27  adam
 * Minor changes. Dictionary is lower case now.
 *
 * Revision 1.7  1995/09/06  16:11:16  adam
 * Option: only one word key per file.
 *
 * Revision 1.6  1995/09/05  15:28:39  adam
 * More work on search engine.
 *
 * Revision 1.5  1995/09/04  12:33:42  adam
 * Various cleanup. YAZ util used instead.
 *
 * Revision 1.4  1995/09/04  09:10:35  adam
 * More work on index add/del/update.
 * Merge sort implemented.
 * Initial work on z39 server.
 *
 * Revision 1.3  1995/09/01  14:06:35  adam
 * Split of work into more files.
 *
 * Revision 1.2  1995/09/01  10:30:24  adam
 * More work on indexing. Not working yet.
 *
 * Revision 1.1  1995/08/31  14:50:24  adam
 * New simple file index tool.
 *
 */

#include <alexutil.h>
#include <dict.h>
#include <isam.h>

#define IT_MAX_WORD 256
#define IT_KEY_HAVE_SEQNO 1
#define IT_KEY_HAVE_FIELD 0

struct it_key {
    int  sysno;
    int   seqno;
};

enum dirsKind { dirs_dir, dirs_file };

struct dir_entry {
    enum dirsKind kind;
    char *name;
    int ctime;
};

struct dirs_entry {
    enum dirsKind kind;
    char path[256];
    SYSNO sysno;
    int ctime;
};

struct recordGroup {
    char *groupName;
    char *databaseName;
    char *path;
};

struct dirs_info *dirs_open (Dict dict, const char *rep);
struct dirs_entry *dirs_read (struct dirs_info *p);
struct dirs_entry *dirs_last (struct dirs_info *p);
void dirs_mkdir (struct dirs_info *p, const char *src, int ctime);
void dirs_rmdir (struct dirs_info *p, const char *src);
void dirs_add (struct dirs_info *p, const char *src, int sysno, int ctime);
void dirs_del (struct dirs_info *p, const char *src);
void dirs_free (struct dirs_info **pp);

struct dir_entry *dir_open (const char *rep);
void dir_sort (struct dir_entry *e);
void dir_free (struct dir_entry **e_p);

void repositoryUpdate (struct recordGroup *rGroup);
void repositoryAdd (struct recordGroup *rGroup);
void repositoryDelete (struct recordGroup *rGroup);

void key_open (int mem);
int key_close (void);
void key_write (int cmd, struct it_key *k, const char *str);
int key_compare (const void *p1, const void *p2);
int key_qsort_compare (const void *p1, const void *p2);
void key_logdump (int mask, const void *p);
void key_input (const char *dict_fname, const char *isam_fname,
                 int nkeys, int cache);
int merge_sort (char **buf, int from, int to);

#define TEMP_FNAME  "keys%d.tmp"
#define FNAME_WORD_DICT "worddict"
#define FNAME_WORD_ISAM "wordisam"

struct strtab *strtab_mk (void);
int strtab_src (struct strtab *t, const char *name, void ***infop);
void strtab_del (struct strtab *t,
                 void (*func)(const char *name, void *info, void *data),
                 void *data);
int index_char_cvt (int c);
int index_word_prefix (char *string, int attset_ordinal,
                       int local_attribute, const char *databaseName);

int fileExtract (SYSNO *sysno, const char *fname,
                 struct recordGroup *rGroup, int deleteFlag);
