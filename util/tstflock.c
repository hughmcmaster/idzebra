/*
 * Copyright (C) 1995-2006, Index Data ApS
 * See the file LICENSE for details.
 *
 * $Id: tstflock.c,v 1.10 2006-06-30 13:02:20 adam Exp $
 */

#include <assert.h>
#include <stdlib.h>
#include <yaz/test.h>
#include <yaz/log.h>
#if YAZ_POSIX_THREADS
#include <pthread.h>
#endif
#ifdef WIN32
#include <windows.h>
#include <process.h>
#endif

#include <idzebra/flock.h>
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <string.h>

static char seq[1000];
static char *seqp = 0;

#define NUM_THREADS 100

static void small_sleep()
{
#ifdef WIN32
    Sleep(50);
#else
    sleep(1);
#endif
}

void *run_func(void *arg)
{
    int i;
    int *pdata = (int*) arg;
    int use_write_lock = *pdata;
    ZebraLockHandle lh = zebra_lock_create(0, "my.LCK");
    for (i = 0; i<2; i++)
    {
        int write_lock;

        if (use_write_lock == 2)
            write_lock = (rand() & 3) == 3 ? 1 : 0;
        else
            write_lock = use_write_lock;
            
        if (write_lock)
        {
            zebra_lock_w(lh);
            
            *seqp++ = 'L';
            small_sleep();
            *seqp++ = 'U';
            
            zebra_unlock(lh);
        }
        else
        {
            zebra_lock_r(lh);
            
            *seqp++ = 'l';
            small_sleep();
            *seqp++ = 'u';
            
            zebra_unlock(lh);
        }
    }
    zebra_lock_destroy(lh);
    *pdata = 123;
    return 0;
}

#ifdef WIN32
DWORD WINAPI ThreadProc(void *p)
{
    run_func(p);
    return 0;
}
#endif

static void tst_thread(int num, int write_flag)
{
#ifdef WIN32
    HANDLE handles[NUM_THREADS];
    DWORD dwThreadId[NUM_THREADS];
#endif
#if YAZ_POSIX_THREADS
    pthread_t child_thread[NUM_THREADS];
#endif
    int i, id[NUM_THREADS];

    seqp = seq;
    assert (num <= NUM_THREADS);
    for (i = 0; i < num; i++)
    {
        id[i] = write_flag;
#if YAZ_POSIX_THREADS
        pthread_create(&child_thread[i], 0 /* attr */, run_func, &id[i]);
#endif
#ifdef WIN32
        if (1)
        {
            void *pData = &id[i];
            handles[i] = CreateThread(
                NULL,              /* default security attributes */
                0,                 /* use default stack size */
                ThreadProc,        /* thread function */
                pData,             /* argument to thread function */
                0,                 /* use default creation flags */
                &dwThreadId[i]);   /* returns the thread identifier */
        }

#endif
    }
#if YAZ_POSIX_THREADS
    for (i = 0; i<num; i++)
        pthread_join(child_thread[i], 0);
#endif
#ifdef WIN32
    WaitForMultipleObjects(num, handles, TRUE, INFINITE);
#endif
    for (i = 0; i < num; i++)
        YAZ_CHECK(id[i] == 123);
    *seqp++ = '\0';
}

static void tst()
{
    tst_thread(4, 1); /* write locks */
#if 0
    printf("seq=%s\n", seq);
#endif
    if (1)
    {
        int i = 0;
        while (seq[i])
        {
            YAZ_CHECK_EQ(seq[i], 'L');
            YAZ_CHECK_EQ(seq[i+1], 'U');
            i = i + 2;
        }
    }

#if 0
    tst_thread(6, 0);  /* read locks */
    printf("seq=%s\n", seq);
#endif
#if 0
    tst_thread(20, 2); /* random locks */
    printf("seq=%s\n", seq);
#endif
}

int main(int argc, char **argv)
{
    YAZ_CHECK_INIT(argc, argv);

    yaz_log_time_format("%s:%!");

    zebra_flock_init();

    tst();

    YAZ_CHECK_TERM;
}


/*
 * Local variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 * vim: shiftwidth=4 tabstop=8 expandtab
 */

