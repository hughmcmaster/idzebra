/* $Id: extract.c,v 1.255 2007-04-07 22:26:27 adam Exp $
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

#include <stdio.h>
#include <assert.h>
#include <ctype.h>
#ifdef WIN32
#include <io.h>
#endif
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <fcntl.h>

#include "index.h"
#include "orddict.h"
#include <direntz.h>
#include <charmap.h>

static int log_level_extract = 0;
static int log_level_details = 0;
static int log_level_initialized = 0;

/* 1 if we use eliminitate identical delete/insert keys */
/* eventually this the 0-case code will be removed */
#define FLUSH2 1

void extract_flush_record_keys2(ZebraHandle zh, zint sysno,
                                zebra_rec_keys_t ins_keys,
                                zint ins_rank,
                                zebra_rec_keys_t del_keys,
                                zint del_rank);

static void zebra_init_log_level(void)
{
    if (!log_level_initialized)
    {
        log_level_initialized = 1;

        log_level_extract = yaz_log_module_level("extract");
        log_level_details = yaz_log_module_level("indexdetails");
    }
}

static void extract_flush_record_keys(ZebraHandle zh, zint sysno,
                                      int cmd, zebra_rec_keys_t reckeys,
                                      zint staticrank);
static void extract_flush_sort_keys(ZebraHandle zh, zint sysno,
                                    int cmd, zebra_rec_keys_t skp);
static void extract_schema_add (struct recExtractCtrl *p, Odr_oid *oid);
static void extract_token_add (RecWord *p);

static void check_log_limit(ZebraHandle zh)
{
    if (zh->records_processed + zh->records_skipped == zh->m_file_verbose_limit)
    {
        yaz_log(YLOG_LOG, "More than %d file log entries. Omitting rest",
                zh->m_file_verbose_limit);
    }
}

static void logRecord (ZebraHandle zh)
{
    check_log_limit(zh);
    ++zh->records_processed;
    if (!(zh->records_processed % 1000))
    {
        yaz_log(YLOG_LOG, "Records: "ZINT_FORMAT" i/u/d "
                ZINT_FORMAT"/"ZINT_FORMAT"/"ZINT_FORMAT, 
                zh->records_processed, zh->records_inserted, 
                zh->records_updated, zh->records_deleted);
    }
}

static void extract_add_index_string (RecWord *p, 
                                      zinfo_index_category_t cat,
                                      const char *str, int length);

static void extract_set_store_data_prepare(struct recExtractCtrl *p);

static void extract_init(struct recExtractCtrl *p, RecWord *w)
{
    w->seqno = 1;
    w->index_name = "any";
    w->index_type = 'w';
    w->extractCtrl = p;
    w->record_id = 0;
    w->section_id = 0;
    w->segment = 0;
}

static void searchRecordKey(ZebraHandle zh,
			    zebra_rec_keys_t reckeys,
                            const char *index_name,
			    const char **ws, int ws_length)
{
    int i;
    int ch = -1;
    zinfo_index_category_t cat = zinfo_index_category_index;

    for (i = 0; i<ws_length; i++)
        ws[i] = NULL;

    if (ch < 0)
        ch = zebraExplain_lookup_attr_str(zh->reg->zei, cat, '0', index_name);
    if (ch < 0)
        ch = zebraExplain_lookup_attr_str(zh->reg->zei, cat, 'p', index_name);
    if (ch < 0)
        ch = zebraExplain_lookup_attr_str(zh->reg->zei, cat, 'w', index_name);

    if (ch < 0)
	return ;

    if (zebra_rec_keys_rewind(reckeys))
    {
	zint startSeq = -1;
	const char *str;
	size_t slen;
	struct it_key key;
	zint seqno;
	while (zebra_rec_keys_read(reckeys, &str, &slen, &key))
	{
	    assert(key.len <= IT_KEY_LEVEL_MAX && key.len > 2);

	    seqno = key.mem[key.len-1];
	    
	    if (key.mem[0] == ch)
	    {
		zint woff;
		
		if (startSeq == -1)
		    startSeq = seqno;
		woff = seqno - startSeq;
		if (woff >= 0 && woff < ws_length)
		    ws[woff] = str;
	    }
	}
    }
}

#define FILE_MATCH_BLANK "\t "

static char *get_match_from_spec(ZebraHandle zh,
                          zebra_rec_keys_t reckeys,
                          const char *fname, const char *spec)
{
    static char dstBuf[2048];      /* static here ??? */
    char *dst = dstBuf;
    const char *s = spec;

    while (1)
    {
	for (; *s && strchr(FILE_MATCH_BLANK, *s); s++)
	    ;
        if (!*s)
            break;
        if (*s == '(')
        {
	    const char *ws[32];
	    char attset_str[64], attname_str[64];
	    int i;
            int first = 1;
	    
	    for (s++; strchr(FILE_MATCH_BLANK, *s); s++)
		;
	    for (i = 0; *s && *s != ',' && *s != ')' && 
		     !strchr(FILE_MATCH_BLANK, *s); s++)
		if (i+1 < sizeof(attset_str))
		    attset_str[i++] = *s;
	    attset_str[i] = '\0';
	    
	    for (; strchr(FILE_MATCH_BLANK, *s); s++)
		;
	    if (*s != ',')
                strcpy(attname_str, attset_str);
            else
	    {
		for (s++; strchr(FILE_MATCH_BLANK, *s); s++)
		    ;
		for (i = 0; *s && *s != ')' && 
			 !strchr(FILE_MATCH_BLANK, *s); s++)
		    if (i+1 < sizeof(attname_str))
			attname_str[i++] = *s;
		attname_str[i] = '\0';
	    }

            searchRecordKey (zh, reckeys, attname_str, ws, 32);

            if (*s != ')')
            {
                yaz_log (YLOG_WARN, "Missing ) in match criteria %s in group %s",
                      spec, zh->m_group ? zh->m_group : "none");
                return NULL;
            }
            s++;

            for (i = 0; i<32; i++)
                if (ws[i])
                {
                    if (first)
                    {
                        *dst++ = ' ';
                        first = 0;
                    }
                    strcpy (dst, ws[i]);
                    dst += strlen(ws[i]);
                }
            if (first)
            {
                yaz_log (YLOG_WARN, "Record didn't contain match"
                      " fields in (%s,%s)", attset_str, attname_str);
                return NULL;
            }
        }
        else if (*s == '$')
        {
            int spec_len;
            char special[64];
            const char *spec_src = NULL;
            const char *s1 = ++s;
            while (*s1 && !strchr(FILE_MATCH_BLANK, *s1))
                s1++;

            spec_len = s1 - s;
            if (spec_len > sizeof(special)-1)
                spec_len = sizeof(special)-1;
            memcpy (special, s, spec_len);
            special[spec_len] = '\0';
            s = s1;

            if (!strcmp (special, "group"))
                spec_src = zh->m_group;
            else if (!strcmp (special, "database"))
                spec_src = zh->basenames[0];
            else if (!strcmp (special, "filename")) {
                spec_src = fname;
	    }
            else if (!strcmp (special, "type"))
                spec_src = zh->m_record_type;
            else 
                spec_src = NULL;
            if (spec_src)
            {
                strcpy (dst, spec_src);
                dst += strlen (spec_src);
            }
        }
        else if (*s == '\"' || *s == '\'')
        {
            int stopMarker = *s++;
            char tmpString[64];
            int i = 0;

            while (*s && *s != stopMarker)
            {
                if (i+1 < sizeof(tmpString))
                    tmpString[i++] = *s++;
            }
            if (*s)
                s++;
            tmpString[i] = '\0';
            strcpy (dst, tmpString);
            dst += strlen (tmpString);
        }
        else
        {
            yaz_log (YLOG_WARN, "Syntax error in match criteria %s in group %s",
                  spec, zh->m_group ? zh->m_group : "none");
            return NULL;
        }
        *dst++ = 1;
    }
    if (dst == dstBuf)
    {
        yaz_log (YLOG_WARN, "No match criteria for record %s in group %s",
              fname, zh->m_group ? zh->m_group : "none");
        return NULL;
    }
    *dst = '\0';
    return dstBuf;
}

