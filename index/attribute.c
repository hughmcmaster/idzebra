/* $Id: attribute.c,v 1.25 2006-06-23 11:21:38 adam Exp $
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

#include <stdio.h>

#include <yaz/log.h>
#include <yaz/diagbib1.h>
#include <idzebra/res.h>
#include <idzebra/util.h>
#include <attrfind.h>
#include "index.h"

static data1_att *getatt(data1_attset *p, int att)
{
    data1_att *a;
    data1_attset_child *c;

    /* scan local set */
    for (a = p->atts; a; a = a->next)
	if (a->value == att)
	    return a;
    /* scan included sets */
    for (c = p->children; c; c = c->next)
	if ((a = getatt(c->child, att)))
	    return a;
    return 0;
}

static int att_getentbyatt(ZebraHandle zi, oid_value set, int att,
                           const char **name)
{
    data1_att *r;
    data1_attset *p;

    if (!(p = data1_attset_search_id (zi->reg->dh, set)))
    {
	zebraExplain_loadAttsets (zi->reg->dh, zi->res);
	p = data1_attset_search_id (zi->reg->dh, set);
    }
    if (!p)   /* set undefined */
        return -2;
    if (!(r = getatt(p, att)))
	return -1;
    *name = r->name;
    return 0;
}


ZEBRA_RES zebra_attr_list_get_ord(ZebraHandle zh,
                                  Z_AttributeList *attr_list,
                                  zinfo_index_category_t cat,
                                  int index_type,
                                  oid_value curAttributeSet,
                                  int *ord)
{
    int use_value = -1;
    const char *use_string = 0;
    AttrType use;

    attr_init_AttrList(&use, attr_list, 1);
    use_value = attr_find_ex(&use, &curAttributeSet, &use_string);

    if (use_value < 0)
    {
        if (!use_string)
            use_string = "any";
    }
    else
    {
        /* we have a use attribute and attribute set */
        int r;
        
        r = att_getentbyatt(zh, curAttributeSet, use_value, &use_string);
        if (r == -2)
        {
            zebra_setError_zint(zh,  YAZ_BIB1_UNSUPP_ATTRIBUTE_SET, 0);
            return ZEBRA_FAIL;
        }
        if (r == -1)
        {
            zebra_setError_zint(zh, YAZ_BIB1_UNSUPP_USE_ATTRIBUTE,  use_value);
            return ZEBRA_FAIL;
        }
    }
    if (!use_string)
    {
        zebra_setError(zh, YAZ_BIB1_UNSUPP_USE_ATTRIBUTE, 0);
        return ZEBRA_FAIL;
    }
    *ord = zebraExplain_lookup_attr_str(zh->reg->zei, cat, 
                                        index_type, use_string);
    if (*ord == -1)
    {
        if (use_value < 0)
            zebra_setError(zh, YAZ_BIB1_UNSUPP_USE_ATTRIBUTE, use_string);
        else
            zebra_setError_zint(zh, YAZ_BIB1_UNSUPP_USE_ATTRIBUTE, use_value);
        return ZEBRA_FAIL;
    }
    return ZEBRA_OK;
}

ZEBRA_RES zebra_apt_get_ord(ZebraHandle zh,
                            Z_AttributesPlusTerm *zapt,
                            int index_type,
                            const char *xpath_use,
                            oid_value curAttributeSet,
                            int *ord)
{
    ZEBRA_RES res = ZEBRA_OK;
    AttrType relation;
    int relation_value;
    zinfo_index_category_t cat = zinfo_index_category_index;

    attr_init_APT(&relation, zapt, 2);
    relation_value = attr_find(&relation, NULL);

    if (relation_value == 103) /* always matches */
        cat = zinfo_index_category_alwaysmatches;
    
    if (!xpath_use)
    {
        res = zebra_attr_list_get_ord(zh, zapt->attributes,
                                      cat, index_type,
                                      curAttributeSet, ord);
        /* use attribute not found. But it the relation is
           always matches and the regulare index attribute is found
           return a different diagnostic */
        if (res != ZEBRA_OK && 
            relation_value == 103
            &&  zebra_attr_list_get_ord(
                zh, zapt->attributes, 
                zinfo_index_category_index, index_type,
                curAttributeSet, ord) == ZEBRA_OK)
            zebra_setError_zint(zh, YAZ_BIB1_UNSUPP_RELATION_ATTRIBUTE, 103);
    }
    else
    {
        *ord = zebraExplain_lookup_attr_str(zh->reg->zei, cat, index_type,
                                            xpath_use);
        if (*ord == -1)
        {
            yaz_log(YLOG_LOG, "zebra_apt_get_ord FAILED xpath=%s index_type=%c",
                    xpath_use, index_type);
            zebra_setError(zh, YAZ_BIB1_UNSUPP_USE_ATTRIBUTE, 0);
            res = ZEBRA_FAIL;
        }
        else
        {
            yaz_log(YLOG_LOG, "zebra_apt_get_ord OK xpath=%s index_type=%c",
                    xpath_use, index_type);
            
        }
    }
    return res;
}

ZEBRA_RES zebra_sort_get_ord(ZebraHandle zh,
                             Z_SortAttributes *sortAttributes,
                             int *ord,
                             int *numerical)
{
    AttrType structure;
    int structure_value;
    attr_init_AttrList(&structure, sortAttributes->list, 4);

    *numerical = 0;
    structure_value = attr_find(&structure, 0);
    if (structure_value == 109)
        *numerical = 1;
    
    if (zebra_attr_list_get_ord(zh, sortAttributes->list,
                                zinfo_index_category_sort,
                                's', VAL_BIB1, ord)== ZEBRA_OK)
        return ZEBRA_OK;
    if (zebra_attr_list_get_ord(zh, sortAttributes->list,
                                zinfo_index_category_sort,
                                'S', VAL_BIB1, ord)== ZEBRA_OK)
        return ZEBRA_OK;
    return ZEBRA_FAIL;
}


/*
 * Local variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */

