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
  pappl_subscription_t	*sub;		// Current subscription
  ipp_t			*n;		// Notify event attributes
  char			uri[1024] = "",	// "notify-printer/system-uri" value
			text[1024];	// "notify-text" value
  va_list		cap;		// Copy of additional arguments


  // Loop through all of the subscriptions and deliver any events...
  for (sub = (pappl_subscription_t *)cupsArrayFirst(system->subscriptions); sub; sub = (pappl_subscription_t *)cupsArrayNext(system->subscriptions))
  {
    if ((sub->mask & event) && (!sub->job || job == sub->job) && (!sub->printer || printer == sub->printer))
    {
      pthread_rwlock_wrlock(&sub->rwlock);

      n = ippNew();
      ippAddString(n, IPP_TAG_EVENT_NOTIFICATION, IPP_CONST_TAG(IPP_TAG_CHARSET), "notify-charset", NULL, "utf-8");
      ippAddString(n, IPP_TAG_EVENT_NOTIFICATION, IPP_CONST_TAG(IPP_TAG_LANGUAGE), "notify-natural-language", NULL, sub->language);
      if (printer)
      {
        if (!uri[0])
          httpAssembleURI(HTTP_URI_CODING_ALL, uri, sizeof(uri), "ipps", NULL, system->hostname, system->port, printer->resource);

        ippAddString(n, IPP_TAG_EVENT_NOTIFICATION, IPP_TAG_URI, "notify-printer-uri", NULL, uri);
      }
      else
      {
        if (!uri[0])
          httpAssembleURI(HTTP_URI_CODING_ALL, uri, sizeof(uri), "ipps", NULL, system->hostname, system->port, "/ipp/system");

        ippAddString(n, IPP_TAG_EVENT_NOTIFICATION, IPP_TAG_URI, "notify-system-uri", NULL, uri);
      }
      if (job)
        ippAddInteger(n, IPP_TAG_EVENT_NOTIFICATION, IPP_TAG_INTEGER, "notify-job-id", job->job_id);
      ippAddInteger(n, IPP_TAG_EVENT_NOTIFICATION, IPP_TAG_INTEGER, "notify-subscription-id", sub->subscription_id);
      ippAddString(n, IPP_TAG_EVENT_NOTIFICATION, IPP_CONST_TAG(IPP_TAG_URI), "notify-subscription-uuid", NULL, sub->uuid);
      ippAddInteger(n, IPP_TAG_EVENT_NOTIFICATION, IPP_TAG_INTEGER, "notify-sequence-number", ++ sub->last_sequence);
      ippAddString(n, IPP_TAG_EVENT_NOTIFICATION, IPP_CONST_TAG(IPP_TAG_KEYWORD), "notify-subscribed-event", NULL, _papplSubscriptionEventString(event));
      if (message)
      {
        va_copy(cap, ap);
        vsnprintf(text, sizeof(text), message, cap);
        va_end(cap);
        ippAddString(n, IPP_TAG_EVENT_NOTIFICATION, IPP_TAG_TEXT, "notify-text", NULL, text);
      }
#if 0 // TODO: Support user data
      if (sub->userdata)
      {
        attr = ippCopyAttribute(n, sub->userdata, 0);
        ippSetGroupTag(n, &attr, IPP_TAG_EVENT_NOTIFICATION);
      }
#endif // 0
      if (job && (event & PAPPL_EVENT_JOB_ALL))
      {
        _papplJobCopyState(job, IPP_TAG_EVENT_NOTIFICATION, n, NULL);

	if (event == PAPPL_EVENT_JOB_CREATED)
	{
	  ippAddString(n, IPP_TAG_EVENT_NOTIFICATION, IPP_TAG_NAME, "job-name", NULL, job->name);
	  ippAddString(n, IPP_TAG_EVENT_NOTIFICATION, IPP_TAG_NAME, "job-originating-user-name", NULL, job->username);
	}
      }
      if (!sub->job && printer && (event & PAPPL_EVENT_PRINTER_ALL))
      {
	ippAddBoolean(n, IPP_TAG_EVENT_NOTIFICATION, "printer-is-accepting-jobs", 1);
        _papplPrinterCopyState(printer, IPP_TAG_EVENT_NOTIFICATION, n, NULL, NULL);
      }
      // TODO: add system event notifications
      if (printer)
	ippAddInteger(n, IPP_TAG_EVENT_NOTIFICATION, IPP_TAG_INTEGER, "printer-up-time", (int)(time(NULL) - printer->start_time));
      else
	ippAddInteger(n, IPP_TAG_EVENT_NOTIFICATION, IPP_TAG_INTEGER, "system-up-time", (int)(time(NULL) - system->start_time));

      cupsArrayAdd(sub->events, n);
      if (cupsArrayCount(sub->events) > 100)
      {
	cupsArrayRemove(sub->events, cupsArrayFirst(sub->events));
	sub->first_sequence ++;
      }

      pthread_rwlock_unlock(&sub->rwlock);

      // TODO: broadcast notification
    }
  }
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

