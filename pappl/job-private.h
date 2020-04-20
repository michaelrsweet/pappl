//
// Private job header file for the Printer Application Framework
//
// Copyright © 2019-2020 by Michael R Sweet.
// Copyright © 2010-2019 by Apple Inc.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#ifndef _PAPPL_JOB_PRIVATE_H_
#  define _PAPPL_JOB_PRIVATE_H_

//
// Include necessary headers...
//

#  include "base-private.h"
#  include "job.h"
#  include "log.h"
#  include <sys/wait.h>

extern char **environ;


//
// Types and structures...
//

struct _pappl_job_s			// Job data
{
  pthread_rwlock_t	rwlock;			// Reader/writer lock
  pappl_system_t	*system;		// Containing system
  pappl_printer_t	*printer;		// Containing printer
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
			completed;		// "[date-]time-at-completed" value
  int			impressions,		// "job-impressions" value
			impcompleted;		// "job-impressions-completed" value
  ipp_t			*attrs;			// Static attributes
  char			*filename;		// Print file name
  int			fd;			// Print file descriptor
  void			*data;			// Per-job driver data
};


//
// Functions...
//

extern int		_papplJobCompareActive(pappl_job_t *a, pappl_job_t *b) _PAPPL_PRIVATE;
extern int		_papplJobCompareAll(pappl_job_t *a, pappl_job_t *b) _PAPPL_PRIVATE;
extern int		_papplJobCompareCompleted(pappl_job_t *a, pappl_job_t *b) _PAPPL_PRIVATE;
extern void		_papplJobDelete(pappl_job_t *job) _PAPPL_PRIVATE;
#  ifdef HAVE_LIBJPEG
extern bool		_papplJobFilterJPEG(pappl_job_t *job, pappl_device_t *device, void *data);
#  endif // HAVE_LIBJPEG
#  ifdef HAVE_LIBPNG
extern bool		_papplJobFilterPNG(pappl_job_t *job, pappl_device_t *device, void *data);
#  endif // HAVE_LIBPNG
extern void		*_papplJobProcess(pappl_job_t *job) _PAPPL_PRIVATE;
extern void		_papplJobProcessRaster(pappl_job_t *job, pappl_client_t *client) _PAPPL_PRIVATE;
extern const char	*_papplJobReasonString(pappl_jreason_t reason) _PAPPL_PRIVATE;
extern void		_papplJobSetState(pappl_job_t *job, ipp_jstate_t state) _PAPPL_PRIVATE;


#endif // !_PAPPL_JOB_PRIVATE_H_
