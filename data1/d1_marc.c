/* $Id: d1_marc.c,v 1.2 2002-10-22 13:19:50 adam Exp $
   Copyright (C) 1995,1996,1997,1998,1999,2000,2001,2002
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

/* converts data1 tree to ISO2709/MARC record */

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <yaz/oid.h>
#include <yaz/log.h>
#include <yaz/marcdisp.h>
#include <yaz/readconf.h>
#include <yaz/xmalloc.h>
#include <yaz/tpath.h>
#include <data1.h>

data1_marctab *data1_read_marctab (data1_handle dh, const char *file)
{
    FILE *f;
    NMEM mem = data1_nmem_get (dh);
    data1_marctab *res = (data1_marctab *)nmem_malloc(mem, sizeof(*res));
    char line[512], *argv[50];
    int lineno = 0;
    int argc;
    
    if (!(f = data1_path_fopen(dh, file, "r")))
    {
	yaz_log(LOG_WARN|LOG_ERRNO, "%s", file);
	return 0;
    }

    res->name = 0;
    res->reference = VAL_NONE;
    res->next = 0;
    res->length_data_entry = 4;
    res->length_starting = 5;
    res->length_implementation = 0;
    strcpy(res->future_use, "4");

    strcpy(res->record_status, "n");
    strcpy(res->implementation_codes, "    ");
    res->indicator_length = 2;
    res->identifier_length = 2;
    res->force_indicator_length = -1;
    res->force_identifier_length = -1;
    strcpy(res->user_systems, "z  ");
    
    while ((argc = readconf_line(f, &lineno, line, 512, argv, 50)))
	if (!strcmp(*argv, "name"))
	{
	    if (argc != 2)
	    {
		yaz_log(LOG_WARN, "%s:%d:Missing arg for %s", file, lineno,
			*argv);
		continue;
	    }
	    res->name = nmem_strdup(mem, argv[1]);
	}
	else if (!strcmp(*argv, "reference"))
	{
	    if (argc != 2)
	    {
		yaz_log(LOG_WARN, "%s:%d: Missing arg for %s", file, lineno,
			*argv);
		continue;
	    }
	    if ((res->reference = oid_getvalbyname(argv[1])) == VAL_NONE)
	    {
		yaz_log(LOG_WARN, "%s:%d: Unknown tagset reference '%s'",
			file, lineno, argv[1]);
		continue;
	    }
	}
	else if (!strcmp(*argv, "length-data-entry"))
	{
	    if (argc != 2)
	    {
		yaz_log(LOG_WARN, "%s:%d: Missing arg for %s", file, lineno,
			*argv);
		continue;
	    }
	    res->length_data_entry = atoi(argv[1]);
	}
	else if (!strcmp(*argv, "length-starting"))
	{
	    if (argc != 2)
	    {
		yaz_log(LOG_WARN, "%s:%d: Missing arg for %s", file, lineno,
			*argv);
		continue;
	    }
	    res->length_starting = atoi(argv[1]);
	}
	else if (!strcmp(*argv, "length-implementation"))
	{
	    if (argc != 2)
	    {
		yaz_log(LOG_WARN, "%s:%d: Missing arg for %s", file, lineno,
			*argv);
		continue;
	    }
	    res->length_implementation = atoi(argv[1]);
	}
	else if (!strcmp(*argv, "future-use"))
	{
	    if (argc != 2)
	    {
		yaz_log(LOG_WARN, "%s:%d: Missing arg for %s", file, lineno,
			*argv);
		continue;
	    }
	    strncpy(res->future_use, argv[1], 2);
	}
	else if (!strcmp(*argv, "force-indicator-length"))
	{
	    if (argc != 2)
	    {
		yaz_log(LOG_WARN, "%s:%d: Missing arg for %s", file, lineno,
			*argv);
		continue;
	    }
	    res->force_indicator_length = atoi(argv[1]);
	}
	else if (!strcmp(*argv, "force-identifier-length"))
	{
	    if (argc != 2)
	    {
		yaz_log(LOG_WARN, "%s:%d: Missing arg for %s", file, lineno,
			*argv);
		continue;
	    }
	    res->force_identifier_length = atoi(argv[1]);
	}
	else
	    yaz_log(LOG_WARN, "%s:%d: Unknown directive '%s'", file, lineno,
		    *argv);

    fclose(f);
    return res;
}


/*
 * Locate some data under this node. This routine should handle variants
 * prettily.
 */
static char *get_data(data1_node *n, int *len)
{
    char *r;

    while (n)
    {
        if (n->which == DATA1N_data)
        {
            int i;
            *len = n->u.data.len;

            for (i = 0; i<*len; i++)
                if (!d1_isspace(n->u.data.data[i]))
                    break;
            while (*len && d1_isspace(n->u.data.data[*len - 1]))
                (*len)--;
            *len = *len - i;
            if (*len > 0)
                return n->u.data.data + i;
        }
        if (n->which == DATA1N_tag)
            n = n->child;
	else if (n->which == DATA1N_data)
            n = n->next;
	else
            break;	
    }
    r = "";
    *len = strlen(r);
    return r;
}

