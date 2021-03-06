/* This file is part of the Zebra server.
   Copyright (C) Index Data

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
#include <assert.h>
#include <idzebra/util.h>
#include <rset.h>

static RSFD r_open(RSET ct, int flag);
static void r_close(RSFD rfd);
static void r_delete(RSET ct);
static void r_pos(RSFD rfd, double *current, double *total);
static int r_read(RSFD rfd, void *buf, TERMID *term);

static const struct rset_control control =
{
    "null",
    r_delete,
    rset_get_one_term,
    r_open,
    r_close,
    0, /* no forward */
    r_pos,
    r_read,
    rset_no_write,
};

RSET rset_create_null(NMEM nmem, struct rset_key_control *kcontrol,
                      TERMID term)
{
    RSET rnew = rset_create_base(&control, nmem, kcontrol, 0, term, 0, 0);
    rnew->priv = 0;
    return rnew;
}

static RSFD r_open(RSET ct, int flag)
{
    RSFD rfd;
    if (flag & RSETF_WRITE)
    {
        yaz_log (YLOG_FATAL, "NULL set type is read-only");
        return NULL;
    }
    rfd = rfd_create_base(ct);
    rfd->priv = 0;
    return rfd;
}

static void r_close(RSFD rfd)
{
}

static void r_delete(RSET ct)
{
}

static void r_pos(RSFD rfd, double *current, double *total)
{
    assert(rfd);
    assert(current);
    assert(total);
    *total = 0;
    *current = 0;
}

static int r_read(RSFD rfd, void *buf, TERMID *term)
{
    if (term)
        *term = 0;
    return 0;
}

/*
 * Local variables:
 * c-basic-offset: 4
 * c-file-style: "Stroustrup"
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */

