/* $Id: trunc.c,v 1.39 2004-08-25 09:18:06 adam Exp $
   Copyright (C) 1995,1996,1997,1998,1999,2000,2001,2002,2003,2004
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


#include <stdio.h>
#include <assert.h>

#include "index.h"
#include <rstemp.h>
#include <rsnull.h>
#include <rsisams.h>
#include <rsisamc.h>
#include <rsisamb.h>
#include <rsmultior.h>

struct trunc_info {
    int  *ptr;
    int  *indx;
    char **heap;
    int  heapnum;
    int  (*cmp)(const void *p1, const void *p2);
    int  keysize;
    char *swapbuf;
    char *tmpbuf;
    char *buf;
};

static void heap_swap (struct trunc_info *ti, int i1, int i2)
{
    int swap;

    swap = ti->ptr[i1];
    ti->ptr[i1] = ti->ptr[i2];
    ti->ptr[i2] = swap;
}

static void heap_delete (struct trunc_info *ti)
{
    int cur = 1, child = 2;

    heap_swap (ti, 1, ti->heapnum--);
    while (child <= ti->heapnum) {
        if (child < ti->heapnum &&
            (*ti->cmp)(ti->heap[ti->ptr[child]],
                       ti->heap[ti->ptr[1+child]]) > 0)
            child++;
        if ((*ti->cmp)(ti->heap[ti->ptr[cur]],
                       ti->heap[ti->ptr[child]]) > 0)
        {
            heap_swap (ti, cur, child);
            cur = child;
            child = 2*cur;
        }
        else
            break;
    }
}

static void heap_insert (struct trunc_info *ti, const char *buf, int indx)
{
    int cur, parent;

    cur = ++(ti->heapnum);
    memcpy (ti->heap[ti->ptr[cur]], buf, ti->keysize);
    ti->indx[ti->ptr[cur]] = indx;
    parent = cur/2;
    while (parent && (*ti->cmp)(ti->heap[ti->ptr[parent]],
                                ti->heap[ti->ptr[cur]]) > 0)
    {
        heap_swap (ti, cur, parent);
        cur = parent;
        parent = cur/2;
    }
}

static struct trunc_info *heap_init (int size, int key_size,
				     int (*cmp)(const void *p1,
						const void *p2))
{
    struct trunc_info *ti = (struct trunc_info *) xmalloc (sizeof(*ti));
    int i;

    ++size;
    ti->heapnum = 0;
    ti->keysize = key_size;
    ti->cmp = cmp;
    ti->indx = (int *) xmalloc (size * sizeof(*ti->indx));
    ti->heap = (char **) xmalloc (size * sizeof(*ti->heap));
    ti->ptr = (int *) xmalloc (size * sizeof(*ti->ptr));
    ti->swapbuf = (char *) xmalloc (ti->keysize);
    ti->tmpbuf = (char *) xmalloc (ti->keysize);
    ti->buf = (char *) xmalloc (size * ti->keysize);
    for (i = size; --i >= 0; )
    {
        ti->ptr[i] = i;
        ti->heap[i] = ti->buf + ti->keysize * i;
    }
    return ti;
}

static void heap_close (struct trunc_info *ti)
{
    xfree (ti->ptr);
    xfree (ti->indx);
    xfree (ti->heap);
    xfree (ti->swapbuf);
    xfree (ti->tmpbuf);
    xfree (ti->buf);
    xfree (ti);
}

static RSET rset_trunc_r (ZebraHandle zi, const char *term, int length,
                          const char *flags, ISAMS_P *isam_p, int from, int to,
                          int merge_chunk, int preserve_position,
                          int term_type)
{
    RSET result; 
    RSFD result_rsfd;
    int nn = 0;

    /*
    rset_temp_parms parms;
    parms.cmp = key_compare_it;
    parms.key_size = sizeof(struct it_key);
    parms.temp_path = res_get (zi->res, "setTmpDir");
    result = rset_create (rset_kind_temp, &parms);
    */
    result=rstemp_create(NULL, /* FIXME - use a proper nmem */
            sizeof(struct it_key), key_compare_it, 
            res_get (zi->res, "setTmpDir"));
    result_rsfd = rset_open (result, RSETF_WRITE);

    if (to - from > merge_chunk)
    {
        RSFD *rsfd;
        RSET *rset;
        int i, i_add = (to-from)/merge_chunk + 1;
        struct trunc_info *ti;
        int rscur = 0;
        int rsmax = (to-from)/i_add + 1;
        
        rset = (RSET *) xmalloc (sizeof(*rset) * rsmax);
        rsfd = (RSFD *) xmalloc (sizeof(*rsfd) * rsmax);
        
        for (i = from; i < to; i += i_add)
        {
            if (i_add <= to - i)
                rset[rscur] = rset_trunc_r (zi, term, length, flags,
				            isam_p, i, i+i_add,
                                            merge_chunk, preserve_position,
                                            term_type);
            else
                rset[rscur] = rset_trunc_r (zi, term, length, flags,
                                            isam_p, i, to,
                                            merge_chunk, preserve_position,
                                            term_type);
            rscur++;
        }
        ti = heap_init (rscur, sizeof(struct it_key), key_compare_it);
        for (i = rscur; --i >= 0; )
        {
            rsfd[i] = rset_open (rset[i], RSETF_READ);
            if (rset_read (rset[i], rsfd[i], ti->tmpbuf))
                heap_insert (ti, ti->tmpbuf, i);
            else
            {
                rset_close (rset[i], rsfd[i]);
                rset_delete (rset[i]);
            }
        }
        while (ti->heapnum)
        {
            int n = ti->indx[ti->ptr[1]];

            rset_write (result, result_rsfd, ti->heap[ti->ptr[1]]);
            nn++;

            while (1)
            {
                if (!rset_read (rset[n], rsfd[n], ti->tmpbuf))
                {
                    heap_delete (ti);
                    rset_close (rset[n], rsfd[n]);
                    rset_delete (rset[n]);
                    break;
                }
                if ((*ti->cmp)(ti->tmpbuf, ti->heap[ti->ptr[1]]) > 1)
                {
                    heap_delete (ti);
                    heap_insert (ti, ti->tmpbuf, n);
                    break;
                }
            }
        }
        xfree (rset);
        xfree (rsfd);
        heap_close (ti);
    }
    else if (zi->reg->isamc)
    {
        ISAMC_PP *ispt;
        int i;
        struct trunc_info *ti;

        ispt = (ISAMC_PP *) xmalloc (sizeof(*ispt) * (to-from));

        ti = heap_init (to-from, sizeof(struct it_key),
                        key_compare_it);
        for (i = to-from; --i >= 0; )
        {
            ispt[i] = isc_pp_open (zi->reg->isamc, isam_p[from+i]);
            if (isc_pp_read (ispt[i], ti->tmpbuf))
                heap_insert (ti, ti->tmpbuf, i);
            else
                isc_pp_close (ispt[i]);
        }
        while (ti->heapnum)
        {
            int n = ti->indx[ti->ptr[1]];

            rset_write (result, result_rsfd, ti->heap[ti->ptr[1]]);
            nn++;
            if (preserve_position)
            {
                heap_delete (ti);
                if (isc_pp_read (ispt[n], ti->tmpbuf))
                    heap_insert (ti, ti->tmpbuf, n);
                else
                    isc_pp_close (ispt[n]);
            }
            else
            {
                while (1)
                {
                    if (!isc_pp_read (ispt[n], ti->tmpbuf))
                    {
                        heap_delete (ti);
                        isc_pp_close (ispt[n]);
                        break;
                    }
                    if ((*ti->cmp)(ti->tmpbuf, ti->heap[ti->ptr[1]]) > 1)
                    {
                        heap_delete (ti);
                        heap_insert (ti, ti->tmpbuf, n);
                        break;
                    }
                }
            }
        }
        heap_close (ti);
        xfree (ispt);
    }
    else if (zi->reg->isams)
    {
        ISAMS_PP *ispt;
        int i;
        struct trunc_info *ti;
        int nn = 0;

        ispt = (ISAMS_PP *) xmalloc (sizeof(*ispt) * (to-from));

        ti = heap_init (to-from, sizeof(struct it_key),
                        key_compare_it);
        for (i = to-from; --i >= 0; )
        {
            ispt[i] = isams_pp_open (zi->reg->isams, isam_p[from+i]);
            if (isams_pp_read (ispt[i], ti->tmpbuf))
                heap_insert (ti, ti->tmpbuf, i);
            else
                isams_pp_close (ispt[i]);
        }
        while (ti->heapnum)
        {
            int n = ti->indx[ti->ptr[1]];

            rset_write (result, result_rsfd, ti->heap[ti->ptr[1]]);
            nn++;
            while (1)
            {
                if (!isams_pp_read (ispt[n], ti->tmpbuf))
                {
                    heap_delete (ti);
                    isams_pp_close (ispt[n]);
                    break;
                }
                if ((*ti->cmp)(ti->tmpbuf, ti->heap[ti->ptr[1]]) > 1)
                {
                    heap_delete (ti);
                    heap_insert (ti, ti->tmpbuf, n);
                    break;
                }
            }
        }
        heap_close (ti);
        xfree (ispt);
    }
    else if (zi->reg->isamb)
    {
        ISAMB_PP *ispt;
        int i;
        struct trunc_info *ti;

        ispt = (ISAMB_PP *) xmalloc (sizeof(*ispt) * (to-from));

        ti = heap_init (to-from, sizeof(struct it_key),
                        key_compare_it);
        for (i = to-from; --i >= 0; )
        {
	    if (isam_p[from+i]) {
                ispt[i] = isamb_pp_open (zi->reg->isamb, isam_p[from+i]);
                if (isamb_pp_read (ispt[i], ti->tmpbuf))
                    heap_insert (ti, ti->tmpbuf, i);
                else
                    isamb_pp_close (ispt[i]);
	    }
        }
        while (ti->heapnum)
        {
            int n = ti->indx[ti->ptr[1]];

            rset_write (result, result_rsfd, ti->heap[ti->ptr[1]]);
            nn++;

            if (preserve_position)
            {
                heap_delete (ti);
                if (isamb_pp_read (ispt[n], ti->tmpbuf))
                    heap_insert (ti, ti->tmpbuf, n);
                else
                    isamb_pp_close (ispt[n]);
            }
            else
            {
                while (1)
                {
                    if (!isamb_pp_read (ispt[n], ti->tmpbuf))
                    {
                        heap_delete (ti);
                        isamb_pp_close (ispt[n]);
                        break;
                    }
                    if ((*ti->cmp)(ti->tmpbuf, ti->heap[ti->ptr[1]]) > 1)
                    {
                        heap_delete (ti);
                        heap_insert (ti, ti->tmpbuf, n);
                        break;
                    }
                }
            }
        }
        heap_close (ti);
        xfree (ispt);
    }
    else
        logf (LOG_WARN, "Unknown isam set in rset_trunc_r");

    rset_close (result, result_rsfd);
    return result;
}

