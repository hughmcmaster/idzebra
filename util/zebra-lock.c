
#include <assert.h>

#include <zebra-lock.h>

int zebra_mutex_init (Zebra_mutex *p)
{
#if HAVE_PTHREAD_H
    pthread_mutex_init (&p->mutex, 0);
#endif
#ifdef WIN32
    InitializeCriticalSection (&p->mutex);
#endif
    return 0;
}

int zebra_mutex_destroy (Zebra_mutex *p)
{
#if HAVE_PTHREAD_H
    pthread_mutex_destroy (&p->mutex);
#endif
#ifdef WIN32
    DeleteCriticalSection (&p->mutex);
#endif
    return 0;
}

int zebra_mutex_lock (Zebra_mutex *p)
{
#if HAVE_PTHREAD_H
    pthread_mutex_lock (&p->mutex);
#endif
#ifdef WIN32
    EnterCriticalSection (&p->mutex);
#endif
    return 0;
}

int zebra_mutex_unlock (Zebra_mutex *p)
{
#if HAVE_PTHREAD_H
    pthread_mutex_unlock (&p->mutex);
#endif
#ifdef WIN32
    LeaveCriticalSection (&p->mutex);
#endif
    return 0;
}

int zebra_lock_rdwr_init (Zebra_lock_rdwr *p)
{
    p->readers_reading = 0;
    p->writers_writing = 0;
#if HAVE_PTHREAD_H
    pthread_mutex_init (&p->mutex, 0);
    pthread_cond_init (&p->lock_free, 0);
#endif
    return 0;
}

int zebra_lock_rdwr_destroy (Zebra_lock_rdwr *p)
{
    assert (p->readers_reading == 0);
    assert (p->writers_writing == 0);
#if HAVE_PTHREAD_H
    pthread_mutex_destroy (&p->mutex);
    pthread_cond_destroy (&p->lock_free);
#endif
    return 0;
}

int zebra_lock_rdwr_rlock (Zebra_lock_rdwr *p)
{
#if HAVE_PTHREAD_H
    pthread_mutex_lock (& p->mutex);
    while (p->writers_writing)
	pthread_cond_wait (&p->lock_free, &p->mutex);
    p->readers_reading++;
    pthread_mutex_unlock(&p->mutex);
#endif
    return 0;
}

int zebra_lock_rdwr_wlock (Zebra_lock_rdwr *p)
{
#if HAVE_PTHREAD_H
    pthread_mutex_lock (&p->mutex);
    while (p->writers_writing || p->readers_reading)
	pthread_cond_wait (&p->lock_free, &p->mutex);
    p->writers_writing++;
    pthread_mutex_unlock (&p->mutex);
#endif
    return 0;
}

int zebra_lock_rdwr_runlock (Zebra_lock_rdwr *p)
{
#if HAVE_PTHREAD_H
    pthread_mutex_lock (&p->mutex);
    if (p->readers_reading == 0)
    {
	pthread_mutex_unlock (&p->mutex);
	return -1;
    } 
    else
    {
	p->readers_reading--;
	if (p->readers_reading == 0)
	    pthread_cond_signal (&p->lock_free);
	pthread_mutex_unlock (&p->mutex);
    }
#endif
    return 0;
}

int zebra_lock_rdwr_wunlock (Zebra_lock_rdwr *p)
{
#if HAVE_PTHREAD_H
    pthread_mutex_lock (&p->mutex);
    if (p->writers_writing == 0)
    {
	pthread_mutex_unlock (&p->mutex);
	return -1;
    }
    else
    {
	p->writers_writing--;
	pthread_cond_broadcast(&p->lock_free);
	pthread_mutex_unlock(&p->mutex);
    }
#endif
    return 0;
}

int zebra_mutex_cond_init (Zebra_mutex_cond *p)
{
#if HAVE_PTHREAD_H
    pthread_cond_init (&p->cond, 0);
    pthread_mutex_init (&p->mutex, 0);
#endif
    return 0;
}

int zebra_mutex_cond_destroy (Zebra_mutex_cond *p)
{
#if HAVE_PTHREAD_H
    pthread_cond_destroy (&p->cond);
    pthread_mutex_destroy (&p->mutex);
#endif
    return 0;
}

int zebra_mutex_cond_lock (Zebra_mutex_cond *p)
{
#if HAVE_PTHREAD_H
    return pthread_mutex_lock (&p->mutex);
#else
    return 0;
#endif
}

int zebra_mutex_cond_unlock (Zebra_mutex_cond *p)
{
#if HAVE_PTHREAD_H
    return pthread_mutex_unlock (&p->mutex);
#else
    return 0;
#endif
}

int zebra_mutex_cond_wait (Zebra_mutex_cond *p)
{
#if HAVE_PTHREAD_H
    return pthread_cond_wait (&p->cond, &p->mutex);
#else
    return 0;
#endif
}

int zebra_mutex_cond_signal (Zebra_mutex_cond *p)
{
#if HAVE_PTHREAD_H
    return pthread_cond_signal (&p->cond);
#else
    return 0;
#endif
}