struct recordLogInfo {
    const char *fname;
    int recordOffset;
    struct recordGroup *rGroup;
};

static void init_extractCtrl(ZebraHandle zh, struct recExtractCtrl *ctrl)
{
    int i;
    for (i = 0; i<256; i++)
    {
	if (zebra_maps_is_positioned(zh->reg->zebra_maps, i))
	    ctrl->seqno[i] = 1;
	else
	    ctrl->seqno[i] = 0;
    }
    ctrl->flagShowRecords = !zh->m_flag_rw;
}

static void all_matches_add(struct recExtractCtrl *ctrl)
{
    RecWord word;
    extract_init(ctrl, &word);
    word.index_name = "_ALLRECORDS";
    word.index_type = 'w';
    word.seqno = 1;
    extract_add_index_string (&word, zinfo_index_category_alwaysmatches,
                              "", 0);
}

ZEBRA_RES zebra_extract_records_stream(ZebraHandle zh, 
                                       struct ZebraRecStream *stream,
                                       enum zebra_recctrl_action_t action,
                                       int test_mode, 
                                       const char *recordType,
                                       zint *sysno,
                                       const char *match_criteria,
                                       const char *fname,
                                       RecType recType,
                                       void *recTypeClientData);


ZEBRA_RES zebra_extract_file(ZebraHandle zh, zint *sysno, const char *fname, 
			     int deleteFlag)
{
    ZEBRA_RES r = ZEBRA_OK;
    int i, fd;
    char gprefix[128];
    char ext[128];
    char ext_res[128];
    struct file_read_info *fi = 0;
    const char *original_record_type = 0;
    RecType recType;
    void *recTypeClientData;
    struct ZebraRecStream stream, *streamp;

    zebra_init_log_level();

    if (!zh->m_group || !*zh->m_group)
        *gprefix = '\0';
    else
        sprintf (gprefix, "%s.", zh->m_group);
    
    yaz_log(log_level_extract, "zebra_extract_file %s", fname);

    /* determine file extension */
    *ext = '\0';
    for (i = strlen(fname); --i >= 0; )
        if (fname[i] == '/')
            break;
        else if (fname[i] == '.')
        {
            strcpy (ext, fname+i+1);
            break;
        }
    /* determine file type - depending on extension */
    original_record_type = zh->m_record_type;
    if (!zh->m_record_type)
    {
        sprintf (ext_res, "%srecordType.%s", gprefix, ext);
        zh->m_record_type = res_get (zh->res, ext_res);
    }
    if (!zh->m_record_type)
    {
        check_log_limit(zh);
	if (zh->records_processed + zh->records_skipped
            < zh->m_file_verbose_limit)
            yaz_log (YLOG_LOG, "? %s", fname);
        zh->records_skipped++;
        return 0;
    }
    /* determine match criteria */
    if (!zh->m_record_id)
    {
        sprintf (ext_res, "%srecordId.%s", gprefix, ext);
        zh->m_record_id = res_get (zh->res, ext_res);
    }

    if (!(recType =
	  recType_byName (zh->reg->recTypes, zh->res, zh->m_record_type,
			  &recTypeClientData)))
    {
        yaz_log(YLOG_WARN, "No such record type: %s", zh->m_record_type);
        return ZEBRA_FAIL;
    }

    switch(recType->version)
    {
    case 0:
	break;
    default:
	yaz_log(YLOG_WARN, "Bad filter version: %s", zh->m_record_type);
    }
    if (sysno && deleteFlag)
    {
        streamp = 0;
        fi = 0;
    }
    else
    {
        char full_rep[1024];

        if (zh->path_reg && !yaz_is_abspath (fname))
        {
            strcpy (full_rep, zh->path_reg);
            strcat (full_rep, "/");
            strcat (full_rep, fname);
        }
        else
            strcpy (full_rep, fname);
        
        if ((fd = open (full_rep, O_BINARY|O_RDONLY)) == -1)
        {
            yaz_log (YLOG_WARN|YLOG_ERRNO, "open %s", full_rep);
	    zh->m_record_type = original_record_type;
            return ZEBRA_FAIL;
        }
        streamp = &stream;
        zebra_create_stream_fd(streamp, fd, 0);
    }
    r = zebra_extract_records_stream(zh, streamp,
                                     deleteFlag ? 
                                     action_delete : action_update,
                                     0, /* tst_mode */
                                     zh->m_record_type,
                                     sysno,
                                     0, /*match_criteria */
                                     fname,
                                     recType, recTypeClientData);
    if (streamp)
        stream.destroy(streamp);
    zh->m_record_type = original_record_type;
    return r;
}

