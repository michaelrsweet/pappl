//
// Windows POSIX threading implementation for the Printer Application Framework
//
// Copyright © 2021 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//


//
// Include necessary headers...
//

#include "base-private.h"


//
// 'pthread_cancel()' - Cancel a child thread.
//

int					// O - 0 on success or errno on error
pthread_cancel(pthread_t t)		// I - Thread ID
{
  if (!t)
    return (EINVAL);

  // TODO: Implement me

  return (0);
}


//
// 'pthread_create()' - Create a new child thread.
//

int					// O - 0 on success or errno on error
pthread_create(
    pthread_t  *t,			// O - Thread ID
    const void *attr,			// I - Thread attributes (not used)
    void       *(*func)(void *),	// I - Thread start function
    void       *arg)			// I - Argument to pass to function
{
  if (!t || !func)
    return (EINVAL);

  (void)attr;

  *t = _beginthreadex(NULL, 0, (LPTHREAD_START_ROUTINE)func, arg, 0, NULL);

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

  // TODO: Implement me

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

  // Note: No support for actually getting the return value, which would
  // require some changes to the threading function...
  WaitForSingleObject(t, INFINITE);

  return (0);
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
  void	*val = *(void **)rw;		// Lock value


  if (!rw)
    return (EINVAL);

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

