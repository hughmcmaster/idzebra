/* This file is part of the Zebra server.
   Copyright (C) 1994-2011 Index Data

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

#if HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <string.h>

#include <idzebra/data1.h>
#include <yaz/log.h>

#include <string.h>

/*
 * Search for a token in the supplied string up to the supplied list of stop characters or EOL
 * At the end, return the character causing the break and fill pTokenBuffer with the token string so far
 * After the scan, *pPosInBuffer will point to the next character after the one causing the break and
 *                 pTokenBuffer will contain the actual token
 */
char data1_ScanNextToken(char* pBuffer,
                         char** pPosInBuffer,
                         char* pBreakChars,
                         char* pWhitespaceChars,
                         char* pTokenBuffer)
{
    char* pBuff = pTokenBuffer;
    *pBuff = '\0';

    while ( **pPosInBuffer )
    {
        if ( strchr(pBreakChars,**pPosInBuffer) != NULL )
        {
            /* Current character is a break character */
            *pBuff++ = '\0';
            return *((*pPosInBuffer)++);
        }
        else
        {
            if ( strchr(pWhitespaceChars, **pPosInBuffer) != NULL )
                (*pPosInBuffer)++;
            else
                *pBuff++ = *((*pPosInBuffer)++);
        }
    }

    *pBuff++ = *((*pPosInBuffer)++);
    return(**pPosInBuffer);
}

/*
 * Attempt to find a string value given the specified tagpath
 *
 * Need to make this safe by passing in a buffer.....
 *
 */
char *data1_getNodeValue(data1_node* node, char* pTagPath)
{
    data1_node* n = NULL;

    n = data1_LookupNode(node, pTagPath );

    if ( n )
    {
        /* n should be a tag node with some data under it.... */
        if ( n->child )
        {
            if ( n->child->which == DATA1N_data )
            {
                return n->child->u.data.data;
            }
            else
            {
                yaz_log(YLOG_WARN,"Attempting to lookup data for tagpath: Child node is not a data node");
            }
        }
        else
        {
            yaz_log(YLOG_WARN,"Found a node matching the tagpath, but it has no child data nodes");
        }
    }
    else
    {
        yaz_log(YLOG_WARN,"Unable to lookup a node on the specified tag path");
    }

    return "";
}


/* Max length of a tag */
#define MAX_TAG_SIZE 50

/*
 * data1_LookupNode : Try and find a node as specified by a tagpath
 */
