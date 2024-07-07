//
// Private subscription header file for the Printer Application Framework
//
// Copyright © 2022-2024 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#ifndef _PAPPL_SUBSCRIPTION_PRIVATE_H_
#  define _PAPPL_SUBSCRIPTION_PRIVATE_H_
#  include "base-private.h"
#  include "subscription.h"
#  ifdef __cplusplus
extern "C" {
#  endif // __cplusplus


//
// Constants...
//

#  define PAPPL_LEASE_DEFAULT	3600		// Default lease duration in seconds (1 hour)
#  define PAPPL_LEASE_MAX	86400		// Maximum lease duration in seconds (1 day)
#  define PAPPL_MAX_EVENTS	100		// Maximum events per subscription


//
// Types...
//

struct _pappl_subscription_s		// Subscription data
{
  pthread_rwlock_t	rwlock;			// Reader/writer lock
  int			subscription_id;	// Subscription ID
#  ifdef DEBUG
  char			name[128];		// Subscription name (for debugging)
#  endif // DEBUG
  pappl_event_t		mask;			// IPP "notify-events" bit field
  pappl_printer_t	*printer;		// Printer, if any
  pappl_scanner_t *scanner; // Scanner, if any
  pappl_job_t		*job;			// Job, if any
  ipp_t			*attrs;			// Attributes
  char			*language,		// Language for notifications
			*username,		// Owner
			*uuid;			// UUID
  time_t		expire;			// Expiration date/time, if any
  int			lease,			// Lease duration
			interval;		// Notification interval
  int			first_sequence,		// First notify-sequence-number used
			last_sequence;		// Last notify-sequence-number used
  cups_array_t		*events;		// Events (ipp_t *'s)
  bool			is_canceled;		// Has this subscription been canceled?
};


//
// Globals...
//

extern const char * const _papplEvents[31];


//
// Functions...
//

extern void		_papplSubscriptionDelete(pappl_subscription_t *sub) _PAPPL_PRIVATE;
extern ipp_attribute_t	*_papplSubscriptionEventExport(ipp_t *ipp, const char *name, ipp_tag_t group_tag, pappl_event_t value) _PAPPL_PRIVATE;
extern pappl_event_t	_papplSubscriptionEventImport(ipp_attribute_t *value) _PAPPL_PRIVATE;
extern const char	*_papplSubscriptionEventString(pappl_event_t value) _PAPPL_PRIVATE;
extern pappl_event_t	_papplSubscriptionEventValue(const char *value) _PAPPL_PRIVATE;

extern void		_papplSubscriptionIPPCancel(pappl_client_t *client) _PAPPL_PRIVATE;
extern void		_papplSubscriptionIPPCreate(pappl_client_t *client) _PAPPL_PRIVATE;
extern void		_papplSubscriptionIPPGetAttributes(pappl_client_t *client) _PAPPL_PRIVATE;
extern void		_papplSubscriptionIPPGetNotifications(pappl_client_t *client) _PAPPL_PRIVATE;
extern void		_papplSubscriptionIPPList(pappl_client_t *client) _PAPPL_PRIVATE;
extern void		_papplSubscriptionIPPRenew(pappl_client_t *client) _PAPPL_PRIVATE;


#  ifdef __cplusplus
}
#  endif // __cplusplus
#endif // !_PAPPL_SUBSCRIPTION_PRIVATE_H_
