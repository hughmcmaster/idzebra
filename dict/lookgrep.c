/*
 * Copyright (C) 1994, Index Data I/S 
 * All rights reserved.
 * Sebastian Hammer, Adam Dickmeiss
 *
 * $Log: lookgrep.c,v $
 * Revision 1.3  1994-10-05 12:16:50  adam
 * Pagesize is a resource now.
 *
 * Revision 1.2  1994/10/04  12:08:07  adam
 * Some bug fixes and some optimizations.
 *
 * Revision 1.1  1994/10/03  17:23:04  adam
 * First version of dictionary lookup with regular expressions and errors.
 *
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

#include <dfa.h>
#include <dict.h>

typedef unsigned MatchWord;
#define WORD_BITS 32
#define MAX_LENGTH 1024

typedef struct {
  int n;                 /* no of MatchWord needed */
  int range;             /* max no. of errors */
  int fact;              /* (range+1)*n */
  MatchWord *match_mask; /* match_mask */
} MatchContext;

#define INLINE 

static INLINE void set_bit (MatchContext *mc, MatchWord *m, int ch, int state)
{
    int off = state & (WORD_BITS-1);
    int wno = state / WORD_BITS;
  
    m[mc->n * ch + wno] |= 1<<off;
}

static INLINE MatchWord get_bit (MatchContext *mc, MatchWord *m, int ch,
                                 int state)
{
    int off = state & (WORD_BITS-1);
    int wno = state / WORD_BITS;

    return m[mc->n * ch + wno] & (1<<off);
}

static MatchContext *mk_MatchContext (DFA_states *dfas, int range)
{
    MatchContext *mc = xmalloc (sizeof(*mc));
    int s;

    mc->n = (dfas->no+WORD_BITS) / WORD_BITS;
    mc->range = range;
    mc->fact = (range+1)*mc->n;
    mc->match_mask = xcalloc (mc->n, sizeof(*mc->match_mask));

    for (s = 0; s<dfas->no; s++)
        if (dfas->sortarray[s]->rule_no)
            set_bit (mc, mc->match_mask, 0, s);
    return mc;
}

static void rm_MatchContext (MatchContext **mc)
{
    xfree ((*mc)->match_mask);
    xfree (*mc);
    *mc = NULL;
}

static void mask_shift (MatchContext *mc, MatchWord *Rdst, MatchWord *Rsrc,
                   DFA_states *dfas, int ch)
{
    int j, s = 0;
    MatchWord *Rsrc_p = Rsrc, mask;

    for (j = 0; j<mc->n; j++)
        Rdst[j] = 0;
    while (1)
    {
        mask = *Rsrc_p++;
        for (j = 0; j<WORD_BITS/4; j++)
        {
            if (mask & 15)
            {
                if (mask & 1)
                {
                    DFA_state *state = dfas->sortarray[s];
                    int i = state->tran_no;
                    while (--i >= 0)
                        if (ch >= state->trans[i].ch[0] &&
                            ch <= state->trans[i].ch[1])
                            set_bit (mc, Rdst, 0, state->trans[i].to);
                }
                if (mask & 2)
                {
                    DFA_state *state = dfas->sortarray[s+1];
                    int i = state->tran_no;
                    while (--i >= 0)
                        if (ch >= state->trans[i].ch[0] &&
                            ch <= state->trans[i].ch[1])
                            set_bit (mc, Rdst, 0, state->trans[i].to);
                }
                if (mask & 4)
                {
                    DFA_state *state = dfas->sortarray[s+2];
                    int i = state->tran_no;
                    while (--i >= 0)
                        if (ch >= state->trans[i].ch[0] &&
                            ch <= state->trans[i].ch[1])
                            set_bit (mc, Rdst, 0, state->trans[i].to);
                }
                if (mask & 8)
                {
                    DFA_state *state = dfas->sortarray[s+3];
                    int i = state->tran_no;
                    while (--i >= 0)
                        if (ch >= state->trans[i].ch[0] &&
                            ch <= state->trans[i].ch[1])
                            set_bit (mc, Rdst, 0, state->trans[i].to);
                }
            }
            s += 4;
            if (s >= dfas->no)
                return;
            mask >>= 4;
        }
    }
}

