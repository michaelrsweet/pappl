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
#include "subscription.h"
#include "scanner-private.h"


//
// Local functions...
//

static int	compare_subscriptions(pappl_subscription_t *a, pappl_subscription_t *b);


//
// 'papplSystemAddEvent()' - Add a notification event.
//

void
papplSystemAddEvent(
    pappl_system_t  *system,    // I - System
    pappl_printer_t *printer,   // I - Associated printer, if any
    pappl_job_t     *job,       // I - Associated job, if any
    pappl_event_t   event,      // I - IPP "notify-events" bit value
    const char      *message,   // I - printf-style message string
    ...)                        // I - Additional arguments as needed
{
  va_list ap; // Argument pointer

  if (!system)
    return;

  if (printer)
    _papplRWLockRead(printer);
  if (job)
    _papplRWLockRead(job);

  va_start(ap, message);
  _papplSystemAddEventNoLockv(system, printer, NULL, job, event, message, ap);
  va_end(ap);

  if (job)
    _papplRWUnlock(job);
  if (printer)
    _papplRWUnlock(printer);
}


void
papplSystemAddScannerEvent(
    pappl_system_t  *system,    // I - System
    pappl_scanner_t *scanner,   // I - Associated scanner, if any
    pappl_job_t     *job,       // I - Associated job, if any
    pappl_event_t   event,      // I - IPP "notify-events" bit value
    const char      *message,   // I - printf-style message string
    ...)                        // I - Additional arguments as needed
{
  va_list ap; // Argument pointer

  if (!system)
    return;

  if (scanner)
    _papplRWLockRead(scanner);
  if (job)
    _papplRWLockRead(job);

  va_start(ap, message);
  _papplSystemAddEventNoLockv(system, NULL, scanner, job, event, message, ap);
  va_end(ap);

  if (job)
    _papplRWUnlock(job);
  if (scanner)
    _papplRWUnlock(scanner);
}

//
// '_papplSystemAddEventNoLock()' - Add a notification event (no lock).
//

void
_papplSystemAddEventNoLock(
    pappl_system_t  *system,    // I - System
    pappl_printer_t *printer,   // I - Associated printer, if any
    pappl_scanner_t *scanner,   // I - Associated scanner, if any
    pappl_job_t     *job,       // I - Associated job, if any
    pappl_event_t   event,      // I - IPP "notify-events" bit value
    const char      *message,   // I - printf-style message string
    ...)                        // I - Additional arguments as needed
{
  va_list ap; // Argument pointer

  va_start(ap, message);
  _papplSystemAddEventNoLockv(system, printer, scanner, job, event, message, ap);
  va_end(ap);
}

//
// '_papplSystemAddEventNoLockv()' - Add a notification event (no lock).
//