data1_node *data1_LookupNode(data1_node* node, char* pTagPath)
{
    /* Node matching the pattern in the tagpath */
    data1_node* matched_node = NULL;

    /* Current Child node as we search for nodes matching the pattern in the tagpath */
    data1_node* current_child = node->child;

    /* Current position in string */
    char* pCurrCharInPath = pTagPath;

    /* Work buffer */
    char Buffer[MAX_TAG_SIZE];

    /* The tag type of this node */
    int iTagType = 0;

    /* for non string tags, the tag value */
    int iTagValue = 0;

    /* for string tags, the tag value */
    char StringTagVal[MAX_TAG_SIZE];

    /* Which occurence of that tag under this node */
    int iOccurences=0;

    /* Character causing a break */
    char sepchr = '\0';
    Buffer[0] = '\0';
    StringTagVal[0] = '\0';

    sepchr = data1_ScanNextToken(pTagPath, &pCurrCharInPath, ",[(."," ", Buffer);

    if ( sepchr == '[' )
    {
        /* Next component in node value is [ TagType, TagVal, TagOccurence ] */
        sepchr = data1_ScanNextToken(pTagPath, &pCurrCharInPath, ","," ", Buffer);
        iTagType = atoi(Buffer);

        /* Occurence is optional... */
        sepchr = data1_ScanNextToken(pTagPath, &pCurrCharInPath, ",]."," ", Buffer);

        if ( iTagType == 3 )
            strcpy(StringTagVal,Buffer);
        else
            iTagValue = atoi(Buffer);

        /* If sepchar was a ',' there should be an instance */
        if ( sepchr == ',' )
        {
            sepchr = data1_ScanNextToken(pTagPath, &pCurrCharInPath, "]."," ", Buffer);
            iOccurences = atoi(Buffer);
        }

        if ( sepchr == ']' )
        {
            /* See if we can scan the . for the next component or the end of the line... */
            sepchr = data1_ScanNextToken(pTagPath, &pCurrCharInPath, "."," ", Buffer);
        }
        else
        {
            yaz_log(YLOG_FATAL,"Node does not end with a ]");
            /* Fatal Error */
            return(NULL);
        }
    }
    else
    {
        /* We have a TagName so Read up to ( or . or EOL */
        iTagType = 3;
        strcpy(StringTagVal,Buffer);

        if ( sepchr == '(' )
        {
            /* Read the occurence */
            sepchr = data1_ScanNextToken(pTagPath, &pCurrCharInPath, ")"," ", Buffer);
            iOccurences = atoi(Buffer);

            /* See if we can find the . at the end of this clause */
            sepchr = data1_ScanNextToken(pTagPath, &pCurrCharInPath, "."," ", Buffer);
        }

    }

    yaz_log(YLOG_DEBUG,"search node for child like [%d,%d,%s,%d]",iTagType,iTagValue,StringTagVal,iOccurences);


    /* OK.. We have extracted tagtype, Value and Occurence, see if we can find a node */
    /* Under the current parent matching that description                             */

    while ( ( current_child ) && ( matched_node == NULL ) )
    {
        if ( current_child->which == DATA1N_tag )
        {
            if ( iTagType == 3 )
            {
                if ( ( current_child->u.tag.element == NULL ) &&
                     ( strcmp(current_child->u.tag.tag, StringTagVal) == 0 ) )
                {
                    if ( iOccurences )
                    {
                        /* Everything matched, but not yet found the
                           right occurence of the given tag */
                        iOccurences--;
                    }
                    else
                    {
                        /* We have matched a string tag... Is there more to
                           process? */
                        matched_node = current_child;
                    }
                }
            }
            else /* Attempt to match real element */
            {
                yaz_log(YLOG_WARN,"Non string tag matching not yet implemented");
            }
        }
        current_child = current_child->next;
    }


    /* If there is more... Continue */
    if ( ( sepchr == '.' ) && ( matched_node ) )
    {
        return data1_LookupNode(matched_node, pCurrCharInPath);
    }
    else
    {
        return matched_node;
    }
}

/**
   \brief Count the number of occurences of the last instance on a tagpath.
   \param node : The root of the tree we wish to look for occurences in
   \param pTagPath : The tagpath we want to count the occurences of...
*/
int data1_CountOccurences(data1_node* node, char* pTagPath)
{
    int iRetVal = 0;
    data1_node* n = NULL;
    data1_node* pParent = NULL;

    n = data1_LookupNode(node, pTagPath );


    if ( ( n ) &&
         ( n->which == DATA1N_tag ) &&
         ( n->parent ) )
    {
        data1_node* current_child;
        pParent = n->parent;

        for ( current_child = pParent->child;
              current_child;
              current_child = current_child->next )
        {
            if ( current_child->which == DATA1N_tag )
            {
                if ( current_child->u.tag.element == NULL )
                {
                    if ( ( n->u.tag.tag ) &&
                         ( current_child->u.tag.tag ) &&
                         ( strcmp(current_child->u.tag.tag, n->u.tag.tag) == 0 ) )
                    {
                        iRetVal++;
                    }
                }
                else if ( current_child->u.tag.element == n->u.tag.element )
                {
                    /* Hmmm... Is the above right for non string tags???? */
                    iRetVal++;
                }
            }
        }
    }

    return iRetVal;
}
/*
 * Local variables:
 * c-basic-offset: 4
 * c-file-style: "Stroustrup"
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */

