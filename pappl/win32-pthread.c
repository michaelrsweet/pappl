//
// Windows POSIX threading implementation for the Printer Application Framework
//
// Copyright © 2021-2022 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//


//
// Include necessary headers...
//

#include "base-private.h"
#include <setjmp.h>


//
// Private structures...
//

struct _pthread_s
{
  HANDLE	h;			// Thread handle
  void		*(*func)(void *);	// Thread start function
  void		*arg;			// Argument to pass to function
  void		*retval;		// Return value from function
  bool		canceled;		// Is the thread canceled?
  jmp_buf	jumpbuf;		// Jump buffer for error recovery
};


//
// Local functions...
//

static long	pthread_msec(struct timespec *ts);
static DWORD	pthread_tls(void);
static int	pthread_wrapper(pthread_t t);


//
// 'pthread_cancel()' - Cancel a child thread.
//

int					// O - 0 on success or errno on error
pthread_cancel(pthread_t t)		// I - Thread ID
{
  if (!t)
    return (EINVAL);

  t->canceled = true;

  return (0);
}


//
// 'pthread_cond_broadcast()' - Unblock all threads waiting on a condition
//                              variable.
//

int					// O - 0 on success or errno on error
pthread_cond_broadcast(
    pthread_cond_t *c)			// I - Condition variable
{
  WakeAllConditionVariable(c);

  return (0);
}


//
// 'pthread_cond_destroy()' - Free memory associated with a condition variable.
//

int					// O - 0 on success or errno on error
pthread_cond_destroy(pthread_cond_t *c)	// I - Condition variable
{
  (void)c;

  return (0);
}


//
// 'pthread_cond_init()' - Initialize a condition variable.
//

int					// O - 0 on success or errno on error
pthread_cond_init(pthread_cond_t *c,	// I - Condition variable
                  const void     *a)	// I - Attributes (not used)
{
  (void) a;

  InitializeConditionVariable(c);

  return (0);
}


//
// 'pthread_cond_signal()' - Wake a single thread waiting on a condition
//                           variable.
//

int					// O - 0 on success or errno on error
pthread_cond_signal(pthread_cond_t *c)	// I - Condition variable
{
  WakeConditionVariable(c);

  return (0);
}


//
// 'pthread_cond_timedwait()' - Wait a specified amount of time for a condition
//                              variable.
//

int					// O - 0 on success or errno on error
pthread_cond_timedwait(
    pthread_cond_t  *c,			// I - Condition variable
    pthread_mutex_t *m,			// I - Mutex
    struct timespec *t)			// I - Timeout
{
  pthread_testcancel();

  if (!SleepConditionVariableCS(c, m, pthread_msec(t)) || pthread_msec(t) <= 0)
    return (ETIMEDOUT);

  return (0);
}


//
// 'pthread_cond_wait()' - Wait indefinitely for a condition variable.
//

int					// O - 0 on success or errno on error
pthread_cond_wait(pthread_cond_t  *c,	// I - Condition variable
                  pthread_mutex_t *m)	// I - Mutex
{
  pthread_testcancel();

  SleepConditionVariableCS(c, m, INFINITE);

  return (0);
}


//
// 'pthread_create()' - Create a new child thread.
//

int					// O - 0 on success or errno on error
pthread_create(
    pthread_t      *tp,			// O - Thread ID
    pthread_attr_t *attr,		// I - Thread attributes (not used)
    void           *(*func)(void *),	// I - Thread start function
    void           *arg)		// I - Argument to pass to function
{
  pthread_t	t;			// Thread data


  if (!tp || !func)
    return (EINVAL);

  (void)attr;

  if ((t = (pthread_t)calloc(1, sizeof(struct _pthread_s))) == NULL)
  {
    *tp = NULL;
    return (ENOMEM);
  }

  *tp     = t;
  t->func = func;
  t->arg  = arg;
  t->h    = (HANDLE)_beginthreadex(NULL, 0, (LPTHREAD_START_ROUTINE)pthread_wrapper, t, 0, NULL);

  if (t->h == 0 || t->h == (HANDLE)-1)
    return (errno);

  if (attr && *attr == PTHREAD_CREATE_DETACHED)
    return (pthread_detach(t));
  else
    return (0);
}


//
// 'pthread_detach()' - Detach a child thread from its parent.
//

int					// O - 0 on success or errno on error
pthread_detach(pthread_t t)		// I - Thread ID
{
  if (!t)
    return (EINVAL);

  CloseHandle(t->h);
  t->h = 0;

  return (0);
}


//
// 'pthread_join()' - Wait for a child thread to complete.
//

int					// O - 0 on success or errno on error
pthread_join(pthread_t t,		// I - Thread ID
             void      **value)		// O - Return value from thread function
{
  if (!t)
    return (EINVAL);

  pthread_testcancel();

  if (t->h)
  {
    WaitForSingleObject(t->h, INFINITE);
    CloseHandle(t->h);
  }

  if (value)
    *value = t->retval;

  free(t);

  return (0);
}


//
// 'pthread_msec()' - Calculate milliseconds for a given time value.
//

static long				// O - Milliseconds to specified time
pthread_msec(struct timespec *ts)	// I - Time value
{
  struct timeval	curtime;	// Current time


  gettimeofday(&curtime, NULL);
  return (1000 * (long)(ts->tv_sec - curtime.tv_sec) + (ts->tv_nsec / 1000 - curtime.tv_usec) / 1000);
}


