/*
 * Copyright (C) 1994, Index Data I/S 
 * All rights reserved.
 * Sebastian Hammer, Adam Dickmeiss
 *
 * $Log: rootblk.c,v $
 * Revision 1.1  1994-09-26 16:08:00  quinn
 * Most of the functionality in place.
 *
 */

/*
 * Read and write the blocktype header.
 */

#include <isam.h>
#include "rootblk.h"

int is_rb_write(isam_blocktype *ib, is_type_header *hd)
{
    int pt = 0, ct = 0, towrite;

    while ((towrite = sizeof(*hd) - pt) > 0)
    {
    	if (towrite > bf_blocksize(ib->bf))
    		towrite = bf_blocksize(ib->bf);
    	if (bf_write(ib->bf, ct, 0, towrite, (char *)hd + pt) < 0)
	    return -1;
	pt += bf_blocksize(ib->bf);
	ct++;
    }
    return ct;
}

int is_rb_read(isam_blocktype *ib, is_type_header *hd)
{
    int pt = 0, ct = 0, rs, toread;

    while ((toread = sizeof(*hd) - pt) > 0)
    {
	if (toread > bf_blocksize(ib->bf))
	    toread = bf_blocksize(ib->bf);
    	if ((rs = bf_read(ib->bf, ct, 0, toread, (char*)hd + pt)) <= 0)
	    return rs;
	pt += bf_blocksize(ib->bf);
	ct++;
    }
    return ct;
}
