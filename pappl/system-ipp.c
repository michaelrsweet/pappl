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
// '_papplSubscriptionIPPCancel()' - Cancel a subscription.
//

void
_papplSubscriptionIPPCancel(
    pappl_client_t *client)		// I - Client
{
  // Authorize access...
  if (!_papplPrinterIsAuthorized(client))
    return;

  // TODO: Cancel subscription...
}


//
// '_papplSubscriptionIPPGetAttributes()' - Get subscription attributes.
//

void
_papplSubscriptionIPPGetAttributes(
    pappl_client_t *client)		// I - Client
{
  // Authorize access...
  if (!_papplPrinterIsAuthorized(client))
    return;

  // TODO: Return attributes for a subscription
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
  // Authorize access...
  if (!_papplPrinterIsAuthorized(client))
    return;

  // TODO: List subscriptions from request...
}


//
// '_papplSubscriptionIPPRenew()' - Renew a subscription.
//

void
_papplSubscriptionIPPRenew(
    pappl_client_t *client)		// I - Client
{
  // Authorize access...
  if (!_papplPrinterIsAuthorized(client))
    return;

  // TODO: Renew subscription from request...
}