void
_papplSystemAddEventNoLockv(
    pappl_system_t  *system,    // I - System
    pappl_printer_t *printer,   // I - Associated printer, if any
    pappl_scanner_t *scanner,   // I - Associated scanner, if any
    pappl_job_t     *job,       // I - Associated job, if any
    pappl_event_t   event,      // I - IPP "notify-events" bit value
    const char      *message,   // I - printf-style message string
    va_list         ap)         // I - Pointer to additional arguments
{
  pappl_subscription_t *sub;   // Current subscription
  ipp_t *n;                    // Notify event attributes
  char uri[1024] = "",         // "notify-printer/scanner/system-uri" value
       text[1024];             // "notify-text" value
  va_list cap;                 // Copy of additional arguments

  // Loop through all of the subscriptions and deliver any events...
  _papplRWLockRead(system);

  if (system->systemui_cb && system->systemui_data)
    (system->systemui_cb)(system, printer, job, event, system->systemui_data);

  if (system->event_cb)
    (system->event_cb)(system, printer, job, event, system->event_data);

  // Check if scanner-specific callbacks are defined
  if (system->systemui_scan_cb && system->systemui_scan_data)
    (system->systemui_scan_cb)(system, scanner, job, event, system->systemui_scan_data);

  if (system->scan_event_cb && system->scan_event_data)
    (system->scan_event_cb)(system, scanner, job, event, system->scan_event_data);

  for (sub = (pappl_subscription_t *)cupsArrayGetFirst(system->subscriptions); sub; sub = (pappl_subscription_t *)cupsArrayGetNext(system->subscriptions))
  {
    if ((sub->mask & event) && (!sub->job || job == sub->job) && (!sub->printer || printer == sub->printer) && (!sub->scanner || scanner == sub->scanner))
    {
      _papplRWLockWrite(sub);

      n = ippNew();
      ippAddString(n, IPP_TAG_EVENT_NOTIFICATION, IPP_CONST_TAG(IPP_TAG_CHARSET), "notify-charset", NULL, "utf-8");
      ippAddString(n, IPP_TAG_EVENT_NOTIFICATION, IPP_CONST_TAG(IPP_TAG_LANGUAGE), "notify-natural-language", NULL, sub->language);

      if (printer)
      {
        if (!uri[0])
          httpAssembleURI(HTTP_URI_CODING_ALL, uri, sizeof(uri), "ipps", NULL, system->hostname, system->port, printer->resource);

        ippAddString(n, IPP_TAG_EVENT_NOTIFICATION, IPP_TAG_URI, "notify-printer-uri", NULL, uri);
      }
      else if (scanner)
      {
        if (!uri[0])
          httpAssembleURI(HTTP_URI_CODING_ALL, uri, sizeof(uri), "ipps", NULL, system->hostname, system->port, scanner->resource);

        ippAddString(n, IPP_TAG_EVENT_NOTIFICATION, IPP_TAG_URI, "notify-scanner-uri", NULL, uri);
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
      ippAddInteger(n, IPP_TAG_EVENT_NOTIFICATION, IPP_TAG_INTEGER, "notify-sequence-number", ++sub->last_sequence);
      ippAddString(n, IPP_TAG_EVENT_NOTIFICATION, IPP_CONST_TAG(IPP_TAG_KEYWORD), "notify-subscribed-event", NULL, _papplSubscriptionEventString(event));

      if (message)
      {
        va_copy(cap, ap);
        vsnprintf(text, sizeof(text), message, cap);
        va_end(cap);
        ippAddString(n, IPP_TAG_EVENT_NOTIFICATION, IPP_TAG_TEXT, "notify-text", NULL, text);
      }

      if (job && (event & PAPPL_EVENT_JOB_ALL))
      {
        _papplJobCopyStateNoLock(job, IPP_TAG_EVENT_NOTIFICATION, n, NULL);

        if (event == PAPPL_EVENT_JOB_CREATED)
        {
          ippAddString(n, IPP_TAG_EVENT_NOTIFICATION, IPP_TAG_NAME, "job-name", NULL, job->name);
          ippAddString(n, IPP_TAG_EVENT_NOTIFICATION, IPP_TAG_NAME, "job-originating-user-name", NULL, job->username);
        }
      }

      if (!sub->job && printer && (event & PAPPL_EVENT_PRINTER_ALL))
        _papplPrinterCopyStateNoLock(printer, IPP_TAG_EVENT_NOTIFICATION, n, NULL, NULL);

      if (!sub->job && scanner && (event & PAPPL_EVENT_SCANNER_ALL))
        _papplScannerCopyStateNoLock(scanner, IPP_TAG_EVENT_NOTIFICATION, n, NULL, NULL);

      if (printer)
        ippAddInteger(n, IPP_TAG_EVENT_NOTIFICATION, IPP_TAG_INTEGER, "printer-up-time", (int)(time(NULL) - printer->start_time));
      else if (scanner)
        ippAddInteger(n, IPP_TAG_EVENT_NOTIFICATION, IPP_TAG_INTEGER, "scanner-up-time", (int)(time(NULL) - scanner->start_time));
      else
        ippAddInteger(n, IPP_TAG_EVENT_NOTIFICATION, IPP_TAG_INTEGER, "system-up-time", (int)(time(NULL) - system->start_time));

      cupsArrayAdd(sub->events, n);
      if (cupsArrayGetCount(sub->events) > PAPPL_MAX_EVENTS)
      {
        cupsArrayRemove(sub->events, cupsArrayGetFirst(sub->events));
        sub->first_sequence++;
      }

      _papplRWUnlock(sub);

      pthread_cond_broadcast(&system->subscription_cond);
    }
  }

  _papplRWUnlock(system);
}


//
// '_papplSystemAddSubscription()' - Add a subscription to a system.
//

bool					// O - `true` on success, `false` on error
_papplSystemAddSubscription(
    pappl_system_t       *system,	// I - System
    pappl_subscription_t *sub,		// I - Subscription
    int                  sub_id)	// I - Subscription ID or `0` for new
{
  if (!system || !sub || sub_id < 0)
    return (false);

  _papplRWLockWrite(system);

  if (!system->subscriptions)
    system->subscriptions = cupsArrayNew((cups_array_cb_t)compare_subscriptions, NULL, NULL, 0, NULL, NULL);

  if (!system->subscriptions || (system->max_subscriptions && (size_t)cupsArrayGetCount(system->subscriptions) >= system->max_subscriptions))
  {
    _papplRWUnlock(system);
    return (false);
  }

  if (sub_id == 0)
    sub->subscription_id = ++ system->next_subscription_id;

  cupsArrayAdd(system->subscriptions, sub);

  _papplRWUnlock(system);

  return (true);
}


//
// '_papplSystemCleanSubscriptions()' - Clean/expire subscriptions.
//

void
_papplSystemCleanSubscriptions(
    pappl_system_t *system,		// I - Subscription
    bool           clean_all)		// I - Clean all subscriptions?
{
  pappl_subscription_t	*sub;		// Current subscription
  cups_array_t		*expired = NULL;// Expired subscriptions
  time_t		curtime;	// Current time


  // Loop through all of the subscriptions and move all of the expired or
  // canceled subscriptions to a temporary array...
  _papplRWLockWrite(system);
  for (curtime = time(NULL), sub = (pappl_subscription_t *)cupsArrayGetFirst(system->subscriptions); sub; sub = (pappl_subscription_t *)cupsArrayGetNext(system->subscriptions))
  {
    if (clean_all || sub->is_canceled || sub->expire <= curtime)
    {
      if (!expired)
        expired = cupsArrayNew(NULL, NULL, NULL, 0, NULL, NULL);

      cupsArrayAdd(expired, sub);
      cupsArrayRemove(system->subscriptions, sub);
    }
  }
  _papplRWUnlock(system);

  // Now clean up the expired subscriptions...
  for (sub = (pappl_subscription_t *)cupsArrayGetFirst(expired); sub; sub = (pappl_subscription_t *)cupsArrayGetNext(expired))
    _papplSubscriptionDelete(sub);

  cupsArrayDelete(expired);
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

  _papplRWLockRead(system);
  match = (pappl_subscription_t *)cupsArrayFind(system->subscriptions, &key);
  _papplRWUnlock(system);

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
