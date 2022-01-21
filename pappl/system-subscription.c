//
// System event functions for the Printer Application Framework
//
// Copyright © 2022 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

//
// Include necessary headers...
//

#include "pappl-private.h"


//
// Local functions...
//

static int	compare_subscriptions(pappl_subscription_t *a, pappl_subscription_t *b);


//
// 'papplSystemAddEvent()' - Add a notification event.
//

void
papplSystemAddEvent(
    pappl_system_t  *system,		// I - System
    pappl_printer_t *printer,		// I - Associated printer, if any
    pappl_job_t     *job,		// I - Associated job, if any
    pappl_event_t   event,		// I - IPP "notify-events" bit value
    const char      *message,		// I - printf-style message string
    ...)				// I - Additional arguments as needed
{
  va_list	ap;			// Argument pointer


  if (!system || !message)
    return;

  pthread_rwlock_wrlock(&system->rwlock);

  va_start(ap, message);
  _papplSystemAddEventNoLockv(system, printer, job, event, message, ap);
  va_end(ap);

  pthread_rwlock_unlock(&system->rwlock);
}


//
// '_papplSystemAddEventNoLock()' - Add a notification event (no lock).
//

void
_papplSystemAddEventNoLock(
    pappl_system_t  *system,		// I - System
    pappl_printer_t *printer,		// I - Associated printer, if any
    pappl_job_t     *job,		// I - Associated job, if any
    pappl_event_t   event,		// I - IPP "notify-events" bit value
    const char      *message,		// I - printf-style message string
    ...)				// I - Additional arguments as needed
{
  va_list	ap;			// Argument pointer


  va_start(ap, message);
  _papplSystemAddEventNoLockv(system, printer, job, event, message, ap);
  va_end(ap);
}


//
// '_papplSystemAddEventNoLockv()' - Add a notification event (no lock).
//

void
_papplSystemAddEventNoLockv(
    pappl_system_t  *system,		// I - System
    pappl_printer_t *printer,		// I - Associated printer, if any
    pappl_job_t     *job,		// I - Associated job, if any
    pappl_event_t   event,		// I - IPP "notify-events" bit value
    const char      *message,		// I - printf-style message string
    va_list         ap)			// I - Pointer to additional arguments
{
  (void)system;
  (void)printer;
  (void)job;
  (void)event;
  (void)message;
  (void)ap;
}


//
// '_papplSystemAddSubscription()' - Add a subscription to a system.
//

void
_papplSystemAddSubscription(
    pappl_system_t       *system,	// I - System
    pappl_subscription_t *sub,		// I - Subscription
    int                  sub_id)	// I - Subscription ID or `0` for new
{
  if (!system || !sub || sub_id < 0)
    return;

  pthread_rwlock_wrlock(&system->rwlock);

  if (sub_id == 0)
    sub->subscription_id = system->next_subscription_id ++;

  if (!system->subscriptions)
    system->subscriptions = cupsArrayNew((cups_array_func_t)compare_subscriptions, NULL);

  cupsArrayAdd(system->subscriptions, sub);

  pthread_rwlock_unlock(&system->rwlock);
}


//
// '_papplSystemCleanSubscriptions()' - Clean/expire subscriptions.
//

void
_papplSystemCleanSubscriptions(
    pappl_system_t *system)		// I - Subscription
{
  (void)system;
}


//
// 'papplSystemFindSubscription()' - Find a subscription.
//
// This function finds the numbered event notification subscription on a system.
//

pappl_subscription_t *			// O - Subscription or `NULL` if not found.
papplSystemFindSubscription(
    pappl_system_t *system,		// I - System
    int            sub_id)		// I - Subscription ID
{
  pappl_subscription_t	key,		// Search key
			*match;		// Match, if any


  if (!system || sub_id < 1)
    return (NULL);

  key.subscription_id = sub_id;

  pthread_rwlock_rdlock(&system->rwlock);
  match = (pappl_subscription_t *)cupsArrayFind(system->subscriptions, &key);
  pthread_rwlock_unlock(&system->rwlock);

  return (match);
}


//
// 'compare_subscriptions()' - Compare two subscriptions.
//

static int				// O - Result of comparison
compare_subscriptions(
    pappl_subscription_t *a,		// I - First subscription
    pappl_subscription_t *b)		// I - Second subscription
{
  return (b->subscription_id - a->subscription_id);
}