/*
  If sysno is provided, then it's used to identify the reocord.
  If not, and match_criteria is provided, then sysno is guessed
  If not, and a record is provided, then sysno is got from there
  
 */

ZEBRA_RES zebra_buffer_extract_record(ZebraHandle zh, 
                                      const char *buf, size_t buf_size,
                                      enum zebra_recctrl_action_t action,
                                      int test_mode, 
                                      const char *recordType,
                                      zint *sysno,
                                      const char *match_criteria,
                                      const char *fname)
{
    struct ZebraRecStream stream;
    ZEBRA_RES res;
    void *clientData;
    RecType recType = 0;

    if (recordType && *recordType)
    {
        yaz_log(log_level_extract,
                "Record type explicitly specified: %s", recordType);
        recType = recType_byName (zh->reg->recTypes, zh->res, recordType,
                                  &clientData);
    } 
    else
    {
        if (!(zh->m_record_type))
	{
            yaz_log (YLOG_WARN, "No such record type defined");
            return ZEBRA_FAIL;
        }
        yaz_log(log_level_extract, "Get record type from rgroup: %s",
                zh->m_record_type);
        recType = recType_byName (zh->reg->recTypes, zh->res,
				  zh->m_record_type, &clientData);
        recordType = zh->m_record_type;
    }
    
    if (!recType)
    {
        yaz_log (YLOG_WARN, "No such record type: %s", recordType);
        return ZEBRA_FAIL;
    }

    zebra_create_stream_mem(&stream, buf, buf_size);

    res = zebra_extract_records_stream(zh, &stream,
                                       action,
                                       test_mode, 
                                       recordType,
                                       sysno,
                                       match_criteria,
                                       fname,
                                       recType, clientData);
    stream.destroy(&stream);
    return res;
}

ZEBRA_RES zebra_extract_records_stream(ZebraHandle zh, 
                                       struct ZebraRecStream *stream,
                                       enum zebra_recctrl_action_t action,
                                       int test_mode, 
                                       const char *recordType,
                                       zint *sysno,
                                       const char *match_criteria,
                                       const char *fname,
                                       RecType recType,
                                       void *recTypeClientData)
{
    ZEBRA_RES res = ZEBRA_OK;
    while (1)
    {
        int more = 0;
        res = zebra_extract_record_stream(zh, stream,
                                          action,
                                          test_mode, 
                                          recordType,
                                          sysno,
                                          match_criteria,
                                          fname,
                                          recType, recTypeClientData, &more);
        if (!more)
        {
            res = ZEBRA_OK;
            break;
        }
        if (res != ZEBRA_OK)
            break;
        if (sysno)
            break;
    }
    return res;
}


ZEBRA_RES zebra_extract_record_stream(ZebraHandle zh, 
                                      struct ZebraRecStream *stream,
                                      enum zebra_recctrl_action_t action,
                                      int test_mode, 
                                      const char *recordType,
                                      zint *sysno,
                                      const char *match_criteria,
                                      const char *fname,
                                      RecType recType,
                                      void *recTypeClientData,
                                      int *more)

