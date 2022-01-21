//
// Public subscription header file for the Printer Application Framework
//
// Copyright © 2022 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#ifndef _PAPPL_SUBSCRIPTION_PRIVATE_H_
#  define _PAPPL_SUBSCRIPTION_PRIVATE_H_
#  include "subscription.h"
#  ifdef __cplusplus
extern "C" {
#  endif // __cplusplus


//
// Types...
//

struct _pappl_subscription_s		// Subscription data
{
  pthread_rwlock_t	rwlock;			// Reader/writer lock
  int			subscription_id;	// Subscription ID
  pappl_event_t		mask;			// IPP "notifiy-events" bit field
  pappl_printer_t	*printer;		// Printer, if any
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
  bool			is_deleted;		// Has this subscription been deleted?
};


//
// Functions...
//

extern void		_papplSubscriptionDelete(pappl_subscription_t *sub) _PAPPL_PRIVATE;
extern ipp_attribute_t	*_papplSubscriptionEventExport(ipp_t *ipp, const char *name, ipp_tag_t group_tag, pappl_event_t value) _PAPPL_PRIVATE;
extern pappl_event_t	_papplSubscriptionEventImport(ipp_attribute_t *value) _PAPPL_PRIVATE;
extern const char	*_papplSubscriptionEventString(pappl_event_t value) _PAPPL_PRIVATE;
extern pappl_event_t	_papplSubscriptionEventValue(const char *value) _PAPPL_PRIVATE;


#  ifdef __cplusplus
}
#  endif // __cplusplus
#endif // !_PAPPL_SUBSCRIPTION_PRIVATE_H_
