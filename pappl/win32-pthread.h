//
// Windows POSIX threading header file for the Printer Application Framework
//
// Copyright © 2021-2022 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#ifndef _PAPPL_WIN32_PTHREAD_H_
#  define _PAPPL_WIN32_PTHREAD_H_
#  include <process.h>
#  include <windows.h>


//
// Constants...
//

#  define PTHREAD_COND_INITIALIZER	{ 0 }
#  define PTHREAD_MUTEX_INITIALIZER	{ (void*)-1, -1, 0, 0, 0, 0 }
#  define PTHREAD_RWLOCK_INITIALIZER	{ 0 }

#  define PTHREAD_CREATE_JOINABLE	0
#  define PTHREAD_CREATE_DETACHED	1


//
// Types...
//

typedef struct _pthread_s *pthread_t;	// Thread identifier
typedef unsigned pthread_attr_t;	// Thread creation attributes
typedef CONDITION_VARIABLE pthread_cond_t;
					// Condition variable
typedef CRITICAL_SECTION pthread_mutex_t;
					// Mutual exclusion lock
typedef SRWLOCK pthread_rwlock_t;	// Reader/writer lock


//
// Functions...
//

extern int	pthread_cancel(pthread_t t);
extern int	pthread_create(pthread_t *t, pthread_attr_t *attr, void *(*func)(void *), void *arg);
extern int	pthread_detach(pthread_t t);
extern int	pthread_join(pthread_t t, void **value);

#  define pthread_attr_destroy(attr)
#  define pthread_attr_init(attr) *(attr) = 0
#  define pthread_attr_setdetachstate(attr,s) *(attr) = (s)

extern int	pthread_cond_broadcast(pthread_cond_t *c);
extern int	pthread_cond_destroy(pthread_cond_t *c);
extern int	pthread_cond_init(pthread_cond_t *c, const void *attr);
extern int	pthread_cond_signal(pthread_cond_t *c);
extern int	pthread_cond_timedwait(pthread_cond_t *c, pthread_mutex_t *m, struct timespec *t);
extern int	pthread_cond_wait(pthread_cond_t *c, pthread_mutex_t *m);

extern int	pthread_mutex_destroy(pthread_mutex_t *m);
extern int	pthread_mutex_init(pthread_mutex_t *m, const void *attr);
extern int	pthread_mutex_lock(pthread_mutex_t *m);
extern int	pthread_mutex_unlock(pthread_mutex_t *m);

extern int	pthread_rwlock_destroy(pthread_rwlock_t *rw);
extern int	pthread_rwlock_init(pthread_rwlock_t *rw, const void *attr);
extern int	pthread_rwlock_rdlock(pthread_rwlock_t *rw);
extern int	pthread_rwlock_unlock(pthread_rwlock_t *rw);
extern int	pthread_rwlock_wrlock(pthread_rwlock_t *rw);

extern pthread_t pthread_self(void);
extern void	pthread_testcancel(void);

#endif // _PAPPL_WIN32_PTHREAD_H_
