//
// IPP subscription processing for the Printer Application Framework
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

static pappl_subscription_t	*find_subscription(pappl_client_t *client);


//
// '_papplSubscriptionIPPCancel()' - Cancel a subscription.
//

void
_papplSubscriptionIPPCancel(
    pappl_client_t *client)		// I - Client
{
  pappl_subscription_t	*sub;		// Subscription


  // Authorize access...
  if (client->printer)
  {
    if (!_papplPrinterIsAuthorized(client))
      return;
  }
  else
  {
    http_status_t	auth_status;	// Authorization status

    // Verify the connection is authorized...
    if ((auth_status = papplClientIsAuthorized(client)) != HTTP_STATUS_CONTINUE)
    {
      papplClientRespond(client, auth_status, NULL, NULL, 0, 0);
      return;
    }
  }

  // Find the subscription...
  if ((sub = find_subscription(client)) == NULL)
    return;

  // Cancel it...
  papplSubscriptionCancel(sub);
  papplClientRespondIPP(client, IPP_STATUS_OK, NULL);
}


//
// '_papplSubscriptionIPPCreate()' - Create subscriptions.
//

void
_papplSubscriptionIPPCreate(
    pappl_client_t *client)		// I - Client
{
  pappl_subscription_t	*sub;		// Subscription
  ipp_attribute_t	*attr;		// Subscription attribute
  const char		*username;	// Most authenticated username
  int			num_subs = 0,	// Number of subscriptions
			ok_subs = 0;	// Number of good subscriptions


  // Authorize access...
  if (client->printer)
  {
    if (!_papplPrinterIsAuthorized(client))
      return;
  }
  else
  {
    http_status_t	auth_status;	// Authorization status

    // Verify the connection is authorized...
    if ((auth_status = papplClientIsAuthorized(client)) != HTTP_STATUS_CONTINUE)
    {
      papplClientRespond(client, auth_status, NULL, NULL, 0, 0);
      return;
    }
  }

  if (ippGetOperation(client->request) == IPP_OP_CREATE_JOB_SUBSCRIPTIONS && !client->job)
  {
    // Get the job target for the subscription...
    int	job_id;				// Job ID

    if ((attr = ippFindAttribute(client->request, "notify-job-id", IPP_TAG_ZERO)) == NULL)
    {
      papplClientRespondIPP(client, IPP_STATUS_ERROR_BAD_REQUEST, "Missing \"notify-job-id\" attribute.");
      return;
    }
    else if (ippGetGroupTag(attr) != IPP_TAG_OPERATION || ippGetValueTag(attr) != IPP_TAG_INTEGER || ippGetCount(attr) != 1 || (job_id = ippGetInteger(attr, 0)) < 1)
    {
      papplClientRespondIPP(client, IPP_STATUS_ERROR_BAD_REQUEST, "Bad \"notify-job-id\" attribute.");
      return;
    }
    else if ((client->job = papplPrinterFindJob(client->printer, job_id)) == NULL)
    {
      papplClientRespondIPP(client, IPP_STATUS_ERROR_NOT_FOUND, "Job #%d not found.", job_id);
      return;
    }
  }

  // For the Create-xxx-Subscriptions operations, queue up a successful-ok
  // response...
  if (ippGetOperation(client->request) == IPP_OP_CREATE_JOB_SUBSCRIPTIONS || ippGetOperation(client->request) == IPP_OP_CREATE_PRINTER_SUBSCRIPTIONS || ippGetOperation(client->request) == IPP_OP_CREATE_SYSTEM_SUBSCRIPTIONS)
    papplClientRespondIPP(client, IPP_STATUS_OK, NULL);

  if (client->username[0])
    username = client->username;
  else if ((username = ippGetString(ippFindAttribute(client->request, "requesting-user-name", IPP_TAG_NAME), 0, NULL)) == NULL)
    username = "anonymous";

  // Skip past the initial attributes to the first subscription group.
  attr = ippGetFirstAttribute(client->request);
  while (attr && ippGetGroupTag(attr) != IPP_TAG_SUBSCRIPTION)
    attr = ippGetNextAttribute(client->request);

  while (attr)
  {
    const char		*attrname,	// Attribute name
			*pull_method = NULL,
					// "notify-pull-method" value
			*language = "en";
					// "notify-natural-language" value
    pappl_event_t	events = PAPPL_EVENT_NONE;
					// "notify-events" bit field
    const void		*data = NULL;	// "notify-user-data" value, if any
    cups_len_t		datalen = 0;	// "notify-user-data" value length
    int			interval = 0,	// "notify-time-interval" value
			lease = PAPPL_LEASE_DEFAULT;
					// "notify-lease-duration" value
    ipp_status_t	status = IPP_STATUS_OK;
					// "notify-status-code" value

    num_subs ++;

    while (attr)
    {
      if ((attrname = ippGetName(attr)) == NULL)
        break;

      if (!strcmp(attrname, "notify-recipient-uri"))
      {
        // Don't allow push notifications...
        status = IPP_STATUS_ERROR_ATTRIBUTES_OR_VALUES;
	ippCopyAttribute(client->response, attr, 0);
      }
      else if (!strcmp(attrname, "notify-pull-method"))
      {
        // Allow "ippget" pull method...
	pull_method = ippGetString(attr, 0, NULL);

        if (ippGetValueTag(attr) != IPP_TAG_KEYWORD || ippGetCount(attr) != 1 || !pull_method || strcmp(pull_method, "ippget"))
	{
          ippCopyAttribute(client->response, attr, 0);
	  pull_method = NULL;
	  status      = IPP_STATUS_ERROR_ATTRIBUTES_OR_VALUES;
	}
      }
      else if (!strcmp(attrname, "notify-charset"))
      {
        // Only allow "utf-8" and "us-ascii" character sets...
        const char *charset = ippGetString(attr, 0, NULL);
					// "notify-charset" value

        if (ippGetValueTag(attr) != IPP_TAG_CHARSET || ippGetCount(attr) != 1 || !charset ||
	    (strcmp(charset, "us-ascii") && strcmp(charset, "utf-8")))
	{
	  status = IPP_STATUS_ERROR_ATTRIBUTES_OR_VALUES;
	  ippCopyAttribute(client->response, attr, 0);
	}
      }
      else if (!strcmp(attrname, "notify-natural-language"))
      {
        language = ippGetString(attr, 0, NULL);

        if (ippGetValueTag(attr) !=  IPP_TAG_LANGUAGE || ippGetCount(attr) != 1)
        {
	  status = IPP_STATUS_ERROR_ATTRIBUTES_OR_VALUES;
	  ippCopyAttribute(client->response, attr, 0);
	}
      }
      else if (!strcmp(attrname, "notify-user-data"))
      {
        if (ippGetValueTag(attr) != IPP_TAG_STRING || ippGetCount(attr) != 1 || (data = ippGetOctetString(attr, 0, &datalen)) == NULL || datalen > 63)
	{
	  status = IPP_STATUS_ERROR_ATTRIBUTES_OR_VALUES;
	  ippCopyAttribute(client->response, attr, 0);
	}
      }
      else if (!strcmp(attrname, "notify-events"))
      {
        if (ippGetValueTag(attr) != IPP_TAG_KEYWORD)
	{
	  status = IPP_STATUS_ERROR_ATTRIBUTES_OR_VALUES;
	  ippCopyAttribute(client->response, attr, 0);
	}
	else
          events = _papplSubscriptionEventImport(attr);
      }
      else if (!strcmp(attrname, "notify-lease-duration"))
      {
        if (ippGetValueTag(attr) != IPP_TAG_INTEGER || ippGetCount(attr) != 1 || (lease = ippGetInteger(attr, 0)) < 0)
	{
	  status = IPP_STATUS_ERROR_ATTRIBUTES_OR_VALUES;
	  ippCopyAttribute(client->response, attr, 0);
	}
      }
      else if (!strcmp(attrname, "notify-time-interval"))
      {
        if (ippGetValueTag(attr) != IPP_TAG_INTEGER || ippGetCount(attr) != 1 || (interval = ippGetInteger(attr, 0)) < 0)
	{
	  status = IPP_STATUS_ERROR_ATTRIBUTES_OR_VALUES;
	  ippCopyAttribute(client->response, attr, 0);
	}
      }

      attr = ippGetNextAttribute(client->request);
    }

    if (!pull_method || events == PAPPL_EVENT_NONE)
      status = IPP_STATUS_ERROR_BAD_REQUEST;

    if (num_subs > 1)
      ippAddSeparator(client->response);

    if (status)
    {
      // Just return a status code since something was wrong with this request...
      ippAddInteger(client->response, IPP_TAG_SUBSCRIPTION, IPP_TAG_ENUM, "notify-status-code", status);
    }
    else if ((sub = papplSubscriptionCreate(client->system, client->printer, client->job, 0, events, username, language, data, (size_t)datalen, interval, lease)) != NULL)
    {
      // Return the subscription ID for this one...
      ippAddInteger(client->response, IPP_TAG_SUBSCRIPTION, IPP_TAG_INTEGER, "notify-subscription-id", sub->subscription_id);
      ok_subs ++;
    }
    else
    {
      // Return a status code indicating that we weren't able to create the
      // subscription for an internal reason...
      ippAddInteger(client->response, IPP_TAG_SUBSCRIPTION, IPP_TAG_ENUM, "notify-status-code", IPP_STATUS_ERROR_INTERNAL);
    }
  }

  // If we weren't able to create all of the requested subscriptions, return an
  // appropriate status code...
  if (ok_subs == 0 && num_subs != 0)
    ippSetStatusCode(client->response, IPP_STATUS_ERROR_IGNORED_ALL_SUBSCRIPTIONS);
  else if (ok_subs != num_subs)
    ippSetStatusCode(client->response, IPP_STATUS_OK_IGNORED_SUBSCRIPTIONS);
}


//
// '_papplSubscriptionIPPGetAttributes()' - Get subscription attributes.
//

void
_papplSubscriptionIPPGetAttributes(
    pappl_client_t *client)		// I - Client
{
  pappl_subscription_t	*sub;		// Subscription
  cups_array_t		*ra;		// Requested attributes


  // Authorize access...
  if (client->printer)
  {
    if (!_papplPrinterIsAuthorized(client))
      return;
  }
  else
  {
    http_status_t	auth_status;	// Authorization status

    // Verify the connection is authorized...
    if ((auth_status = papplClientIsAuthorized(client)) != HTTP_STATUS_CONTINUE)
    {
      papplClientRespond(client, auth_status, NULL, NULL, 0, 0);
      return;
    }
  }

  // Find the subscription...
  if ((sub = find_subscription(client)) == NULL)
    return;

  // Return attributes...
  ra = ippCreateRequestedArray(client->request);

  papplClientRespondIPP(client, IPP_STATUS_OK, NULL);

  _papplRWLockRead(sub);
  _papplCopyAttributes(client->response, sub->attrs, ra, IPP_TAG_SUBSCRIPTION, 0);
  _papplRWUnlock(sub);

  cupsArrayDelete(ra);
}


//
// '_papplSubscriptionIPPGetNotifications()' - Get event notifications.
//

void
_papplSubscriptionIPPGetNotifications(
    pappl_client_t *client)		// I - Client
{
  ipp_attribute_t	*sub_ids,	// notify-subscription-ids
			*seq_nums;	// notify-sequence-numbers
  pappl_subscription_t	*sub;		// Current subscription
  bool			notify_wait;	// Wait for events?
  cups_len_t		i,		// Looping vars
			count;		// Number of IDs
  int			seq_num;	// Sequence number
  ipp_t			*event;		// Current event
  int			num_events = 0;	// Number of events returned


  // Authorize access...
  if (client->printer)
  {
    if (!_papplPrinterIsAuthorized(client))
      return;
  }
  else
  {
    http_status_t	auth_status;	// Authorization status

    // Verify the connection is authorized...
    if ((auth_status = papplClientIsAuthorized(client)) != HTTP_STATUS_CONTINUE)
    {
      papplClientRespond(client, auth_status, NULL, NULL, 0, 0);
      return;
    }
  }

  // Get request attributes...
  if ((sub_ids = ippFindAttribute(client->request, "notify-subscription-ids", IPP_TAG_INTEGER)) == NULL)
  {
    papplClientRespondIPP(client, IPP_STATUS_ERROR_BAD_REQUEST, "Missing \"notify-subscription-ids\" attribute.");
    return;
  }

  count       = ippGetCount(sub_ids);
  seq_nums    = ippFindAttribute(client->request, "notify-sequence-numbers", IPP_TAG_INTEGER);
  notify_wait = ippGetBoolean(ippFindAttribute(client->request, "notify-wait", IPP_TAG_BOOLEAN), 0);

  if (seq_nums && count != ippGetCount(seq_nums))
  {
    papplClientRespondIPP(client, IPP_STATUS_ERROR_BAD_REQUEST, "The \"notify-subscription-ids\" and \"notify-sequence-numbers\" attributes have different lengths.");
    return;
  }

  do
  {
    for (i = 0; i < count; i ++)
    {
      if ((sub = papplSystemFindSubscription(client->system, ippGetInteger(sub_ids, i))) == NULL)
      {
        papplClientRespondIPP(client, IPP_STATUS_ERROR_NOT_FOUND, "Subscription #%d was not found.", ippGetInteger(sub_ids, i));
        ippAddInteger(client->response, IPP_TAG_UNSUPPORTED_GROUP, IPP_TAG_INTEGER, "notify-subscription-ids", ippGetInteger(sub_ids, i));
	break;
      }

      _papplRWLockRead(sub);

      seq_num = ippGetInteger(seq_nums, i);
      if (seq_num < sub->first_sequence)
	seq_num = sub->first_sequence;

      if (seq_num > sub->last_sequence)
      {
        // No more events...
        _papplRWUnlock(sub);
	continue;
      }

      // Copy events to the output...
      for (event = (ipp_t *)cupsArrayGetElement(sub->events, (cups_len_t)(seq_num - sub->first_sequence)); event; event = (ipp_t *)cupsArrayGetNext(sub->events))
      {
	if (num_events == 0)
	{
	  // This is the first event in the notification...
	  papplClientRespondIPP(client, IPP_STATUS_OK, NULL);
	  ippAddInteger(client->response, IPP_TAG_OPERATION, IPP_TAG_INTEGER, "notify-get-interval", 30);
	  if (client->printer)
	    ippAddInteger(client->response, IPP_TAG_OPERATION, IPP_TAG_INTEGER, "printer-up-time", (int)(time(NULL) - client->printer->start_time));
	  else
	    ippAddInteger(client->response, IPP_TAG_OPERATION, IPP_TAG_INTEGER, "system-up-time", (int)(time(NULL) - client->system->start_time));
	}
	else
	{
	  // Add a separator between attribute groups...
	  ippAddSeparator(client->response);
	}

	ippCopyAttributes(client->response, event, 0, NULL, NULL);
	num_events ++;
      }

      _papplRWUnlock(sub);
    }

    if (i < count || !notify_wait)
    {
      // Stop looping...
      break;
    }
    else if (num_events == 0)
    {
      // Wait up to 30 seconds for more events...
      struct timeval	curtime;	// Current time
      struct timespec	timeout;	// Timeout

      papplLogClient(client, PAPPL_LOGLEVEL_DEBUG, "Waiting for events.");

      gettimeofday(&curtime, NULL);
      timeout.tv_sec  = curtime.tv_sec + 30;
      timeout.tv_nsec = curtime.tv_usec * 1000;

      pthread_mutex_lock(&client->system->subscription_mutex);
      pthread_cond_timedwait(&client->system->subscription_cond, &client->system->subscription_mutex, &timeout);
      pthread_mutex_unlock(&client->system->subscription_mutex);

      papplLogClient(client, PAPPL_LOGLEVEL_DEBUG, "Done waiting for events.");

      // Don't wait again...
      notify_wait = false;
    }
  }
  while (num_events == 0);
}


//
// '_papplSubscriptionIPPList()' - List all subscriptions for a printer or system.
//

void
_papplSubscriptionIPPList(
    pappl_client_t *client)		// I - Client
{
  pappl_subscription_t	*sub;		// Subscription
  cups_array_t		*ra;		// Requested attributes
  bool			my_subs;	// my-subscriptions value
  int			job_id,		// notify-job-id value
			limit,		// limit value, if any
			count = 0;	// Number of subscriptions reported
  const char		*username;	// Most authenticated user name


  // Authorize access...
  if (client->printer)
  {
    if (!_papplPrinterIsAuthorized(client))
      return;
  }
  else
  {
    http_status_t	auth_status;	// Authorization status

    // Verify the connection is authorized...
    if ((auth_status = papplClientIsAuthorized(client)) != HTTP_STATUS_CONTINUE)
    {
      papplClientRespond(client, auth_status, NULL, NULL, 0, 0);
      return;
    }
  }

  // Get request attributes...
  job_id  = ippGetInteger(ippFindAttribute(client->request, "notify-job-id", IPP_TAG_INTEGER), 0);
  limit   = ippGetInteger(ippFindAttribute(client->request, "limit", IPP_TAG_INTEGER), 0);
  my_subs = ippGetBoolean(ippFindAttribute(client->request, "my-subscriptions", IPP_TAG_BOOLEAN), 0);
  ra      = ippCreateRequestedArray(client->request);

  if (client->username[0])
    username = client->username;
  else if ((username = ippGetString(ippFindAttribute(client->request, "requesting-user-name", IPP_TAG_NAME), 0, NULL)) == NULL)
    username = "anonymous";

  papplClientRespondIPP(client, IPP_STATUS_OK, NULL);
  pthread_rwlock_rdlock(&client->system->rwlock);

  for (sub = (pappl_subscription_t *)cupsArrayGetFirst(client->system->subscriptions); sub; sub = (pappl_subscription_t *)cupsArrayGetNext(client->system->subscriptions))
  {
    if ((job_id > 0 && (!sub->job || sub->job->job_id != job_id)) || (job_id <= 0 && sub->job))
      continue;

    if (my_subs && strcmp(username, sub->username))
      continue;

    if (count > 0)
      ippAddSeparator(client->response);

    _papplRWLockRead(sub);
    _papplCopyAttributes(client->response, sub->attrs, ra, IPP_TAG_SUBSCRIPTION, 0);
    _papplRWUnlock(sub);

    count ++;
    if (limit > 0 && count >= limit)
      break;
  }
  pthread_rwlock_unlock(&client->system->rwlock);

  cupsArrayDelete(ra);
}


//
// '_papplSubscriptionIPPRenew()' - Renew a subscription.
//

void
_papplSubscriptionIPPRenew(
    pappl_client_t *client)		// I - Client
{
  pappl_subscription_t	*sub;		// Subscription
  ipp_attribute_t	*attr;		// "notify-lease-duration" attribute
  int			lease;		// Lease duration


  // Authorize access...
  if (client->printer)
  {
    if (!_papplPrinterIsAuthorized(client))
      return;
  }
  else
  {
    http_status_t	auth_status;	// Authorization status

    // Verify the connection is authorized...
    if ((auth_status = papplClientIsAuthorized(client)) != HTTP_STATUS_CONTINUE)
    {
      papplClientRespond(client, auth_status, NULL, NULL, 0, 0);
      return;
    }
  }

  // Find the subscription...
  if ((sub = find_subscription(client)) == NULL)
    return;

  // Renew it...
  if ((attr = ippFindAttribute(client->request, "notify-lease-duration", IPP_TAG_ZERO)) == NULL)
  {
    lease = PAPPL_LEASE_DEFAULT;
  }
  else if (ippGetGroupTag(attr) != IPP_TAG_OPERATION || ippGetValueTag(attr) != IPP_TAG_INTEGER || ippGetCount(attr) != 1 || (lease = ippGetInteger(attr, 0)) < 0)
  {
    papplClientRespondIPP(client, IPP_STATUS_ERROR_BAD_REQUEST, "Bad \"notify-lease-duration\" attribute.");
    return;
  }

  papplSubscriptionRenew(sub, lease);
  papplClientRespondIPP(client, IPP_STATUS_OK, NULL);
}


//
// 'find_subscription()' - Find the referenced subscription.
//

static pappl_subscription_t *		// O - Subscription or `NULL` on error
find_subscription(
    pappl_client_t *client)		// I - Client
{
  ipp_attribute_t	*sub_id;	// "subscription-id" attribute
  pappl_subscription_t	*sub;		// Subscription


  if ((sub_id = ippFindAttribute(client->request, "notify-subscription-id", IPP_TAG_ZERO)) == NULL)
  {
    papplClientRespondIPP(client, IPP_STATUS_ERROR_BAD_REQUEST, "Missing \"notify-subscription-id\" attribute.");
    return (NULL);
  }
  else if (ippGetGroupTag(sub_id) != IPP_TAG_OPERATION || ippGetValueTag(sub_id) != IPP_TAG_INTEGER || ippGetCount(sub_id) != 1 || ippGetInteger(sub_id, 0) < 1)
  {
    papplClientRespondIPP(client, IPP_STATUS_ERROR_BAD_REQUEST, "Bad \"notify-subscription-id\" attribute.");
    return (NULL);
  }
  else if ((sub = papplSystemFindSubscription(client->system, ippGetInteger(sub_id, 0))) == NULL)
  {
    papplClientRespondIPP(client, IPP_STATUS_ERROR_NOT_FOUND, "Subscription #%d was not found.", ippGetInteger(sub_id, 0));
    return (NULL);
  }
  else if (client->printer && sub->printer != client->printer)
  {
    papplClientRespondIPP(client, IPP_STATUS_ERROR_NOT_POSSIBLE, "Subscription #%d is not assigned to the specified printer.", ippGetInteger(sub_id, 0));
    return (NULL);
  }

  return (sub);
}
