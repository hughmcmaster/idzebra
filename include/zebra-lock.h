
#ifndef ZEBRA_LOCK_H
#define ZEBRA_LOCK_H

#if HAVE_PTHREAD_H
#include <pthread.h>
#endif

#include <yaz/yconfig.h>

YAZ_BEGIN_CDECL

typedef struct {
    pthread_mutex_t mutex;
} Zebra_mutex;

YAZ_EXPORT int zebra_mutex_init (Zebra_mutex *p);
YAZ_EXPORT int zebra_mutex_destroy (Zebra_mutex *p);
YAZ_EXPORT int zebra_mutex_lock (Zebra_mutex *p);
YAZ_EXPORT int zebra_mutex_unlock (Zebra_mutex *p);

typedef struct {
    int readers_reading;
    int writers_writing;
    pthread_mutex_t mutex;
    pthread_cond_t lock_free;
} Zebra_lock_rdwr;

YAZ_EXPORT int zebra_lock_rdwr_init (Zebra_lock_rdwr *p);
YAZ_EXPORT int zebra_lock_rdwr_destroy (Zebra_lock_rdwr *p);
YAZ_EXPORT int zebra_lock_rdwr_rlock (Zebra_lock_rdwr *p);
YAZ_EXPORT int zebra_lock_rdwr_wlock (Zebra_lock_rdwr *p);
YAZ_EXPORT int zebra_lock_rdwr_runlock (Zebra_lock_rdwr *p);
YAZ_EXPORT int zebra_lock_rdwr_wunlock (Zebra_lock_rdwr *p);

typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} Zebra_mutex_cond;

YAZ_EXPORT int zebra_mutex_cond_init (Zebra_mutex_cond *p);
YAZ_EXPORT int zebra_mutex_cond_destroy (Zebra_mutex_cond *p);
YAZ_EXPORT int zebra_mutex_cond_lock (Zebra_mutex_cond *p);
YAZ_EXPORT int zebra_mutex_cond_unlock (Zebra_mutex_cond *p);
YAZ_EXPORT int zebra_mutex_cond_wait (Zebra_mutex_cond *p);
YAZ_EXPORT int zebra_mutex_cond_signal (Zebra_mutex_cond *p);

YAZ_END_CDECL
#endif
