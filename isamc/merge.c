/*
 * Copyright (c) 1996, Index Data.
 * See the file LICENSE for details.
 * Sebastian Hammer, Adam Dickmeiss
 *
 * $Log: merge.c,v $
 * Revision 1.2  1996-11-01 13:36:46  adam
 * New element, max_blocks_mem, that control how many blocks of max size
 * to store in memory during isc_merge.
 * Function isc_merge now ignoreds delete/update of identical keys and
 * the proper blocks are then non-dirty and not written in flush_blocks.
 *
 * Revision 1.1  1996/11/01  08:59:15  adam
 * First version of isc_merge that supports update/delete.
 *
 */

#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>

#include <log.h>
#include "isamc-p.h"

struct isc_merge_block {
    int offset;       /* offset in r_buf */
    int block;        /* block number of file (0 if none) */
    int dirty;        /* block is different from that on file */
};

static void flush_blocks (ISAMC is, struct isc_merge_block *mb, int ptr,
                          char *r_buf, int *firstpos, int cat, int last)
{
    int i;

    for (i = 0; i<ptr; i++)
    {
        /* skip rest if not dirty */
        if (!mb[i].dirty)
        {
            assert (mb[i].block);
            if (!*firstpos)
                *firstpos = mb[i].block;
            if (is->method->debug > 2)
                logf (LOG_LOG, "isc: skip block %d %d", cat, mb[i].block);
            ++(is->files[cat].no_skip_writes);
            continue;
        }
        /* consider this block number */

        if (!mb[i].block) 
            mb[i].block = isc_alloc_block (is, cat);
        if (!*firstpos)
            *firstpos = mb[i].block;

        /* consider next block pointer */
        if (last && i == ptr-1)
            mb[i+1].block = 0;
        else if (!mb[i+1].block)       
            mb[i+1].block = isc_alloc_block (is, cat);

        /* write block */
        assert (mb[i+1].offset > mb[i].offset);
        isc_write_dblock (is, cat, mb[i].block, r_buf + mb[i].offset,
                          mb[i+1].block, mb[i+1].offset - mb[i].offset);
    }
}

