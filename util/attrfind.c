/* $Id: attrfind.c,v 1.1 2006-05-19 13:49:38 adam Exp $
   Copyright (C) 2005-2006
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

#include <attrfind.h>

void attr_init_APT(AttrType *src, Z_AttributesPlusTerm *zapt, int type)
{
    src->attributeList = zapt->attributes->attributes;
    src->num_attributes = zapt->attributes->num_attributes;
    src->type = type;
    src->major = 0;
    src->minor = 0;
}

void attr_init_AttrList(AttrType *src, Z_AttributeList *list, int type)
{
    src->attributeList = list->attributes;
    src->num_attributes = list->num_attributes;
    src->type = type;
    src->major = 0;
    src->minor = 0;
}

int attr_find_ex(AttrType *src, oid_value *attributeSetP, 
		 const char **string_value)
{
    int num_attributes;

    num_attributes = src->num_attributes;
    while (src->major < num_attributes)
    {
        Z_AttributeElement *element;

        element = src->attributeList[src->major];
        if (src->type == *element->attributeType)
        {
            switch (element->which) 
            {
            case Z_AttributeValue_numeric:
                ++(src->major);
                if (element->attributeSet && attributeSetP)
                {
                    oident *attrset;

                    attrset = oid_getentbyoid(element->attributeSet);
                    *attributeSetP = attrset->value;
                }
                return *element->value.numeric;
                break;
            case Z_AttributeValue_complex:
                if (src->minor >= element->value.complex->num_list)
                    break;
                if (element->attributeSet && attributeSetP)
                {
                    oident *attrset;
                    
                    attrset = oid_getentbyoid(element->attributeSet);
                    *attributeSetP = attrset->value;
                }
                if (element->value.complex->list[src->minor]->which ==  
                    Z_StringOrNumeric_numeric)
                {
                    ++(src->minor);
                    return
                        *element->value.complex->list[src->minor-1]->u.numeric;
                }
                else if (element->value.complex->list[src->minor]->which ==  
                         Z_StringOrNumeric_string)
                {
                    if (!string_value)
                        break;
                    ++(src->minor);
                    *string_value = 
                        element->value.complex->list[src->minor-1]->u.string;
                    return -2;
                }
                else
                    break;
            default:
                assert(0);
            }
        }
        ++(src->major);
    }
    return -1;
}

int attr_find(AttrType *src, oid_value *attributeSetP)
{
    return attr_find_ex(src, attributeSetP, 0);
}


/*
 * Local variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */
