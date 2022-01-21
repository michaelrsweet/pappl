//
// System event subscription functions for the Printer Application Framework
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
// Local globals...
//

static const char * const pappl_events[31] =
{					// IPP "notify-events" strings for bits
  "document-completed",
  "document-config-changed",
  "document-created",
  "document-fetchable",
  "document-state-changed",
  "document-stopped",

  "job-completed",
  "job-config-changed",
  "job-created",
  "job-fetchable",
  "job-progress",
  "job-state-changed",
  "job-stopped",

  "printer-config-changed",
  "printer-finishings-changed",
  "printer-media-changed",
  "printer-queue-order-changed",
  "printer-restarted",
  "printer-shutdown",
  "printer-state-changed",
  "printer-stopped",

  "resource-canceled",
  "resource-config-changed",
  "resource-created",
  "resource-installed",
  "resource-changed",

  "printer-created",
  "printer-deleted",

  "system-config-changed",
  "system-state-changed",
  "system-stopped"
};


//
// 'papplSubscriptionCancel()' - Cancel a subscription.
//
// This function cancels a subscription.
//

void
papplSubscriptionCancel(
    pappl_subscription_t *sub)		// I - Subscription
{
  (void)sub;
}


//
// 'papplSubscriptionCreate()' - Create a subscription.
//
// This function creates a new system, printer, or job event subscription.
//

pappl_subscription_t *			// O - Subscription
papplSubscriptionCreate(
    pappl_system_t  *system,		// I - System
    pappl_printer_t *printer,		// I - Printer, if any
    pappl_job_t     *job,		// I - Job, if any
    int             sub_id,		// I - Subscription ID or `0` for new
    pappl_event_t   events,		// I - Notification events
    const char      *username,		// I - Owner
    const char      *natural_language,	// I - Language
    int             interval,		// I - Notification interval
    int             lease)		// I - Lease duration or `0` for unlimited
{
  pappl_subscription_t	*sub;		// New subscription
  char		uuid[256];		// "notify-subscription-uuid" value


  if (!system || !events)
    return (NULL);

  if ((sub = (pappl_subscription_t *)calloc(1, sizeof(pappl_subscription_t))) == NULL)
    return (NULL);

  sub->printer         = printer;
  sub->job             = job;
  sub->subscription_id = sub_id;
  sub->mask            = events;
  sub->username        = strdup(username);
  sub->language        = strdup(natural_language ? natural_language : "en");
  sub->interval        = interval;
  sub->lease           = lease;

  if (lease)
    sub->expire = time(NULL) + lease;
  else
    sub->expire = time(NULL) + 86400 * 365;

  _papplSystemAddSubscription(system, sub, sub_id);

  sub->attrs = ippNew();
  ippAddString(sub->attrs, IPP_TAG_SUBSCRIPTION, IPP_CONST_TAG(IPP_TAG_CHARSET), "notify-charset", NULL, "utf-8");
  ippAddString(sub->attrs, IPP_TAG_SUBSCRIPTION, IPP_CONST_TAG(IPP_TAG_LANGUAGE), "notify-natural-language", NULL, sub->language);
  ippAddInteger(sub->attrs, IPP_TAG_SUBSCRIPTION, IPP_TAG_INTEGER, "notify-subscription-id", sub->subscription_id);

  _papplSystemMakeUUID(system, printer ? printer->name : NULL, -sub->subscription_id, uuid, sizeof(uuid));
  ippAddString(sub->attrs, IPP_TAG_SUBSCRIPTION, IPP_TAG_URI, "notify-subscription-uuid", NULL, uuid);

  if (job)
    ippAddInteger(sub->attrs, IPP_TAG_SUBSCRIPTION, IPP_TAG_INTEGER, "notify-job-id", job->job_id);
  else
    ippAddInteger(sub->attrs, IPP_TAG_SUBSCRIPTION, IPP_TAG_INTEGER, "notify-lease-duration", sub->lease);

  ippAddString(sub->attrs, IPP_TAG_SUBSCRIPTION, IPP_CONST_TAG(IPP_TAG_NAME), "notify-subscriber-user-name", NULL, sub->username);

  _papplSubscriptionEventExport(sub->attrs, "notify-events", IPP_TAG_SUBSCRIPTION, events);

  ippAddString(sub->attrs, IPP_TAG_SUBSCRIPTION, IPP_CONST_TAG(IPP_TAG_KEYWORD), "notify-pull-method", NULL, "ippget");

  sub->events = cupsArrayNew3(NULL, NULL, NULL, 0, NULL, (cups_afree_func_t)ippDelete);

  return (sub);
}


//
// '_papplSubscriptionDelete()' - Free the memory used for a subscription.
//

void
_papplSubscriptionDelete(
    pappl_subscription_t *sub)		// I - Subscription
{
  ippDelete(sub->attrs);
  free(sub->username);
  free(sub->language);
  cupsArrayDelete(sub->events);

  free(sub);
}


//
// '_papplSubscriptionEventExport()' - Convert an IPP "notify-events" bit field value to an attribute.
//

ipp_attribute_t	*			// O - IPP attribute
_papplSubscriptionEventExport(
    ipp_t         *ipp,			// I - IPP message
    const char    *name,		// I - IPP attribute name
    ipp_tag_t     group_tag,		// I - IPP group
    pappl_event_t value)		// I - IPP "notify-events" bit value
{
  pappl_event_t	event;			// Current event
  int		i,			// Looping var
		num_events = 0;		// Number of event keywords
  const char	*events[31];		// Event keywords


  for (i = 0, event = PAPPL_EVENT_DOCUMENT_COMPLETED; i < (int)(sizeof(pappl_events) / sizeof(pappl_events[0])); i ++, event *= 2)
  {
    if (value & event)
      events[num_events ++] = pappl_events[i];
  }

  if (num_events == 0)
    events[num_events ++] = "none";

  return (ippAddStrings(ipp, group_tag, IPP_CONST_TAG(IPP_TAG_KEYWORD), name, num_events, NULL, events));
}


//
// '_papplSubscriptionEventImport()' - Convert an IPP "notify-events" attribute to a bit field value.
//

pappl_event_t				// O - Bit field value
_papplSubscriptionEventImport(
    ipp_attribute_t *value)		// I - IPP attribute
{
  pappl_event_t	events;			// Current event
  int		i,			// Looping var
		count;			// Number of event keywords


  for (events = PAPPL_EVENT_NONE, i = 0, count = ippGetCount(value); i < count; i ++)
    events |= _papplSubscriptionEventValue(ippGetString(value, i, NULL));

  return (events);
}


//
// '_papplSubscriptionEventString()' - Return the keyword value associated with the IPP "notify-events" bit value.
//

const char *				// O - IPP "notify-events" keyword value
_papplSubscriptionEventString(
    pappl_event_t value)		// I - IPP "notify-events" bit value
{
  if (value == PAPPL_EVENT_NONE)
    return ("none");
  else
    return (_PAPPL_LOOKUP_STRING(value, pappl_events));
}


//
// '_papplSubscriptionEventValue()' - Return the bit value associated with the IPP "notify-events" keyword value.
//

pappl_event_t				// O - IPP "notify-events" bit value
_papplSubscriptionEventValue(
    const char *value)			// I - IPP "notify-events" keyword value
{
  return ((pappl_event_t)_PAPPL_LOOKUP_VALUE(value, pappl_events));
}


//
// 'papplSubscriptionGetEvents()' - Return a subcription's events.
//
// This function returns a subscription's events.
//

pappl_event_t				// O - IPP "notify-events" bit field
papplSubscriptionGetEvents(
    pappl_subscription_t *sub)		// I - Subscription
{
  return (sub ? sub->mask : PAPPL_EVENT_NONE);
}


//
// 'papplSubscriptionGetID()' - Return a subcription's numeric identifier.
//
// This function returns a subscription's numeric identifier.
//

int					// O - Subscription ID
papplSubscriptionGetID(
    pappl_subscription_t *sub)		// I - Subscription
{
  return (sub ? sub->subscription_id : 0);
}


//
// 'papplSubscriptionGetJob()' - Return a subcription's associated job, if any.
//
// This function returns a subscription's associated job, if any.
//

pappl_job_t *				// O - Job or `NULL` if not a job subscription
papplSubscriptionGetJob(
    pappl_subscription_t *sub)		// I - Subscription
{
  return (sub ? sub->job : NULL);
}


//
// 'papplSubscriptionGetPrinter()' - Return a subcription's associated printer, if any.
//
// This function returns a subscription's associated printer, if any.
//

pappl_printer_t *			// O - Printer or `NULL` if not a printer subscription
papplSubscriptionGetPrinter(
    pappl_subscription_t *sub)		// I - Subscription
{
  return (sub ? sub->printer : NULL);
}


//
// 'papplSubscriptionGetUsername()' - Return a subcription's owner.
//
// This function returns a subscription's owner.
//

const char *				// O - Owner
papplSubscriptionGetUsername(
    pappl_subscription_t *sub)		// I - Subscription
{
  return (sub ? sub->username : NULL);
}