static void shift (MatchContext *mc, MatchWord *Rdst, MatchWord *Rsrc,
                   DFA_states *dfas)
{
    int j, s = 0;
    MatchWord *Rsrc_p = Rsrc, mask;
    for (j = 0; j<mc->n; j++)
        Rdst[j] = 0;
    while (1)
    {
        mask = *Rsrc_p++;
        for (j = 0; j<WORD_BITS/4; j++)
        {
            if (mask & 15)
            {
                if (mask & 1)
                {
                    DFA_state *state = dfas->sortarray[s];
                    int i = state->tran_no;
                    while (--i >= 0)
                        set_bit (mc, Rdst, 0, state->trans[i].to);
                }
                if (mask & 2)
                {
                    DFA_state *state = dfas->sortarray[s+1];
                    int i = state->tran_no;
                    while (--i >= 0)
                        set_bit (mc, Rdst, 0, state->trans[i].to);
                }
                if (mask & 4)
                {
                    DFA_state *state = dfas->sortarray[s+2];
                    int i = state->tran_no;
                    while (--i >= 0)
                        set_bit (mc, Rdst, 0, state->trans[i].to);
                }
                if (mask & 8)
                {
                    DFA_state *state = dfas->sortarray[s+3];
                    int i = state->tran_no;
                    while (--i >= 0)
                        set_bit (mc, Rdst, 0, state->trans[i].to);
                }
            }
            s += 4;
            if (s >= dfas->no)
                return;
            mask >>= 4;
        }
    }
}

static void or (MatchContext *mc, MatchWord *Rdst,
                MatchWord *Rsrc1, MatchWord *Rsrc2)
{
    int i;
    for (i = 0; i<mc->n; i++)
        Rdst[i] = Rsrc1[i] | Rsrc2[i];
}

static INLINE int move (MatchContext *mc, MatchWord *Rj1, MatchWord *Rj,
                 Dict_char ch, DFA_states *dfas, MatchWord *Rtmp)
{
    int d;
    MatchWord *Rtmp_2 = Rtmp + mc->n;

    mask_shift (mc, Rj1, Rj, dfas, ch);
    for (d = 1; d <= mc->range; d++)
    {
        or (mc, Rtmp, Rj, Rj1);                         /* 2,3 */
        
        shift (mc, Rtmp_2, Rtmp, dfas);

        mask_shift (mc, Rtmp, Rj+mc->n, dfas, ch);      /* 1 */
                
        or (mc, Rtmp, Rtmp_2, Rtmp);                    /* 1,2,3*/

        Rj1 += mc->n;
        
        or (mc, Rj1, Rtmp, Rj);                         /* 1,2,3,4 */

        Rj += mc->n;
    }
    return 1;

}