{
    zint sysno0 = 0;
    RecordAttr *recordAttr;
    struct recExtractCtrl extractCtrl;
    int r;
    const char *matchStr = 0;
    Record rec;
    off_t start_offset = 0, end_offset = 0;
    const char *pr_fname = fname;  /* filename to print .. */
    int show_progress = zh->records_processed + zh->records_skipped 
        < zh->m_file_verbose_limit ? 1:0;

    zebra_init_log_level();

    if (!pr_fname)
	pr_fname = "<no file>";  /* make it printable if file is omitted */

    zebra_rec_keys_reset(zh->reg->keys);
    zebra_rec_keys_reset(zh->reg->sortKeys);

    if (zebraExplain_curDatabase (zh->reg->zei, zh->basenames[0]))
    {
        if (zebraExplain_newDatabase (zh->reg->zei, zh->basenames[0], 
				      zh->m_explain_database))
            return ZEBRA_FAIL;
    }

    if (stream)
    {
        off_t null_offset = 0;
        extractCtrl.stream = stream;

        start_offset = stream->tellf(stream);

        extractCtrl.first_record = start_offset ? 0 : 1;
        
        stream->endf(stream, &null_offset);;

        extractCtrl.init = extract_init;
        extractCtrl.tokenAdd = extract_token_add;
        extractCtrl.schemaAdd = extract_schema_add;
        extractCtrl.dh = zh->reg->dh;
        extractCtrl.handle = zh;
        extractCtrl.match_criteria[0] = '\0';
        extractCtrl.staticrank = 0;
        extractCtrl.action = action;

        init_extractCtrl(zh, &extractCtrl);

        extract_set_store_data_prepare(&extractCtrl);
        
        r = (*recType->extract)(recTypeClientData, &extractCtrl);

        if (action == action_update)
        {
            action = extractCtrl.action;
        }
        
        switch (r)
        {
        case RECCTRL_EXTRACT_EOF:
            return ZEBRA_FAIL;
        case RECCTRL_EXTRACT_ERROR_GENERIC:
            /* error occured during extraction ... */
            yaz_log (YLOG_WARN, "extract error: generic");
            return ZEBRA_FAIL;
        case RECCTRL_EXTRACT_ERROR_NO_SUCH_FILTER:
            /* error occured during extraction ... */
            yaz_log (YLOG_WARN, "extract error: no such filter");
            return ZEBRA_FAIL;
        case RECCTRL_EXTRACT_SKIP:
            if (show_progress)
                yaz_log (YLOG_LOG, "skip %s %s " ZINT_FORMAT,
                         recordType, pr_fname, (zint) start_offset);
            *more = 1;
            
            end_offset = stream->endf(stream, 0);
            if (end_offset)
                stream->seekf(stream, end_offset);

            return ZEBRA_OK;
        case RECCTRL_EXTRACT_OK:
            break;
        default:
            yaz_log (YLOG_WARN, "extract error: unknown error: %d", r);
            return ZEBRA_FAIL;
        }
        end_offset = stream->endf(stream, 0);
        if (end_offset)
            stream->seekf(stream, end_offset);
        else
            end_offset = stream->tellf(stream);

        all_matches_add(&extractCtrl);
        
        if (extractCtrl.match_criteria[0])
            match_criteria = extractCtrl.match_criteria;
    }

    *more = 1;
    if (!sysno)
    {
	sysno = &sysno0;

        if (match_criteria && *match_criteria) {
            matchStr = match_criteria;
        } else {
            if (zh->m_record_id && *zh->m_record_id) {
                matchStr = get_match_from_spec(zh, zh->reg->keys, pr_fname, 
                                               zh->m_record_id);
		if (!matchStr)
                {
                    yaz_log (YLOG_LOG, "error %s %s " ZINT_FORMAT, recordType,
                             pr_fname, (zint) start_offset);
		    return ZEBRA_FAIL;
                }
            }
        }
        if (matchStr) 
	{
	    int db_ord = zebraExplain_get_database_ord(zh->reg->zei);
	    char *rinfo = dict_lookup_ord(zh->reg->matchDict, db_ord,
					  matchStr);
            if (rinfo)
	    {
		assert(*rinfo == sizeof(*sysno));
                memcpy (sysno, rinfo+1, sizeof(*sysno));
	    }
        }
    }
    if (zebra_rec_keys_empty(zh->reg->keys))
    {
	/* the extraction process returned no information - the record
	   is probably empty - unless flagShowRecords is in use */
	if (test_mode)
	    return ZEBRA_OK;
    }

    if (! *sysno)
    {
        /* new record */
        if (action == action_delete)
        {
	    yaz_log (YLOG_LOG, "delete %s %s " ZINT_FORMAT, recordType,
			 pr_fname, (zint) start_offset);
            yaz_log (YLOG_WARN, "cannot delete record above (seems new)");
            return ZEBRA_FAIL;
        }
	else if (action == action_replace)
	{
	    yaz_log (YLOG_LOG, "update %s %s " ZINT_FORMAT, recordType,
			 pr_fname, (zint) start_offset);
            yaz_log (YLOG_WARN, "cannot update record above (seems new)");
            return ZEBRA_FAIL;
	}
	if (show_progress)
	    yaz_log (YLOG_LOG, "add %s %s " ZINT_FORMAT, recordType, pr_fname,
		     (zint) start_offset);
        rec = rec_new (zh->reg->records);

        *sysno = rec->sysno;

	recordAttr = rec_init_attr (zh->reg->zei, rec);
	if (extractCtrl.staticrank < 0)
        {
            yaz_log(YLOG_WARN, "Negative staticrank for record. Set to 0");
	    extractCtrl.staticrank = 0;
        }

        if (matchStr)
        {
	    int db_ord = zebraExplain_get_database_ord(zh->reg->zei);
            dict_insert_ord(zh->reg->matchDict, db_ord, matchStr,
			    sizeof(*sysno), sysno);
        }

	extract_flush_sort_keys(zh, *sysno, 1, zh->reg->sortKeys);
#if FLUSH2
        extract_flush_record_keys2(zh, *sysno,
                                   zh->reg->keys, extractCtrl.staticrank,
                                   0, recordAttr->staticrank);
#else
        extract_flush_record_keys(zh, *sysno, 1, zh->reg->keys,
                                  extractCtrl.staticrank);
#endif
	recordAttr->staticrank = extractCtrl.staticrank;
        zh->records_inserted++;
    } 
    else
    {
        /* record already exists */
	zebra_rec_keys_t delkeys = zebra_rec_keys_open();
	zebra_rec_keys_t sortKeys = zebra_rec_keys_open();
	if (action == action_insert)
	{
	    yaz_log (YLOG_LOG, "skipped %s %s " ZINT_FORMAT, 
			 recordType, pr_fname, (zint) start_offset);
	    logRecord(zh);
	    return ZEBRA_FAIL;
	}

        rec = rec_get (zh->reg->records, *sysno);
        assert (rec);
	
	recordAttr = rec_init_attr (zh->reg->zei, rec);

        /* decrease total size */
        zebraExplain_recordBytesIncrement (zh->reg->zei,
                                           - recordAttr->recordSize);

	zebra_rec_keys_set_buf(delkeys,
			       rec->info[recInfo_delKeys],
			       rec->size[recInfo_delKeys],
			       0);
	zebra_rec_keys_set_buf(sortKeys,
			       rec->info[recInfo_sortKeys],
			       rec->size[recInfo_sortKeys],
			       0);

	extract_flush_sort_keys(zh, *sysno, 0, sortKeys);
#if !FLUSH2
        extract_flush_record_keys(zh, *sysno, 0, delkeys,
                                  recordAttr->staticrank);
#endif
        if (action == action_delete)
        {
            /* record going to be deleted */
#if FLUSH2
            extract_flush_record_keys2(zh, *sysno, 0, recordAttr->staticrank,
                                       delkeys, recordAttr->staticrank);
#endif       
            if (zebra_rec_keys_empty(delkeys))
            {
		yaz_log(YLOG_LOG, "delete %s %s " ZINT_FORMAT, recordType,
                        pr_fname, (zint) start_offset);
		yaz_log(YLOG_WARN, "cannot delete file above, "
                        "storeKeys false (3)");
	    }
            else
            {
		if (show_progress)
		    yaz_log(YLOG_LOG, "delete %s %s " ZINT_FORMAT, recordType,
                            pr_fname, (zint) start_offset);
                zh->records_deleted++;
                if (matchStr)
		{
		    int db_ord = zebraExplain_get_database_ord(zh->reg->zei);
                    dict_delete_ord(zh->reg->matchDict, db_ord, matchStr);
		}
                rec_del (zh->reg->records, &rec);
            }
            zebra_rec_keys_close(delkeys);
            zebra_rec_keys_close(sortKeys);
	    rec_free(&rec);
            logRecord(zh);
            return ZEBRA_OK;
        }
        else
        {   /* update or special_update */
	    if (show_progress)
                yaz_log(YLOG_LOG, "update %s %s " ZINT_FORMAT, recordType,
                        pr_fname, (zint) start_offset);
            extract_flush_sort_keys(zh, *sysno, 1, zh->reg->sortKeys);

#if FLUSH2
            extract_flush_record_keys2(zh, *sysno,
                                       zh->reg->keys, extractCtrl.staticrank,
                                       delkeys, recordAttr->staticrank);
#else
            extract_flush_record_keys(zh, *sysno, 1, 
                                      zh->reg->keys, extractCtrl.staticrank);
#endif
	    recordAttr->staticrank = extractCtrl.staticrank;
            zh->records_updated++;
        }
	zebra_rec_keys_close(delkeys);
	zebra_rec_keys_close(sortKeys);
    }
    /* update file type */
    xfree (rec->info[recInfo_fileType]);
    rec->info[recInfo_fileType] =
        rec_strdup (recordType, &rec->size[recInfo_fileType]);

    /* update filename */
    xfree (rec->info[recInfo_filename]);
    rec->info[recInfo_filename] =
        rec_strdup (fname, &rec->size[recInfo_filename]);

    /* update delete keys */
    xfree (rec->info[recInfo_delKeys]);
    if (!zebra_rec_keys_empty(zh->reg->keys) && zh->m_store_keys == 1)
    {
	zebra_rec_keys_get_buf(zh->reg->keys,
			       &rec->info[recInfo_delKeys],
			       &rec->size[recInfo_delKeys]);
    }
    else
    {
        rec->info[recInfo_delKeys] = NULL;
        rec->size[recInfo_delKeys] = 0;
    }
    /* update sort keys */
    xfree (rec->info[recInfo_sortKeys]);

    zebra_rec_keys_get_buf(zh->reg->sortKeys,
			   &rec->info[recInfo_sortKeys],
			   &rec->size[recInfo_sortKeys]);

    if (stream)
    {
        recordAttr->recordSize = end_offset - start_offset;
        zebraExplain_recordBytesIncrement(zh->reg->zei,
                                          recordAttr->recordSize);
    }

    /* set run-number for this record */
    recordAttr->runNumber =
	zebraExplain_runNumberIncrement (zh->reg->zei, 0);

    /* update store data */
    xfree (rec->info[recInfo_storeData]);

    /* update store data */
    if (zh->store_data_buf)
    {
        rec->size[recInfo_storeData] = zh->store_data_size;
        rec->info[recInfo_storeData] = zh->store_data_buf;
	zh->store_data_buf = 0;
        recordAttr->recordSize = zh->store_data_size;
    }
    else if (zh->m_store_data)
    {
        off_t cur_offset = stream->tellf(stream);

        rec->size[recInfo_storeData] = recordAttr->recordSize;
        rec->info[recInfo_storeData] = (char *)
	    xmalloc (recordAttr->recordSize);
        stream->seekf(stream, start_offset);
        stream->readf(stream, rec->info[recInfo_storeData],
                      recordAttr->recordSize);
        stream->seekf(stream, cur_offset);
    }
    else
    {
        rec->info[recInfo_storeData] = NULL;
        rec->size[recInfo_storeData] = 0;
    }
    /* update database name */
    xfree (rec->info[recInfo_databaseName]);
    rec->info[recInfo_databaseName] =
        rec_strdup (zh->basenames[0], &rec->size[recInfo_databaseName]); 

    /* update offset */
    recordAttr->recordOffset = start_offset;
    
    /* commit this record */
    rec_put (zh->reg->records, &rec);
    logRecord(zh);
    return ZEBRA_OK;
}

ZEBRA_RES zebra_extract_explain(void *handle, Record rec, data1_node *n)
{
    ZebraHandle zh = (ZebraHandle) handle;
    struct recExtractCtrl extractCtrl;

    if (zebraExplain_curDatabase (zh->reg->zei,
				  rec->info[recInfo_databaseName]))
    {
	abort();
        if (zebraExplain_newDatabase (zh->reg->zei,
				      rec->info[recInfo_databaseName], 0))
            abort ();
    }

    zebra_rec_keys_reset(zh->reg->keys);
    zebra_rec_keys_reset(zh->reg->sortKeys);

    extractCtrl.init = extract_init;
    extractCtrl.tokenAdd = extract_token_add;
    extractCtrl.schemaAdd = extract_schema_add;
    extractCtrl.dh = zh->reg->dh;

    init_extractCtrl(zh, &extractCtrl);

    extractCtrl.flagShowRecords = 0;
    extractCtrl.match_criteria[0] = '\0';
    extractCtrl.staticrank = 0;
    extractCtrl.action = action_update;

    extractCtrl.handle = handle;
    extractCtrl.first_record = 1;
    
    extract_set_store_data_prepare(&extractCtrl);

    if (n)
	grs_extract_tree(&extractCtrl, n);

    if (rec->size[recInfo_delKeys])
    {
	zebra_rec_keys_t delkeys = zebra_rec_keys_open();
	
	zebra_rec_keys_t sortkeys = zebra_rec_keys_open();

	zebra_rec_keys_set_buf(delkeys, rec->info[recInfo_delKeys],
			       rec->size[recInfo_delKeys],
			       0);
#if FLUSH2
	extract_flush_record_keys2(zh, rec->sysno, 
                                   zh->reg->keys, 0, delkeys, 0);
#else
	extract_flush_record_keys(zh, rec->sysno, 0, delkeys, 0);
        extract_flush_record_keys(zh, rec->sysno, 1, zh->reg->keys, 0);
#endif
	zebra_rec_keys_close(delkeys);

	zebra_rec_keys_set_buf(sortkeys, rec->info[recInfo_sortKeys],
			       rec->size[recInfo_sortKeys],
			       0);

	extract_flush_sort_keys(zh, rec->sysno, 0, sortkeys);
	zebra_rec_keys_close(sortkeys);
    }
    else
    {
#if FLUSH2
	extract_flush_record_keys2(zh, rec->sysno, zh->reg->keys, 0, 0, 0);
#else
        extract_flush_record_keys(zh, rec->sysno, 1, zh->reg->keys, 0);        
#endif
    }
    extract_flush_sort_keys(zh, rec->sysno, 1, zh->reg->sortKeys);
    
    xfree (rec->info[recInfo_delKeys]);
    zebra_rec_keys_get_buf(zh->reg->keys,
			   &rec->info[recInfo_delKeys],	
			   &rec->size[recInfo_delKeys]);

    xfree (rec->info[recInfo_sortKeys]);
    zebra_rec_keys_get_buf(zh->reg->sortKeys,
			   &rec->info[recInfo_sortKeys],
			   &rec->size[recInfo_sortKeys]);
    return ZEBRA_OK;
}