static void memint (char *p, int val, int len)
{
    char buf[10];

    if (len == 1)
        *p = val + '0';
    else
    {
        sprintf (buf, "%08d", val);
        memcpy (p, buf+8-len, len);
    }
}

static int is_indicator (data1_marctab *p, data1_node *subf)
{
#if 1
    if (p->indicator_length != 2 ||
	(subf && subf->which == DATA1N_tag && strlen(subf->u.tag.tag) == 2))
	return 1;
#else
    if (subf && subf->which == DATA1N_tag && subf->child->which == DATA1N_tag)
	return 1;
#endif
    return 0;
}

static int nodetomarc(data1_handle dh,
		      data1_marctab *p, data1_node *n, int selected,
                      char **buf, int *size)
{
    int len = 26;
    int dlen;
    int base_address = 25;
    int entry_p, data_p;
    char *op;
    data1_node *field, *subf;

    yaz_log (LOG_DEBUG, "nodetomarc");

    for (field = n->child; field; field = field->next)
    {
        int is00X = 0;

	if (field->which != DATA1N_tag)
	{
	    yaz_log(LOG_WARN, "Malformed field composition for marc output.");
	    return -1;
	}
	if (selected && !field->u.tag.node_selected)
	    continue;

	subf = field->child;
        if (!subf)
            continue;

        len += 4 + p->length_data_entry + p->length_starting
            + p->length_implementation;
        base_address += 3 + p->length_data_entry + p->length_starting
            + p->length_implementation;

	if (subf->which == DATA1N_data)
            is00X = 1;
	if (!data1_is_xmlmode(dh))
        {
            if (subf->which == DATA1N_tag && !strcmp(subf->u.tag.tag, "@"))
                is00X = 1;
        }
            
	
        if (!is00X)
            len += p->indicator_length;  
	/*  we'll allow no indicator if length is not 2 */
	if (is_indicator (p, subf))
	    subf = subf->child;

        for (; subf; subf = subf->next)
        {
            if (!is00X)
                len += p->identifier_length;
	    get_data(subf, &dlen);
            len += dlen;
        }
    }

    if (!*buf)
	*buf = (char *)xmalloc(*size = len);
    else if (*size <= len)
	*buf = (char *)xrealloc(*buf, *size = len);
	
    op = *buf;
    memint (op, len, 5);
    memcpy (op+5, p->record_status, 1);
    memcpy (op+6, p->implementation_codes, 4);
    memint (op+10, p->indicator_length, 1);
    memint (op+11, p->identifier_length, 1);
    memint (op+12, base_address, 5);
    memcpy (op+17, p->user_systems, 3);
    memint (op+20, p->length_data_entry, 1);
    memint (op+21, p->length_starting, 1);
    memint (op+22, p->length_implementation, 1);
    memcpy (op+23, p->future_use, 1);
    
    entry_p = 24;
    data_p = base_address;

    for (field = n->child; field; field = field->next)
    {
        int is00X = 0;

        int data_0 = data_p;
	char *indicator_data = "    ";
	if (selected && !field->u.tag.node_selected)
	    continue;

	subf = field->child;
        if (!subf)
            continue;

        if (subf->which == DATA1N_data)
            is00X = 1;
	if (!data1_is_xmlmode(dh))
        {
            if (subf->which == DATA1N_tag && !strcmp(subf->u.tag.tag, "@"))
                is00X = 1;
        }

	if (is_indicator (p, subf))
	{
            indicator_data = subf->u.tag.tag;
	    subf = subf->child;
	}
        if (!is00X)
        {
            memcpy (op + data_p, indicator_data, p->indicator_length);
            data_p += p->indicator_length;
        }
        for (; subf; subf = subf->next)
        {
	    char *data;

            if (!is00X)
            {
                const char *identifier = "a";
                if (subf->which != DATA1N_tag)
                    yaz_log(LOG_WARN, "Malformed fields for marc output.");
                else
                    identifier = subf->u.tag.tag;
                op[data_p] = ISO2709_IDFS;
                memcpy (op + data_p+1, identifier, p->identifier_length-1);
                data_p += p->identifier_length;
            }
	    data = get_data(subf, &dlen);
            memcpy (op + data_p, data, dlen);
            data_p += dlen;
        }
        op[data_p++] = ISO2709_FS;

        memcpy (op + entry_p, field->u.tag.tag, 3);
        entry_p += 3;
        memint (op + entry_p, data_p - data_0, p->length_data_entry);
        entry_p += p->length_data_entry;
        memint (op + entry_p, data_0 - base_address, p->length_starting);
        entry_p += p->length_starting;
        entry_p += p->length_implementation;
    }
    op[entry_p++] = ISO2709_FS;
    assert (entry_p == base_address);
    op[data_p++] = ISO2709_RS;
    assert (data_p == len);
    return len;
}

char *data1_nodetomarc(data1_handle dh, data1_marctab *p, data1_node *n,
		       int selected, int *len)
{
    int *size;
    char **buf = data1_get_map_buf (dh, &size);

    n = data1_get_root_tag (dh, n);
    if (!n)
        return 0;
    *len = nodetomarc(dh, p, n, selected, buf, size);
    return *buf;
}