//
// 'pthread_mutex_destroy()' - Free memory used by a mutex.
//

int					// O - 0 on success or errno on error
pthread_mutex_destroy(
    pthread_mutex_t *m)			// I - Mutual exclusion lock
{
  if (!m)
    return (EINVAL);

  // Nothing to do...

  return (0);
}


//
// 'pthread_mutex_init()' - Initialize a mutex.
//

int					// O - 0 on success or errno on error
pthread_mutex_init(
    pthread_mutex_t *m,			// I - Mutual exclusion lock
    const void      *attr)		// I - Lock attributes (not used)
{
  if (!m)
    return (EINVAL);

  (void)attr;

  InitializeCriticalSection(m);

  return (0);
}


//
// 'pthread_mutex_lock()' - Lock a mutex.
//

int					// O - 0 on success or errno on error
pthread_mutex_lock(pthread_mutex_t *m)	// I - Mutual exclusion lock
{
  if (!m)
    return (EINVAL);

  EnterCriticalSection(m);

  return (0);
}


//
// 'pthread_mutex_unlock()' - Unlock a mutex.
//

int					// O - 0 on success or errno on error
pthread_mutex_unlock(pthread_mutex_t *m)// I - Mutual exclusion lock
{
  if (!m)
    return (EINVAL);

  LeaveCriticalSection(m);

  return (0);
}


//
// 'pthread_rwlock_destroy()' - Free all memory used by a reader/writer lock.
//

int					// O - 0 on success or errno on error
pthread_rwlock_destroy(
    pthread_rwlock_t *rw)		// I - Reader/writer lock
{
  if (!rw)
    return (EINVAL);

  // Nothing to do...

  return (0);
}


//
// 'pthread_rwlock_init()' - Initialize a reader/writer lock.
//

int					// O - 0 on success or errno on error
pthread_rwlock_init(
    pthread_rwlock_t *rw,		// I - Reader/writer lock
    const void       *attr)		// I - Lock attributes (not used)
{
  if (!rw)
    return (EINVAL);

  (void)attr;

  InitializeSRWLock(rw);

  return (0);
}


//
// 'pthread_rwlock_rdlock()' - Obtain a reader lock.
//

int					// O - 0 on success or errno on error
pthread_rwlock_rdlock(
    pthread_rwlock_t *rw)		// I - Reader/writer lock
{
  if (!rw)
    return (EINVAL);

  AcquireSRWLockShared(rw);

  return (0);
}


//
// 'pthread_rwlock_unlock()' - Release a reader/writer lock.
//

int					// O - 0 on success or errno on error
pthread_rwlock_unlock(
    pthread_rwlock_t *rw)		// I - Reader/writer lock
{
  void	*val;				// Lock value


  if (!rw)
    return (EINVAL);

  val = *(void **)rw;

  if (val == (void *)1)
    ReleaseSRWLockExclusive(rw);
  else
    ReleaseSRWLockShared(rw);

  return (0);
}


//
// 'pthread_rwlock_wrlock()' - Obtain a writer lock.
//

int					// O - 0 on success or errno on error
pthread_rwlock_wrlock(
    pthread_rwlock_t *rw)		// I - Reader/writer lock
{
  if (!rw)
    return (EINVAL);

  AcquireSRWLockExclusive(rw);

  return (0);
}


//
// 'pthread_self()' - Return the current thread.
//

pthread_t				// O - Thread
pthread_self(void)
{
  pthread_t	t;			// Thread


  if ((t = TlsGetValue(pthread_tls())) == NULL)
  {
    // Main thread, so create the info we need...
    if ((t = (pthread_t)calloc(1, sizeof(struct _pthread_s))) != NULL)
    {
      t->h = GetCurrentThread();
      TlsSetValue(pthread_tls(), t);

      if (setjmp(t->jumpbuf))
      {
        if (!t->h)
          free(t);

        _endthreadex(0);
      }
    }
  }

  return (t);
}


//
// 'pthread_testcancel()' - Mark a safe cancellation point.
//

void
pthread_testcancel(void)
{
  pthread_t	t;			// Current thread


  // Go to the thread's exit handler if we've been canceled...
  if ((t = pthread_self()) != NULL && t->canceled)
    longjmp(t->jumpbuf, 1);
}


//
// 'pthread_tls()' - Get the thread local storage key.
//

static DWORD				// O - Key
pthread_tls(void)
{
  static DWORD	tls = 0;		// Thread local storage key
  static CRITICAL_SECTION tls_mutex = { (void*)-1, -1, 0, 0, 0, 0 };
					// Lock for thread local storage access


  EnterCriticalSection(&tls_mutex);
  if (!tls)
  {
    if ((tls = TlsAlloc()) == TLS_OUT_OF_INDEXES)
      abort();
  }
  LeaveCriticalSection(&tls_mutex);

  return (tls);
}


//
// 'pthread_wrapper()' - Wrapper function for a POSIX thread.
//

static int				// O - Exit status
pthread_wrapper(pthread_t t)		// I - Thread
{
  TlsSetValue(pthread_tls(), t);

  if (!setjmp(t->jumpbuf))
  {
    // Call function in thread...
    t->retval = (t->func)(t->arg);
  }

  // Clean up...
  while (t->h == (HANDLE)-1)
  {
    // pthread_create hasn't finished initializing the handle...
    YieldProcessor();
    _ReadWriteBarrier();
  }

  // Free if detached...
  if (!t->h)
    free(t);

  return (0);
}