void extract_rec_keys_log(ZebraHandle zh, int is_insert,
                          zebra_rec_keys_t reckeys,
                          int level)
{
    if (zebra_rec_keys_rewind(reckeys))
    {
	size_t slen;
	const char *str;
	struct it_key key;
        NMEM nmem = nmem_create();

	while(zebra_rec_keys_read(reckeys, &str, &slen, &key))
        {
            char keystr[200]; /* room for zints to print */
            char *dst_term = 0;
            int ord = CAST_ZINT_TO_INT(key.mem[0]);
            int index_type, i;
            const char *string_index;
            
            zebraExplain_lookup_ord(zh->reg->zei, ord, &index_type,
                                    0/* db */, &string_index);
            assert(index_type);
            zebra_term_untrans_iconv(zh, nmem, index_type,
                                     &dst_term, str);
            *keystr = '\0';
            for (i = 0; i<key.len; i++)
            {
                sprintf(keystr + strlen(keystr), ZINT_FORMAT " ", key.mem[i]);
            }

            if (*str < CHR_BASE_CHAR)
            {
                int i;
                char dst_buf[200]; /* room for special chars */

                strcpy(dst_buf , "?");

                if (!strcmp(str, ""))
                    strcpy(dst_buf, "alwaysmatches");
                if (!strcmp(str, FIRST_IN_FIELD_STR))
                    strcpy(dst_buf, "firstinfield");
                else if (!strcmp(str, CHR_UNKNOWN))
                    strcpy(dst_buf, "unknown");
                else if (!strcmp(str, CHR_SPACE))
                    strcpy(dst_buf, "space");
                
                for (i = 0; i<slen; i++)
                {
                    sprintf(dst_buf + strlen(dst_buf), " %d", str[i] & 0xff);
                }
                yaz_log(level, "%s%c %s %s", keystr, index_type,
                        string_index, dst_buf);
                
            }
            else
                yaz_log(level, "%s%c %s \"%s\"", keystr, index_type,
                        string_index, dst_term);

            nmem_reset(nmem);
        }
        nmem_destroy(nmem);
    }
}

void extract_rec_keys_adjust(ZebraHandle zh, int is_insert,
                             zebra_rec_keys_t reckeys)
{
    ZebraExplainInfo zei = zh->reg->zei;
    struct ord_stat {
        int no;
        int ord;
        struct ord_stat *next;
    };

    if (zebra_rec_keys_rewind(reckeys))
    {
        struct ord_stat *ord_list = 0;
        struct ord_stat *p;
	size_t slen;
	const char *str;
	struct it_key key_in;
	while(zebra_rec_keys_read(reckeys, &str, &slen, &key_in))
        {
            int ord = CAST_ZINT_TO_INT(key_in.mem[0]);

            for (p = ord_list; p ; p = p->next)
                if (p->ord == ord)
                {
                    p->no++;
                    break;
                }
            if (!p)
            {
                p = xmalloc(sizeof(*p));
                p->no = 1;
                p->ord = ord;
                p->next = ord_list;
                ord_list = p;
            }
        }

        p = ord_list;
        while (p)
        {
            struct ord_stat *p1 = p;

            if (is_insert)
                zebraExplain_ord_adjust_occurrences(zei, p->ord, p->no, 1);
            else
                zebraExplain_ord_adjust_occurrences(zei, p->ord, - p->no, -1);
            p = p->next;
            xfree(p1);
        }
    }
}

void extract_flush_record_keys2(ZebraHandle zh, zint sysno,
                                zebra_rec_keys_t ins_keys, zint ins_rank,
                                zebra_rec_keys_t del_keys, zint del_rank)
{
    ZebraExplainInfo zei = zh->reg->zei;
    int normal = 0;
    int optimized = 0;

    if (!zh->reg->key_block)
    {
	int mem = 1024*1024 * atoi( res_get_def( zh->res, "memmax", "8"));
        const char *key_tmp_dir = res_get_def (zh->res, "keyTmpDir", ".");
        int use_threads = atoi(res_get_def (zh->res, "threads", "1"));
        zh->reg->key_block = key_block_create(mem, key_tmp_dir, use_threads);
    }

    if (ins_keys)
    {
        extract_rec_keys_adjust(zh, 1, ins_keys);
        if (!del_keys)
            zebraExplain_recordCountIncrement (zei, 1);
        zebra_rec_keys_rewind(ins_keys);
    }
    if (del_keys)
    {
        extract_rec_keys_adjust(zh, 0, del_keys);
        if (!ins_keys)
            zebraExplain_recordCountIncrement (zei, -1);
        zebra_rec_keys_rewind(del_keys);
    }

    while (1)
    {
	size_t del_slen;
	const char *del_str;
	struct it_key del_key_in;
        int del = 0;

	size_t ins_slen;
	const char *ins_str;
	struct it_key ins_key_in;
        int ins = 0;

        if (del_keys)
            del = zebra_rec_keys_read(del_keys, &del_str, &del_slen,
                                      &del_key_in);
        if (ins_keys)
            ins = zebra_rec_keys_read(ins_keys, &ins_str, &ins_slen,
                                      &ins_key_in);

        if (del && ins && ins_rank == del_rank
            && !key_compare(&del_key_in, &ins_key_in) 
            && ins_slen == del_slen && !memcmp(del_str, ins_str, del_slen))
        {
            optimized++;
            continue;
        }
        if (!del && !ins)
            break;
        
        normal++;
        if (del)
            key_block_write(zh->reg->key_block, sysno, 
                            &del_key_in, 0, del_str, del_slen,
                            del_rank, zh->m_staticrank);
        if (ins)
            key_block_write(zh->reg->key_block, sysno, 
                            &ins_key_in, 1, ins_str, ins_slen,
                            ins_rank, zh->m_staticrank);
    }
    yaz_log(YLOG_LOG, "normal=%d optimized=%d", normal, optimized);
}