ISAMC_P isc_merge (ISAMC is, ISAMC_P ipos, ISAMC_I data)
{

    char i_item[128], *i_item_ptr;
    int i_more, i_mode, i;

    ISAMC_PP pp; 
    char f_item[128], *f_item_ptr;
    int f_more;
 
    struct isc_merge_block mb[100];
    int firstpos = 0;
    int cat = 0;
    char r_item_buf[128]; /* temporary result output */
    char *r_buf;          /* block with resulting data */
    int r_offset = 0;     /* current offset in r_buf */
    int ptr = 0;          /* pointer */
    void *r_clientData;   /* encode client data */

    r_clientData = (*is->method->code_start)(ISAMC_ENCODE);
    r_buf = is->merge_buf + ISAMC_BLOCK_OFFSET;

    pp = isc_pp_open (is, ipos);
    /* read first item from file. make sure f_more indicates no boundary */
    f_item_ptr = f_item;
    f_more = isc_read_item (pp, &f_item_ptr);
    if (f_more > 0)
        f_more = 1;
    cat = pp->cat;

    if (is->method->debug > 1)
        logf (LOG_LOG, "isc: isc_merge begin %d %d", cat, pp->pos);

    /* read first item from i */
    i_item_ptr = i_item;
    i_more = (*data->read_item)(data->clientData, &i_item_ptr, &i_mode);

    mb[ptr].block = pp->pos;     /* is zero if no block on disk */
    mb[ptr].dirty = 0;
    mb[ptr].offset = 0;

    while (i_more || f_more)
    {
        char *r_item = r_item_buf;
        int cmp;

        if (f_more > 1)
        {
            /* block to block boundary in the original file. */
            f_more = 1;
            if (cat == pp->cat) 
            {
                /* the resulting output is of the same category as the
                   the original 
                 */
                if (mb[ptr].offset == r_offset)
                {
                    /* the resulting output block is empty. Delete
                       the original (if any)
                     */
                    if (is->method->debug > 3)
                        logf (LOG_LOG, "isc: release A");
                    if (mb[ptr].block)
                        isc_release_block (is, pp->cat, mb[ptr].block);
                    mb[ptr].block = pp->pos;
                    mb[ptr].dirty = 0;
                }
                else
                {
                    /* indicate new boundary based on the original file */
                    mb[++ptr].block = pp->pos;
                    mb[ptr].dirty = 0;
                    mb[ptr].offset = r_offset;
#if 0
                    if (r_item && cat == is->max_cat)
                    {
                        /* We are dealing with block(s) of max size. Block(s)
                           will be flushed. Note: the block(s) are surely not
                           the last one(s).
                         */
                        if (is->method->debug > 2)
                            logf (LOG_LOG, "isc: flush A %d sections", ptr);
                        flush_blocks (is, mb, ptr-1, r_buf, &firstpos, cat, 0);
                        ptr = 0;
                        mb[ptr].block = pp->pos;
                        mb[ptr].dirty = 0;
                        mb[ptr].offset = 0;
                    }
#endif
                }
            }
        }
        if (!f_more)
            cmp = -1;
        else if (!i_more)
            cmp = 1;
        else
            cmp = (*is->method->compare_item)(i_item, f_item);
        if (cmp == 0)                   /* insert i=f */
        {
            if (!i_mode)   /* delete item? */
            {
                /* move i */
                i_item_ptr = i_item;
                i_more = (*data->read_item)(data->clientData, &i_item_ptr,
                                           &i_mode);
                /* it next input item the same as current except
                   for the delete flag? */
                cmp = (*is->method->compare_item)(i_item, f_item);
                if (!cmp && i_mode)
                {
                    /* yes! insert as if it was an insert only */
                    memcpy (r_item, i_item, i_item_ptr - i_item);
                    i_item_ptr = i_item;
                    i_more = (*data->read_item)(data->clientData, &i_item_ptr,
                                                &i_mode);
                }
                else
                {
                    /* no! delete the item */
                    r_item = NULL;
                    mb[ptr].dirty = 1;
                }
            }
            else
            {
                memcpy (r_item, f_item, f_item_ptr - f_item);

                /* move i */
                i_item_ptr = i_item;
                i_more = (*data->read_item)(data->clientData, &i_item_ptr,
                                           &i_mode);
            }
            /* move f */
            f_item_ptr = f_item;
            f_more = isc_read_item (pp, &f_item_ptr);
        }
        else if (cmp > 0)               /* insert f */
        {
            memcpy (r_item, f_item, f_item_ptr - f_item);
            /* move f */
            f_item_ptr = f_item;
            f_more = isc_read_item (pp, &f_item_ptr);
        }
        else                            /* insert i */
        {
            if (!i_mode)                /* delete item which isn't there? */
            {
                logf (LOG_FATAL, "Inconsistent register at offset %d",
                                 r_offset);
                abort ();
            }
            memcpy (r_item, i_item, i_item_ptr - i_item);
            mb[ptr].dirty = 1;
            /* move i */
            i_item_ptr = i_item;
            i_more = (*data->read_item)(data->clientData, &i_item_ptr,
                                        &i_mode);
        }
        if (r_item)  /* insert resulting item? */
        {
            char *r_out_ptr = r_buf + r_offset;
            int new_offset;
            int border;

            /* border set to initial fill or block size depending on
               whether we are creating a new one or updating and old one
             */
            if (mb[ptr].block)
                border = mb[ptr].offset + is->method->filecat[cat].bsize
                         -ISAMC_BLOCK_OFFSET;
            else
                border = mb[ptr].offset + is->method->filecat[cat].ifill
                         -ISAMC_BLOCK_OFFSET;

            (*is->method->code_item)(ISAMC_ENCODE, r_clientData,
                                     &r_out_ptr, &r_item);
            new_offset = r_out_ptr - r_buf; 

            if (border >= r_offset && border < new_offset)
            {
                if (is->method->debug > 2)
                    logf (LOG_LOG, "isc: border %d %d", ptr, border);
                /* Max size of current block category reached ...
                   make new virtual block entry */
                mb[++ptr].block = 0;
                mb[ptr].dirty = 1;
                mb[ptr].offset = r_offset;
                if (cat == is->max_cat && ptr >= is->method->max_blocks_mem)
                {
                    /* We are dealing with block(s) of max size. Block(s)
                       except one will be flushed. Note: the block(s) are
                       surely not the last one(s).
                     */
                    if (is->method->debug > 2)
                        logf (LOG_LOG, "isc: flush B %d sections", ptr-1);
                    flush_blocks (is, mb, ptr-1, r_buf, &firstpos, cat, 0);

                    mb[0].block = mb[ptr-1].block;
                    mb[0].dirty = mb[ptr-1].dirty;
                    memcpy (r_buf, r_buf + mb[ptr-1].offset,
                            mb[ptr].offset - mb[ptr-1].offset);
                    mb[0].offset = 0;

                    mb[1].block = mb[ptr].block;
                    mb[1].dirty = mb[0].dirty;
                    mb[1].offset = mb[ptr].offset - mb[ptr-1].offset;
                    memcpy (r_buf + mb[1].offset, r_buf + r_offset,
                            new_offset - r_offset);
                    new_offset = (new_offset - r_offset) + mb[1].offset;
                    ptr = 1;
               }
            }
            r_offset = new_offset;
        }
        if (cat < is->max_cat && ptr >= is->method->filecat[cat].mblocks)
        {
            /* Max number blocks in current category reached ->
               must switch to next category (with larger block size) 
            */
            int j = 0;

            (is->files[cat].no_remap)++;
            /* delete all original block(s) read so far */
            for (i = 0; i < ptr; i++)
                if (mb[i].block)
                    isc_release_block (is, pp->cat, mb[i].block);
            /* also delete all block to be read in the future */
            pp->deleteFlag = 1;

            /* remap block offsets */
            assert (mb[j].offset == 0);
            cat++;
            mb[j].dirty = 1;
            mb[j].block = 0;
            for (i = 1; i < ptr; i++)
            {
                int border = is->method->filecat[cat].ifill -
                         ISAMC_BLOCK_OFFSET + mb[j].offset;
                if (is->method->debug > 3)
                    logf (LOG_LOG, "isc: remap %d border=%d", i, border);
                if (mb[i+1].offset > border && mb[i].offset <= border)
                {
                    if (is->method->debug > 3)
                        logf (LOG_LOG, "isc:  to %d %d", j, mb[i].offset);
                    mb[++j].dirty = 1;
                    mb[j].block = 0;
                    mb[j].offset = mb[i].offset;
                }
            }
            if (is->method->debug > 2)
                logf (LOG_LOG, "isc: remap from %d to %d sections to cat %d",
                      ptr, j, cat);
            ptr = j;
        }
    }
    if (mb[ptr].offset < r_offset)
    {   /* make the final boundary offset */
        mb[++ptr].dirty = 1;         /* ignored by flush_blocks */
        mb[ptr].block = 0;         /* ignored by flush_blocks */
        mb[ptr].offset = r_offset;
    }
    else
    {   /* empty output. Release last block if any */
        if (cat == pp->cat && mb[ptr].block)
        {
            if (is->method->debug > 3)
                logf (LOG_LOG, "isc: release C");
            isc_release_block (is, pp->cat, mb[ptr].block);
            mb[ptr].block = 0;
            mb[ptr].dirty = 1;
        }
    }

    if (is->method->debug > 2)
        logf (LOG_LOG, "isc: flush C, %d sections", ptr);

    /* flush rest of block(s) in r_buf */
    flush_blocks (is, mb, ptr, r_buf, &firstpos, cat, 1);

    (*is->method->code_stop)(ISAMC_ENCODE, r_clientData);
    if (!firstpos)
        cat = 0;
    if (is->method->debug > 1)
        logf (LOG_LOG, "isc: isc_merge return %d %d", cat, firstpos);
    return cat + firstpos * 8;
}

