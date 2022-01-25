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
  if (!_papplPrinterIsAuthorized(client))
    return;

  // Find the subscription...
  if ((sub = find_subscription(client)) == NULL)
    return;

  // Cancel it...
  papplSubscriptionCancel(sub);
  papplClientRespondIPP(client, IPP_STATUS_OK, NULL);
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
  if (!_papplPrinterIsAuthorized(client))
    return;

  // Find the subscription...
  if ((sub = find_subscription(client)) == NULL)
    return;

  // Return attributes...
  ra = ippCreateRequestedArray(client->request);

  papplClientRespondIPP(client, IPP_STATUS_OK, NULL);

  pthread_rwlock_rdlock(&sub->rwlock);
  _papplCopyAttributes(client->response, sub->attrs, ra, IPP_TAG_SUBSCRIPTION, 0);
  pthread_rwlock_unlock(&sub->rwlock);

  cupsArrayDelete(ra);
}


//
// '_papplSubscriptionIPPGetNotifications()' - Get event notifications.
//

void
_papplSubscriptionIPPGetNotifications(
    pappl_client_t *client)		// I - Client
{
  // Authorize access...
  if (!_papplPrinterIsAuthorized(client))
    return;

  // TODO: Return events for a subscription
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
  if (!_papplPrinterIsAuthorized(client))
    return;

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

  for (sub = (pappl_subscription_t *)cupsArrayFirst(client->system->subscriptions); sub; sub = (pappl_subscription_t *)cupsArrayNext(client->system->subscriptions))
  {
    if ((job_id > 0 && (!sub->job || sub->job->job_id != job_id)) || (job_id <= 0 && sub->job))
      continue;

    if (my_subs && strcmp(username, sub->username))
      continue;

    if (count > 0)
      ippAddSeparator(client->response);

    pthread_rwlock_rdlock(&sub->rwlock);
    _papplCopyAttributes(client->response, sub->attrs, ra, IPP_TAG_SUBSCRIPTION, 0);
    pthread_rwlock_unlock(&sub->rwlock);

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
  if (!_papplPrinterIsAuthorized(client))
    return;

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


  if ((sub_id = ippFindAttribute(client->request, "subscription-id", IPP_TAG_ZERO)) == NULL)
  {
    papplClientRespondIPP(client, IPP_STATUS_ERROR_BAD_REQUEST, "Missing \"subscription-id\" attribute.");
    return (NULL);
  }
  else if (ippGetGroupTag(sub_id) != IPP_TAG_OPERATION || ippGetValueTag(sub_id) != IPP_TAG_INTEGER || ippGetCount(sub_id) != 1 || ippGetInteger(sub_id, 0) < 1)
  {
    papplClientRespondIPP(client, IPP_STATUS_ERROR_BAD_REQUEST, "Bad \"subscription-id\" attribute.");
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