void extract_flush_record_keys(ZebraHandle zh, zint sysno, int cmd,
                               zebra_rec_keys_t reckeys,
                               zint staticrank)
{
    ZebraExplainInfo zei = zh->reg->zei;

    extract_rec_keys_adjust(zh, cmd, reckeys);

    if (log_level_details)
    {
        yaz_log(log_level_details, "Keys for record " ZINT_FORMAT " %s",
                sysno, cmd ? "insert" : "delete");
        extract_rec_keys_log(zh, cmd, reckeys, log_level_details);
    }

    if (!zh->reg->key_block)
    {
	int mem = 1024*1024 * atoi( res_get_def( zh->res, "memmax", "8"));
        const char *key_tmp_dir = res_get_def (zh->res, "keyTmpDir", ".");
        int use_threads = atoi(res_get_def (zh->res, "threads", "1"));
        zh->reg->key_block = key_block_create(mem, key_tmp_dir, use_threads);
    }
    zebraExplain_recordCountIncrement (zei, cmd ? 1 : -1);

#if 0
    yaz_log(YLOG_LOG, "sysno=" ZINT_FORMAT " cmd=%d", sysno, cmd);
    print_rec_keys(zh, reckeys);
#endif
    if (zebra_rec_keys_rewind(reckeys))
    {
	size_t slen;
	const char *str;
	struct it_key key_in;
	while(zebra_rec_keys_read(reckeys, &str, &slen, &key_in))
	{
            key_block_write(zh->reg->key_block, sysno, 
                            &key_in, cmd, str, slen,
                            staticrank, zh->m_staticrank);
	}
    }
}

ZEBRA_RES zebra_rec_keys_to_snippets(ZebraHandle zh,
                                     zebra_rec_keys_t reckeys,
                                     zebra_snippets *snippets)
{
    NMEM nmem = nmem_create();
    if (zebra_rec_keys_rewind(reckeys)) 
    {
	const char *str;
	size_t slen;
	struct it_key key;
	while (zebra_rec_keys_read(reckeys, &str, &slen, &key))
	{
	    char *dst_term = 0;
	    int ord;
            zint seqno;
	    int index_type;

	    assert(key.len <= IT_KEY_LEVEL_MAX && key.len > 2);
	    seqno = key.mem[key.len-1];
	    ord = CAST_ZINT_TO_INT(key.mem[0]);
	    
	    zebraExplain_lookup_ord(zh->reg->zei, ord, &index_type,
				    0/* db */, 0 /* string_index */);
	    assert(index_type);
	    zebra_term_untrans_iconv(zh, nmem, index_type,
				     &dst_term, str);
	    zebra_snippets_append(snippets, seqno, ord, dst_term);
	    nmem_reset(nmem);
	}
    }
    nmem_destroy(nmem);
    return ZEBRA_OK;
}

void print_rec_keys(ZebraHandle zh, zebra_rec_keys_t reckeys)
{
    yaz_log(YLOG_LOG, "print_rec_keys");
    if (zebra_rec_keys_rewind(reckeys))
    {
	const char *str;
	size_t slen;
	struct it_key key;
	while (zebra_rec_keys_read(reckeys, &str, &slen, &key))
	{
	    char dst_buf[IT_MAX_WORD];
	    zint seqno;
	    int index_type;
            int ord = CAST_ZINT_TO_INT(key.mem[0]);
	    const char *db = 0;
	    assert(key.len <= IT_KEY_LEVEL_MAX && key.len > 2);

	    zebraExplain_lookup_ord(zh->reg->zei, ord, &index_type, &db, 0);
	    
	    seqno = key.mem[key.len-1];
	    
	    zebra_term_untrans(zh, index_type, dst_buf, str);
	    
	    yaz_log(YLOG_LOG, "ord=%d seqno=" ZINT_FORMAT 
                    " term=%s", ord, seqno, dst_buf); 
	}
    }
}

static void extract_add_index_string(RecWord *p, zinfo_index_category_t cat,
                                     const char *str, int length)
{
    struct it_key key;
    ZebraHandle zh = p->extractCtrl->handle;
    ZebraExplainInfo zei = zh->reg->zei;
    int ch, i;

    ch = zebraExplain_lookup_attr_str(zei, cat, p->index_type, p->index_name);
    if (ch < 0)
        ch = zebraExplain_add_attr_str(zei, cat, p->index_type, p->index_name);

    i = 0;
    key.mem[i++] = ch;
    key.mem[i++] = p->record_id;
    key.mem[i++] = p->section_id;

    if (zh->m_segment_indexing)
        key.mem[i++] = p->segment;
    key.mem[i++] = p->seqno;
    key.len = i;

    zebra_rec_keys_write(zh->reg->keys, str, length, &key);
}

static void extract_add_sort_string(RecWord *p, const char *str, int length)
{
    struct it_key key;
    ZebraHandle zh = p->extractCtrl->handle;
    ZebraExplainInfo zei = zh->reg->zei;
    int ch;
    zinfo_index_category_t cat = zinfo_index_category_sort;

    ch = zebraExplain_lookup_attr_str(zei, cat, p->index_type, p->index_name);
    if (ch < 0)
        ch = zebraExplain_add_attr_str(zei, cat, p->index_type, p->index_name);
    key.len = 2;
    key.mem[0] = ch;
    key.mem[1] = p->record_id;

    zebra_rec_keys_write(zh->reg->sortKeys, str, length, &key);
}

static void extract_add_staticrank_string(RecWord *p,
                                          const char *str, int length)
{
    char valz[40];
    struct recExtractCtrl *ctrl = p->extractCtrl;

    if (length > sizeof(valz)-1)
        length = sizeof(valz)-1;

    memcpy(valz, str, length);
    valz[length] = '\0';
    ctrl->staticrank = atozint(valz);
}

static void extract_add_string(RecWord *p, const char *string, int length)
{
    ZebraHandle zh = p->extractCtrl->handle;
    assert (length > 0);

    if (!p->index_name)
        return;

    if (zebra_maps_is_index(zh->reg->zebra_maps, p->index_type))
    {
	extract_add_index_string(p, zinfo_index_category_index,
                                 string, length);
        if (zebra_maps_is_alwaysmatches(zh->reg->zebra_maps, p->index_type))
        {
            RecWord word;
            memcpy(&word, p, sizeof(word));

            word.seqno = 1;
            extract_add_index_string(
                &word, zinfo_index_category_alwaysmatches, "", 0);
        }
    }
    else if (zebra_maps_is_sort(zh->reg->zebra_maps, p->index_type))
    {
	extract_add_sort_string(p, string, length);
    }
    else if (zebra_maps_is_staticrank(zh->reg->zebra_maps, p->index_type))
    {
	extract_add_staticrank_string(p, string, length);
    }
}

