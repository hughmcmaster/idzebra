/* $Id: d1_grs.c,v 1.2 2002-10-22 13:19:50 adam Exp $
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

/* converts data1 tree to GRS-1 record */

#include <assert.h>
#include <stdlib.h>

#include <yaz/proto.h>
#include <yaz/log.h>
#include <data1.h>

#define D1_VARIANTARRAY 20 /* fixed max length on sup'd variant-list. Lazy me */

static Z_ElementMetaData *get_ElementMetaData(ODR o)
{
    Z_ElementMetaData *r = (Z_ElementMetaData *)odr_malloc(o, sizeof(*r));

    r->seriesOrder = 0;
    r->usageRight = 0;
    r->num_hits = 0;
    r->hits = 0;
    r->displayName = 0;
    r->num_supportedVariants = 0;
    r->supportedVariants = 0;
    r->message = 0;
    r->elementDescriptor = 0;
    r->surrogateFor = 0;
    r->surrogateElement = 0;
    r->other = 0;

    return r;
}

/*
 * N should point to the *last* (leaf) triple in a sequence. Construct a variant
 * from each of the triples beginning (ending) with 'n', up to the
 * nearest parent tag. num should equal the number of triples in the
 * sequence.
 */
static Z_Variant *make_variant(data1_node *n, int num, ODR o)
{
    Z_Variant *v = (Z_Variant *)odr_malloc(o, sizeof(*v));
    data1_node *p;

    v->globalVariantSetId = 0;
    v->num_triples = num;
    v->triples = (Z_Triple **)odr_malloc(o, sizeof(Z_Triple*) * num);

    /*
     * cycle back up through the tree of variants
     * (traversing exactly 'level' variants).
     */
    for (p = n, num--; p && num >= 0; p = p->parent, num--)
    {
	Z_Triple *t;

	assert(p->which == DATA1N_variant);
	t = v->triples[num] = (Z_Triple *)odr_malloc(o, sizeof(*t));
	t->variantSetId = 0;
	t->zclass = (int *)odr_malloc(o, sizeof(int));
	*t->zclass = p->u.variant.type->zclass->zclass;
	t->type = (int *)odr_malloc(o, sizeof(int));
	*t->type = p->u.variant.type->type;

	switch (p->u.variant.type->datatype)
	{
	    case DATA1K_string:
		t->which = Z_Triple_internationalString;
		t->value.internationalString =
                    odr_strdup(o, p->u.variant.value);
		break;
	    default:
		yaz_log(LOG_WARN, "Unable to handle value for variant %s",
			p->u.variant.type->name);
		return 0;
	}
    }
    return v;
}

/*
 * Traverse the variant children of n, constructing a supportedVariant list.
 */
static int traverse_triples(data1_node *n, int level, Z_ElementMetaData *m,
    ODR o)
{
    data1_node *c;
    
    for (c = n->child; c; c = c->next)
	if (c->which == DATA1N_data && level)
	{
	    if (!m->supportedVariants)
		m->supportedVariants = (Z_Variant **)odr_malloc(o, sizeof(Z_Variant*) *
		    D1_VARIANTARRAY);
	    else if (m->num_supportedVariants >= D1_VARIANTARRAY)
	    {
		yaz_log(LOG_WARN, "Too many variants (D1_VARIANTARRAY==%d)",
			D1_VARIANTARRAY);
		return -1;
	    }

	    if (!(m->supportedVariants[m->num_supportedVariants++] =
	    	make_variant(n, level, o)))
		return -1;
	}
	else if (c->which == DATA1N_variant)
	    if (traverse_triples(c, level+1, m, o) < 0)
		return -1;
    return 0;
}

/*
 * Locate some data under this node. This routine should handle variants
 * prettily.
 */
static char *get_data(data1_node *n, int *len)
{
    char *r;
    data1_node *np = 0;

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
            np = n->child;
        n = n->next;
        if (!n)
        {
            n = np;
            np = 0;
        }
    }
    r = "";
    *len = strlen(r);
    return r;
}

static Z_ElementData *nodetoelementdata(data1_handle dh, data1_node *n,
					int select, int leaf,
					ODR o, int *len)
{
    Z_ElementData *res = (Z_ElementData *)odr_malloc(o, sizeof(*res));

    if (!n)
    {
	res->which = Z_ElementData_elementNotThere;
	res->u.elementNotThere = odr_nullval();
    }
    else if (n->which == DATA1N_data && leaf)
    {
	char str[64], *cp;
	int toget = n->u.data.len;

        cp = get_data (n, &toget);

	switch (n->u.data.what)
	{
        case DATA1I_num:
            res->which = Z_ElementData_numeric;
            res->u.numeric = (int *)odr_malloc(o, sizeof(int));
            *res->u.numeric = atoi_n (cp, toget);
            *len += 4;
            break;
        case DATA1I_text:
        case DATA1I_xmltext:
            res->which = Z_ElementData_string;
            res->u.string = (char *)odr_malloc(o, toget+1);
            if (toget)
                memcpy(res->u.string, cp, toget);
            res->u.string[toget] = '\0';
            *len += toget;
            break;
        case DATA1I_oid:
            res->which = Z_ElementData_oid;
            if (toget > 63)
                toget = 63;
            memcpy (str, cp, toget);
            str[toget] = '\0';
            res->u.oid = odr_getoidbystr(o, str);
            *len += oid_oidlen(res->u.oid) * sizeof(int);
            break;
        default:
            yaz_log(LOG_WARN, "Can't handle datatype.");
            return 0;
	}
    }
    else
    {
	res->which = Z_ElementData_subtree;
	if (!(res->u.subtree = data1_nodetogr (dh, n->parent, select, o, len)))
	    return 0;
    }
    return res;
}

static int is_empty_data (data1_node *n)
{
    if (n && n->which == DATA1N_data && (n->u.data.what == DATA1I_text
			    	|| n->u.data.what == DATA1I_xmltext))
    {
        int i = n->u.data.len;
        
        while (i > 0 && strchr("\n ", n->u.data.data[i-1]))
            i--;
        if (i == 0)
            return 1;
    }
    return 0;
}


static Z_TaggedElement *nodetotaggedelement(data1_handle dh, data1_node *n,
					    int select, ODR o,
					    int *len)
{
    Z_TaggedElement *res = (Z_TaggedElement *)odr_malloc(o, sizeof(*res));
    data1_tag *tag = 0;
    data1_node *data;
    int leaf = 0;

    if (n->which == DATA1N_tag)
    {
	if (n->u.tag.element)
	    tag = n->u.tag.element->tag;
	data = n->child;

        /* skip empty data children */
        while (is_empty_data(data))
            data = data->next;
        if (!data)
            data = n->child;
        else
        {   /* got one. see if this is the only non-empty one */
            data1_node *sub = data->next;
            while (sub && is_empty_data(sub))
                sub = sub->next;
            if (!sub)
                leaf = 1;  /* all empty. 'data' is the only child */
        }
    }
    /*
     * If we're a data element at this point, we need to insert a
     * wellKnown tag to wrap us up.
     */
    else if (n->which == DATA1N_data || n->which == DATA1N_variant)
    {
	if (n->root->u.root.absyn &&
            !(tag = data1_gettagbyname (dh, n->root->u.root.absyn->tagset,
					"wellKnown")))
	{
	    yaz_log(LOG_WARN, "Unable to locate tag for 'wellKnown'");
	    return 0;
	}
	data = n;
	leaf = 1;
        if (is_empty_data(data))
            return 0;
    }
    else
    {
	yaz_log(LOG_WARN, "Bad data.");
	return 0;
    }

    res->tagType = (int *)odr_malloc(o, sizeof(int));
    *res->tagType = (tag && tag->tagset) ? tag->tagset->type : 3;
    res->tagValue = (Z_StringOrNumeric *)odr_malloc(o, sizeof(Z_StringOrNumeric));
    if (tag && tag->which == DATA1T_numeric)
    {
	res->tagValue->which = Z_StringOrNumeric_numeric;
	res->tagValue->u.numeric = (int *)odr_malloc(o, sizeof(int));
	*res->tagValue->u.numeric = tag->value.numeric;
    }
    else
    {
	char *tagstr;

	if (n->which == DATA1N_tag)      
	    tagstr = n->u.tag.tag;       /* tag at node */
	else if (tag)                    
	    tagstr = tag->value.string;  /* no take from well-known */
	else
            return 0;
	res->tagValue->which = Z_StringOrNumeric_string;
	res->tagValue->u.string = odr_strdup(o, tagstr);
    }
    res->tagOccurrence = 0;
    res->appliedVariant = 0;
    res->metaData = 0;
    if (n->which == DATA1N_variant || (data && data->which ==
	DATA1N_variant && data->next == NULL))
    {
	int nvars = 0;

	res->metaData = get_ElementMetaData(o);
	if (n->which == DATA1N_tag && n->u.tag.make_variantlist)
	    if (traverse_triples(data, 0, res->metaData, o) < 0)
		return 0;
	while (data && data->which == DATA1N_variant)
	{
	    nvars++;
	    data = data->child;
	}
	if (n->which != DATA1N_tag || !n->u.tag.no_data_requested)
	    res->appliedVariant = make_variant(data->parent, nvars-1, o);
    }
    if (n->which == DATA1N_tag && n->u.tag.no_data_requested)
    {
	res->content = (Z_ElementData *)odr_malloc(o, sizeof(*res->content));
	res->content->which = Z_ElementData_noDataRequested;
	res->content->u.noDataRequested = odr_nullval();
    }
    else if (!(res->content = nodetoelementdata (dh, data, select, leaf,
						 o, len)))
	return 0;
    *len += 10;
    return res;
}

Z_GenericRecord *data1_nodetogr(data1_handle dh, data1_node *n,
				int select, ODR o, int *len)
{
    Z_GenericRecord *res = (Z_GenericRecord *)odr_malloc(o, sizeof(*res));
    data1_node *c;
    int num_children = 0;

    if (n->which == DATA1N_root)
        n = data1_get_root_tag (dh, n);
        
    for (c = n->child; c; c = c->next)
	num_children++;

    res->elements = (Z_TaggedElement **)
        odr_malloc(o, sizeof(Z_TaggedElement *) * num_children);
    res->num_elements = 0;
    for (c = n->child; c; c = c->next)
    {
	if (c->which == DATA1N_tag && select && !c->u.tag.node_selected)
	    continue;
	if ((res->elements[res->num_elements] =
             nodetotaggedelement (dh, c, select, o, len)))
	    res->num_elements++;
    }
    return res;
}