static int isams_trunc_cmp (const void *p1, const void *p2)
{
    ISAMS_P i1 = *(ISAMS_P*) p1;
    ISAMS_P i2 = *(ISAMS_P*) p2;

    if (i1 > i2)
        return 1;
    else if (i1 < i2)
	return -1;
    return 0;
}

static int isamc_trunc_cmp (const void *p1, const void *p2)
{
    ISAMC_P i1 = *(ISAMC_P*) p1;
    ISAMC_P i2 = *(ISAMC_P*) p2;
    zint d;

    d = (isc_type (i1) - isc_type (i2));
    if (d == 0)
        d = isc_block (i1) - isc_block (i2);
    if (d > 0)
	return 1;
    else if (d < 0)
	return -1;
    return 0;
}

RSET rset_trunc (ZebraHandle zi, ISAMS_P *isam_p, int no,
		 const char *term, int length, const char *flags,
                 int preserve_position, int term_type)
{
    logf (LOG_DEBUG, "rset_trunc no=%d", no);
    if (no < 1)
	return rsnull_create (NULL); /* FIXME - use a proper nmem */
    if (zi->reg->isams)
    {
        if (no == 1)
            return rsisams_create(NULL, /* FIXME - use some nmem */
                    sizeof(struct it_key), key_compare_it,
                    zi->reg->isams, *isam_p);
        /*
        {
            rset_isams_parms parms;

            parms.pos = *isam_p;
            parms.is = zi->reg->isams;
            return rset_create (rset_kind_isams, &parms);
        }
        */
        qsort (isam_p, no, sizeof(*isam_p), isams_trunc_cmp);
    }
    else if (zi->reg->isamc)
    {
        if (no == 1)
            return rsisamc_create(NULL, /* FIXME - use some nmem */
                    sizeof(struct it_key), key_compare_it,
                    zi->reg->isamc, *isam_p);
        /*
        {
            rset_isamc_parms parms;

            parms.key_size = sizeof(struct it_key);
            parms.cmp = key_compare_it;
            parms.pos = *isam_p;
            parms.is = zi->reg->isamc;
            return rset_create (rset_kind_isamc, &parms);
        }
        */

#if 0 /* NEW_TRUNC */ /* FIXME - Use the new multi_or instead !! */
        else if (no < 10000)
        {
            rset_m_or_parms parms;

            parms.key_size = sizeof(struct it_key);
            parms.cmp = key_compare_it;
            parms.isc = zi->reg->isamc;
            parms.isam_positions = isam_p;
            parms.no_isam_positions = no;
            parms.no_save_positions = 100000;
            return rset_create (rset_kind_m_or, &parms);
        }
#endif
        qsort (isam_p, no, sizeof(*isam_p), isamc_trunc_cmp);
    }
    else if (zi->reg->isamb)
    {
        if (no == 1)
            return rsisamb_create(NULL, /* FIXME - use some nmem */
                    sizeof(struct it_key), key_compare_it,
                    zi->reg->isamb, *isam_p);
        /*
        {
            rset_isamb_parms parms;
            parms.key_size = sizeof(struct it_key);
            parms.cmp = key_compare_it;
            parms.pos = *isam_p;
            parms.is = zi->reg->isamb;
            return rset_create (rset_kind_isamb, &parms);
        }
        */
#if 1
        else if (no <10000 ) /* FIXME - hardcoded number */
        {
            RSET r;
            RSET *rsets=xmalloc(no*sizeof(RSET)); /* use nmem! */
            int i;
            for (i=0;i<no;i++)
                rsets[i]=rsisamb_create(NULL, /* */
                    sizeof(struct it_key), key_compare_it,
                    zi->reg->isamb, isam_p[i] );
            r=rsmultior_create( NULL, /* FIXME - use some nmem */
                      sizeof(struct it_key), key_compare_it, 
                      no, rsets);
            xfree(rsets);
            return r;
            /*
            rset_multior_parms m_parms;
            rset_isamb_parms b_parms;
            int i;
            m_parms.key_size = sizeof(struct it_key);
            m_parms.cmp = key_compare_it;
            m_parms.no_rsets=no;
            m_parms.rsets=xmalloc(sizeof(*m_parms.rsets)*no);
            b_parms.key_size = sizeof(struct it_key);
            b_parms.cmp = key_compare_it;
            b_parms.is = zi->reg->isamb;
            for (i=0;i<no;i++)
            {
                b_parms.pos = isam_p[i];
                m_parms.rsets[i]=rset_create (rset_kind_isamb, &b_parms);
            }
            return rset_create (rset_kind_multior, &m_parms);
            */
        } /* <10000 - rs_multior */
#endif        
        qsort (isam_p, no, sizeof(*isam_p), isamc_trunc_cmp);
    }
    else
    {
        logf (LOG_WARN, "Unknown isam set in rset_trunc");
	return rsnull_create (NULL); /* FIXME - nmem */
    }
    return rset_trunc_r (zi, term, length, flags, isam_p, 0, no, 100,
                         preserve_position, term_type);
}