static int dict_grep (Dict dict, Dict_ptr ptr, MatchContext *mc,
                      MatchWord *Rj, int pos,
                      int (*userfunc)(Dict_char *name, char *info),
                      Dict_char *prefix, DFA_states *dfas)
{
    int lo, hi, d;
    void *p;
    short *indxp;
    char *info;

    dict_bf_readp (dict->dbf, ptr, &p);
    lo = 0;
    hi = DICT_nodir(p)-1;
    indxp = (short*) ((char*) p+DICT_pagesize(dict)-sizeof(short));    

    while (lo <= hi)
    {
        if (indxp[-lo] > 0)
        {
            /* string (Dict_char *) DICT_EOS terminated */
            /* unsigned char        length of information */
            /* char *               information */
            int j;
            int was_match = 0;
            info = (char*)p + indxp[-lo];
            for (j=0; ; j++)
            {
                Dict_char ch;
                MatchWord *Rj0 =    Rj + j    *mc->fact;
                MatchWord *Rj1 =    Rj + (j+1)*mc->fact;
                MatchWord *Rj_tmp = Rj + (j+2)*mc->fact;

                memcpy (&ch, info+j*sizeof(Dict_char), sizeof(Dict_char));
                prefix[pos+j] = ch;
                if (ch == DICT_EOS)
                {
                    if (was_match)
                        (*userfunc)(prefix, info+(j+1)*sizeof(Dict_char));
                    break;
                }
                move (mc, Rj1, Rj0, ch, dfas, Rj_tmp);
                for (d = mc->n; --d >= 0; )
                    if (Rj1[mc->range*mc->n + d])
                        break;
                if (d < 0)
                    break;
                was_match = 0;
                for (d = mc->n; --d >= 0; )
                    if (Rj1[mc->range*mc->n + d] & mc->match_mask[d])
                    {
                        was_match = 1;
                        break;
                    }
            }
        }
        else
        {
            MatchWord *Rj1 =    Rj+  mc->fact;
            MatchWord *Rj_tmp = Rj+2*mc->fact;
            Dict_char ch;

            /* Dict_ptr             subptr */
            /* Dict_char            sub char */
            /* unsigned char        length of information */
            /* char *               information */
            info = (char*)p - indxp[-lo];
            memcpy (&ch, info+sizeof(Dict_ptr), sizeof(Dict_char));
            prefix[pos] = ch;
            
            move (mc, Rj1, Rj, ch, dfas, Rj_tmp);
            for (d = mc->n; --d >= 0; )
                if (Rj1[mc->range*mc->n + d])
                    break;
            if (d >= 0)
            {
                Dict_ptr subptr;
                if (info[sizeof(Dict_ptr)+sizeof(Dict_char)])
                {
                    for (d = mc->n; --d >= 0; )
                        if (Rj1[mc->range*mc->n + d] & mc->match_mask[d])
                        {
                            prefix[pos+1] = DICT_EOS;
                            (*userfunc)(prefix, info+sizeof(Dict_ptr)+
                                        sizeof(Dict_char));
                            break;
                        }
                }
                memcpy (&subptr, info, sizeof(Dict_ptr));
                if (subptr)
                {
                    dict_grep (dict, subptr, mc, Rj1, pos+1,
                                  userfunc, prefix, dfas);
                    dict_bf_readp (dict->dbf, ptr, &p);
                    indxp = (short*) ((char*) p+DICT_pagesize(dict)
                                      -sizeof(short));
                }
            }
        }
        lo++;
    }
    return 0;
}

int dict_lookup_grep (Dict dict, Dict_char *pattern, int range,
                    int (*userfunc)(Dict_char *name, char *info))
{
    MatchWord *Rj;
    Dict_char prefix[MAX_LENGTH+1];
    char *this_pattern = pattern;
    DFA_states *dfas;
    MatchContext *mc;
    DFA *dfa = init_dfa();
    int i, d;

    i = parse_dfa (dfa, &this_pattern, dfa_thompson_chars);
    if (i || *this_pattern)
    {
        rm_dfa (&dfa);
        return -1;
    }
    dfa->root = dfa->top;
    dfas = mk_dfas (dfa, 50);
    rm_dfa (&dfa);

    mc = mk_MatchContext (dfas, range);

    Rj = xcalloc ((MAX_LENGTH+1) * mc->n, sizeof(*Rj));

    set_bit (mc, Rj, 0, 0);
    for (d = 1; d<=mc->range; d++)
    {
        int s;
        memcpy (Rj + mc->n * d, Rj + mc->n * (d-1), mc->n * sizeof(*Rj));
        for (s = 0; s<dfas->no; s++)
        {
            if (get_bit (mc, Rj, d-1, s))
            {
                DFA_state *state = dfas->sortarray[s];
                int i = state->tran_no;
                while (--i >= 0)
                    set_bit (mc, Rj, d, state->trans[i].to);
            }
        }
    }
    i = dict_grep (dict, 1, mc, Rj, 0, userfunc, prefix, dfas);

    rm_dfas (&dfas);
    xfree (Rj);
    rm_MatchContext (&mc);
    return i;
}