static void extract_add_incomplete_field(RecWord *p)
{
    ZebraHandle zh = p->extractCtrl->handle;
    const char *b = p->term_buf;
    int remain = p->term_len;
    int first = 1;
    const char **map = 0;
    
    if (remain > 0)
	map = zebra_maps_input(zh->reg->zebra_maps, p->index_type, &b, remain, 0);

    while (map)
    {
	char buf[IT_MAX_WORD+1];
	int i, remain;

	/* Skip spaces */
	while (map && *map && **map == *CHR_SPACE)
	{
	    remain = p->term_len - (b - p->term_buf);
	    if (remain > 0)
		map = zebra_maps_input(zh->reg->zebra_maps, p->index_type, &b,
				       remain, 0);
	    else
		map = 0;
	}
	if (!map)
	    break;
	i = 0;
	while (map && *map && **map != *CHR_SPACE)
	{
	    const char *cp = *map;

	    while (i < IT_MAX_WORD && *cp)
		buf[i++] = *(cp++);
	    remain = p->term_len - (b - p->term_buf);
	    if (remain > 0)
		map = zebra_maps_input(zh->reg->zebra_maps, p->index_type, &b, remain, 0);
	    else
		map = 0;
	}
	if (!i)
	    return;

        if (first)
        {   
            first = 0;
            if (zebra_maps_is_first_in_field(zh->reg->zebra_maps, p->index_type))
            {
                /* first in field marker */
                extract_add_string(p, FIRST_IN_FIELD_STR, FIRST_IN_FIELD_LEN);
                p->seqno++;
            }
        }
	extract_add_string (p, buf, i);
        p->seqno++;
    }
}

static void extract_add_complete_field (RecWord *p)
{
    ZebraHandle zh = p->extractCtrl->handle;
    const char *b = p->term_buf;
    char buf[IT_MAX_WORD+1];
    const char **map = 0;
    int i = 0, remain = p->term_len;

    if (remain > 0)
	map = zebra_maps_input (zh->reg->zebra_maps, p->index_type, &b, remain, 1);

    while (remain > 0 && i < IT_MAX_WORD)
    {
	while (map && *map && **map == *CHR_SPACE)
	{
	    remain = p->term_len - (b - p->term_buf);

	    if (remain > 0)
	    {
		int first = i ? 0 : 1;  /* first position */
		map = zebra_maps_input(zh->reg->zebra_maps, p->index_type, &b, remain, first);
	    }
	    else
		map = 0;
	}
	if (!map)
	    break;

	if (i && i < IT_MAX_WORD)
	    buf[i++] = *CHR_SPACE;
	while (map && *map && **map != *CHR_SPACE)
	{
	    const char *cp = *map;

	    if (**map == *CHR_CUT)
	    {
		i = 0;
	    }
	    else
	    {
		if (i >= IT_MAX_WORD)
		    break;
		while (i < IT_MAX_WORD && *cp)
		    buf[i++] = *(cp++);
	    }
	    remain = p->term_len  - (b - p->term_buf);
	    if (remain > 0)
	    {
		map = zebra_maps_input (zh->reg->zebra_maps, p->index_type, &b,
					remain, 0);
	    }
	    else
		map = 0;
	}
    }
    if (!i)
	return;
    extract_add_string (p, buf, i);
}

static void extract_token_add(RecWord *p)
{
    ZebraHandle zh = p->extractCtrl->handle;
    WRBUF wrbuf;

    if (log_level_extract)
    {
        yaz_log(log_level_extract, "extract_token_add "
                "type=%c index=%s seqno=" ZINT_FORMAT " s=%.*s",
                p->index_type, p->index_name, 
                p->seqno, p->term_len, p->term_buf);
    }
    if ((wrbuf = zebra_replace(zh->reg->zebra_maps, p->index_type, 0,
			       p->term_buf, p->term_len)))
    {
	p->term_buf = wrbuf_buf(wrbuf);
	p->term_len = wrbuf_len(wrbuf);
    }
    if (zebra_maps_is_complete (zh->reg->zebra_maps, p->index_type))
	extract_add_complete_field (p);
    else
	extract_add_incomplete_field(p);
}

static void extract_set_store_data_cb(struct recExtractCtrl *p,
				      void *buf, size_t sz)
{
    ZebraHandle zh = (ZebraHandle) p->handle;

    xfree(zh->store_data_buf);
    zh->store_data_buf = 0;
    zh->store_data_size = 0;
    if (buf && sz)
    {
	zh->store_data_buf = xmalloc(sz);
	zh->store_data_size = sz;
	memcpy(zh->store_data_buf, buf, sz);
    }
}

static void extract_set_store_data_prepare(struct recExtractCtrl *p)
{
    ZebraHandle zh = (ZebraHandle) p->handle;
    xfree(zh->store_data_buf);
    zh->store_data_buf = 0;
    zh->store_data_size = 0;
    p->setStoreData = extract_set_store_data_cb;
}

static void extract_schema_add(struct recExtractCtrl *p, Odr_oid *oid)
{
    ZebraHandle zh = (ZebraHandle) p->handle;
    zebraExplain_addSchema (zh->reg->zei, oid);
}

void extract_flush_sort_keys(ZebraHandle zh, zint sysno,
                             int cmd, zebra_rec_keys_t reckeys)
{
#if 0
    yaz_log(YLOG_LOG, "extract_flush_sort_keys cmd=%d sysno=" ZINT_FORMAT,
            cmd, sysno);
    extract_rec_keys_log(zh, cmd, reckeys, YLOG_LOG);
#endif

    if (zebra_rec_keys_rewind(reckeys))
    {
        zebra_sort_index_t si = zh->reg->sort_index;
	size_t slen;
	const char *str;
	struct it_key key_in;

        zebra_sort_sysno(si, sysno);

	while (zebra_rec_keys_read(reckeys, &str, &slen, &key_in))
        {
            int ord = CAST_ZINT_TO_INT(key_in.mem[0]);
            
            zebra_sort_type(si, ord);
            if (cmd == 1)
                zebra_sort_add(si, str, slen);
            else
                zebra_sort_delete(si);
        }
    }
}

/*
 * Local variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */

