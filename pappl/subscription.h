//
// Public subscription header file for the Printer Application Framework
//
// Copyright © 2022 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#ifndef _PAPPL_SUBSCRIPTION_H_
#  define _PAPPL_SUBSCRIPTION_H_
#  include "base.h"
#  ifdef __cplusplus
extern "C" {
#  endif // __cplusplus


//
// Types...
//

enum pappl_event_e			// IPP "notify-events" bit values
{
  PAPPL_EVENT_DOCUMENT_COMPLETED = 0x00000001,
					// 'document-completed'
  PAPPL_EVENT_DOCUMENT_CONFIG_CHANGED = 0x00000002,
					// 'document-config-changed'
  PAPPL_EVENT_DOCUMENT_CREATED = 0x00000004,
					// 'document-created'
  PAPPL_EVENT_DOCUMENT_FETCHABLE = 0x00000008,
					// 'document-fetchable'
  PAPPL_EVENT_DOCUMENT_STATE_CHANGED = 0x00000010,
					// 'document-state-changed'
  PAPPL_EVENT_DOCUMENT_STOPPED = 0x00000020,
					// 'document-stopped'

  PAPPL_EVENT_JOB_COMPLETED = 0x00000040,
					// 'job-completed'
  PAPPL_EVENT_JOB_CONFIG_CHANGED = 0x00000080,
					// 'job-config-changed'
  PAPPL_EVENT_JOB_CREATED = 0x00000100,
					// 'job-created'
  PAPPL_EVENT_JOB_FETCHABLE = 0x00000200,
					// 'job-fetchable'
  PAPPL_EVENT_JOB_PROGRESS = 0x00000400,
					// 'job-progress'
  PAPPL_EVENT_JOB_STATE_CHANGED = 0x00000800,
					// 'job-state-changed'
  PAPPL_EVENT_JOB_STOPPED = 0x00001000,
					// 'job-stopped'

  PAPPL_EVENT_PRINTER_CONFIG_CHANGED = 0x00002000,
					// 'printer-config-changed'
  PAPPL_EVENT_PRINTER_FINISHINGS_CHANGED = 0x00004000,
					// 'printer-finishings-changed'
  PAPPL_EVENT_PRINTER_MEDIA_CHANGED = 0x00008000,
					// 'printer-media-changed'
  PAPPL_EVENT_PRINTER_QUEUE_ORDER_CHANGED = 0x00010000,
					// 'printer-queue-order-changed'
  PAPPL_EVENT_PRINTER_RESTARTED = 0x00020000,
					// 'printer-restarted'
  PAPPL_EVENT_PRINTER_SHUTDOWN = 0x00040000,
					// 'printer-shutdown'
  PAPPL_EVENT_PRINTER_STATE_CHANGED = 0x00080000,
					// 'printer-state-changed'
  PAPPL_EVENT_PRINTER_STOPPED = 0x00100000,
					// 'printer-stopped'

  PAPPL_EVENT_RESOURCE_CANCELED = 0x00200000,
					// 'resource-canceled'
  PAPPL_EVENT_RESOURCE_CONFIG_CHANGED = 0x00400000,
					// 'resource-config-changed'
  PAPPL_EVENT_RESOURCE_CREATED = 0x00800000,
					// 'resource-created'
  PAPPL_EVENT_RESOURCE_INSTALLED = 0x01000000,
					// 'resource-installed'
  PAPPL_EVENT_RESOURCE_STATE_CHANGED = 0x02000000,
					// 'resource-state-changed'

  PAPPL_EVENT_PRINTER_CREATED = 0x04000000,
					// 'printer-created'
  PAPPL_EVENT_PRINTER_DELETED = 0x08000000,
					// 'printer-deleted'

  PAPPL_EVENT_SYSTEM_CONFIG_CHANGED = 0x10000000,
					// 'system-config-changed'
  PAPPL_EVENT_SYSTEM_STATE_CHANGED = 0x20000000,
					// 'system-state-changed'
  PAPPL_EVENT_SYSTEM_STOPPED = 0x40000000,
					// 'system-stopped'

  PAPPL_EVENT_NONE = 0x00000000,	// 'none'
  PAPPL_EVENT_DOCUMENT_ALL = 0x0000003f,// All 'document-xxx' events
  PAPPL_EVENT_DOCUMENT_STATE_ALL = 0x00000037,
					// All 'document-xxx' state events
  PAPPL_EVENT_JOB_ALL = 0x00001fc0,
					// All 'job-xxx' events
  PAPPL_EVENT_JOB_STATE_ALL = 0x00001940,
					// All 'job-xxx' state events
  PAPPL_EVENT_PRINTER_ALL = 0x001fe000,
					// All 'printer-xxx' events
  PAPPL_EVENT_PRINTER_CONFIG_ALL = 0x0000e000,
					// All 'printer-xxx' configuration events
  PAPPL_EVENT_PRINTER_STATE_ALL = 0x001e0000,
					// All 'printer-xxx' state events
  PAPPL_EVENT_ALL = 0x7fffffff,	// All events

  // New scanner-specific events
  PAPPL_EVENT_SCANNER_CONFIG_CHANGED = 0x80000000, // 'scanner-config-changed'
  PAPPL_EVENT_SCANNER_STATE_CHANGED = 0x100000000,  // 'scanner-state-changed'
  PAPPL_EVENT_SCANNER_STOPPED = 0x600000000,        // 'scanner-stopped',
  PAPPL_EVENT_SCANNER_ALL = 0x180000000 // All 'scanner' events

};
typedef unsigned long long int pappl_event_t;	// Bitfield for IPP/ESCL "notify-events" attribute
typedef void (*pappl_event_cb_t)(pappl_system_t *system, pappl_printer_t *printer, pappl_job_t *job, pappl_event_t event, void *data);
					// System event callback
typedef void (*pappl_scanner_event_cb_t)(pappl_system_t *system, pappl_scanner_t *scanner, pappl_job_t *job, pappl_event_t event, void *data);
          // System scanner event callback


//
// Functions...
//

extern void		papplSubscriptionCancel(pappl_subscription_t *sub) _PAPPL_PUBLIC;

extern pappl_subscription_t *papplSubscriptionCreate(pappl_system_t *system, pappl_printer_t *printer, pappl_job_t *job, int sub_id, pappl_event_t events, const char *username, const char *natural_language, const void *data, size_t datalen, int interval, int lease) _PAPPL_PUBLIC;

extern pappl_event_t	papplSubscriptionGetEvents(pappl_subscription_t *sub) _PAPPL_PUBLIC;
extern int		papplSubscriptionGetID(pappl_subscription_t *sub) _PAPPL_PUBLIC;
extern pappl_job_t	*papplSubscriptionGetJob(pappl_subscription_t *sub) _PAPPL_PUBLIC;
extern pappl_printer_t	*papplSubscriptionGetPrinter(pappl_subscription_t *sub) _PAPPL_PUBLIC;
extern const char	*papplSubscriptionGetUsername(pappl_subscription_t *sub) _PAPPL_PUBLIC;

extern void		papplSubscriptionRenew(pappl_subscription_t *sub, int lease) _PAPPL_PUBLIC;


#  ifdef __cplusplus
}
#  endif // __cplusplus
#endif // !_PAPPL_SUBSCRIPTION_H_
