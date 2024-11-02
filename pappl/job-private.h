//
// Private job header file for the Printer Application Framework
//
// Copyright © 2019-2023 by Michael R Sweet.
// Copyright © 2010-2019 by Apple Inc.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#ifndef _PAPPL_JOB_PRIVATE_H_
#  define _PAPPL_JOB_PRIVATE_H_
#  include "base-private.h"
#  include "job.h"
#  include "log.h"


//
// Types and structures...
//

struct _pappl_job_s			// Job data
{
  pthread_rwlock_t	rwlock;			// Reader/writer lock
  pappl_system_t	*system;		// Containing system
  pappl_printer_t	*printer;		// Containing printer
  pappl_scanner_t	*scanner;		// Containing scanner
  int			job_id;			// "job-id" value
  const char		*name,			// "job-name" value
			*username,		// "job-originating-user-name" value
			*format;		// "document-format" value
  ipp_jstate_t		state;			// "job-state" value
  pappl_jreason_t	state_reasons;		// "job-state-reasons" values
  bool			is_canceled;		// Has this job been canceled?
  char			*message;		// "job-state-message" value
  pappl_loglevel_t	msglevel;		// "job-state-message" log level
  time_t		created,		// "[date-]time-at-creation" value
			processing,		// "[date-]time-at-processing" value
			completed,		// "[date-]time-at-completed" value
			hold_until,		// "job-hold-until[-time]" value
			retain_until;		// "job-retain-until[-interval,-time]" value
  int			copies,			// "copies" value
			copcompleted,		// "copies-completed" value
			impressions,		// "job-impressions" value
			impcompleted;		// "job-impressions-completed" value
  ipp_t			*attrs;			// Static attributes
  char			*filename;		// Print file name
  int			fd;			// Print file descriptor
  bool			streaming;		// Streaming job?
  void			*data;			// Per-job driver data
};


//
// Functions...
//

extern int		_papplJobCompareActive(pappl_job_t *a, pappl_job_t *b) _PAPPL_PRIVATE;
extern int		_papplJobCompareAll(pappl_job_t *a, pappl_job_t *b) _PAPPL_PRIVATE;
extern int		_papplJobCompareCompleted(pappl_job_t *a, pappl_job_t *b) _PAPPL_PRIVATE;
extern void		_papplJobCopyAttributesNoLock(pappl_job_t *job, pappl_client_t *client, cups_array_t *ra) _PAPPL_PRIVATE;
extern void		_papplJobCopyDocumentData(pappl_client_t *client, pappl_job_t *job) _PAPPL_PRIVATE;
extern void		_papplJobCopyStateNoLock(pappl_job_t *job, ipp_tag_t group_tag, ipp_t *ipp, cups_array_t *ra) _PAPPL_PRIVATE;
extern pappl_job_t	*_papplJobCreate(pappl_printer_t *printer, int job_id, const char *username, const char *format, const char *job_name, ipp_t *attrs) _PAPPL_PRIVATE;
extern pappl_job_t  *_papplScanJobCreate(pappl_scanner_t *scanner, int job_id, const char *username, const char *format, const char *job_name) _PAPPL_PRIVATE; // no extra attributes required
extern void		_papplJobDelete(pappl_job_t *job) _PAPPL_PRIVATE;
#  ifdef HAVE_LIBJPEG
extern bool		_papplJobFilterJPEG(pappl_job_t *job, pappl_device_t *device, void *data);
#  endif // HAVE_LIBJPEG
#  ifdef HAVE_LIBPNG
extern bool		_papplJobFilterPNG(pappl_job_t *job, pappl_device_t *device, void *data);
#  endif // HAVE_LIBPNG
extern bool		_papplJobHoldNoLock(pappl_job_t *job, const char *username, const char *until, time_t until_time) _PAPPL_PRIVATE;
extern void		*_papplJobProcess(pappl_job_t *job) _PAPPL_PRIVATE;
extern void		_papplJobProcessIPP(pappl_client_t *client) _PAPPL_PRIVATE;
// TODO
extern void   _papplJobProcessESCL(pappl_client_t *client) _PAPPL_PRIVATE;

extern void		_papplJobProcessRaster(pappl_job_t *job, pappl_client_t *client) _PAPPL_PRIVATE;
extern const char	*_papplJobReasonString(pappl_jreason_t reason) _PAPPL_PRIVATE;
extern void		_papplJobReleaseNoLock(pappl_job_t *job, const char *username) _PAPPL_PRIVATE;
extern void		_papplJobRemoveFile(pappl_job_t *job) _PAPPL_PRIVATE;
extern bool		_papplJobRetainNoLock(pappl_job_t *job, const char *username, const char *until, int until_interval, time_t until_time) _PAPPL_PRIVATE;
extern void		_papplJobSetRetain(pappl_job_t *job) _PAPPL_PRIVATE;
extern void		_papplJobSetState(pappl_job_t *job, ipp_jstate_t state) _PAPPL_PRIVATE;
extern void		_papplJobSubmitFile(pappl_job_t *job, const char *filename) _PAPPL_PRIVATE;
extern bool		_papplJobValidateDocumentAttributes(pappl_client_t *client) _PAPPL_PRIVATE;


#endif // !_PAPPL_JOB_PRIVATE_H_
