//
// Public job header file for the Printer Application Framework
//
// Copyright © 2019-2023 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#ifndef _PAPPL_JOB_H_
#  define _PAPPL_JOB_H_
#  include "base.h"
#  ifdef __cplusplus
extern "C" {
#  endif // __cplusplus


//
// Constants...
//

enum pappl_jreason_e			// IPP "job-state-reasons" bit values
{
  PAPPL_JREASON_NONE = 0x00000000,			// 'none'
  PAPPL_JREASON_ABORTED_BY_SYSTEM = 0x00000001,		// 'aborted-by-system'
  PAPPL_JREASON_COMPRESSION_ERROR = 0x00000002,		// 'compression-error'
  PAPPL_JREASON_DOCUMENT_FORMAT_ERROR = 0x00000004,	// 'document-format-error'
  PAPPL_JREASON_DOCUMENT_PASSWORD_ERROR = 0x00000008,	// 'document-password-error'
  PAPPL_JREASON_DOCUMENT_PERMISSION_ERROR = 0x00000010,	// 'document-permission-error'
  PAPPL_JREASON_DOCUMENT_UNPRINTABLE_ERROR = 0x00000020,// 'document-unprintable-error'
  PAPPL_JREASON_ERRORS_DETECTED = 0x00000040,		// 'errors-detected'
  PAPPL_JREASON_JOB_CANCELED_AT_DEVICE = 0x00000080,	// 'job-canceled-at-device'
  PAPPL_JREASON_JOB_CANCELED_BY_USER = 0x00000100,	// 'job-canceled-by-user'
  PAPPL_JREASON_JOB_COMPLETED_SUCCESSFULLY = 0x00000200,// 'job-completed-successfully'
  PAPPL_JREASON_JOB_COMPLETED_WITH_ERRORS = 0x00000400,	// 'job-completed-with-errors'
  PAPPL_JREASON_JOB_COMPLETED_WITH_WARNINGS = 0x00000800,// 'job-completed-with-warnings'
  PAPPL_JREASON_JOB_DATA_INSUFFICIENT = 0x00001000,	// 'job-data-insufficient'
  PAPPL_JREASON_JOB_INCOMING = 0x000002000,		// 'job-incoming'
  PAPPL_JREASON_JOB_PRINTING = 0x00004000,		// 'job-printing'
  PAPPL_JREASON_JOB_QUEUED = 0x00008000,		// 'job-queued'
  PAPPL_JREASON_JOB_SPOOLING = 0x00010000,		// 'job-spooling'
  PAPPL_JREASON_PRINTER_STOPPED = 0x00020000,		// 'printer-stopped'
  PAPPL_JREASON_PRINTER_STOPPED_PARTLY = 0x00040000,	// 'printer-stopped-partly'
  PAPPL_JREASON_PROCESSING_TO_STOP_POINT = 0x00080000,	// 'processing-to-stop-point'
  PAPPL_JREASON_QUEUED_IN_DEVICE = 0x00100000,		// 'queued-in-device'
  PAPPL_JREASON_WARNINGS_DETECTED = 0x00200000,		// 'warnings-detected'
  PAPPL_JREASON_JOB_HOLD_UNTIL_SPECIFIED = 0x00400000,	// 'job-hold-until-specified'
  PAPPL_JREASON_JOB_CANCELED_AFTER_TIMEOUT = 0x00800000,// 'job-canceled-after-timeout'
  PAPPL_JREASON_JOB_FETCHABLE = 0x01000000,		// 'job-fetchable'
  PAPPL_JREASON_JOB_SUSPENDED_FOR_APPROVAL = 0x02000000	// 'job-suspended-for-approval'

};
typedef unsigned int pappl_jreason_t;	// Bitfield for IPP "job-state-reasons" values


//
// Functions...
//

extern void		papplJobCancel(pappl_job_t *job) _PAPPL_PUBLIC;
extern pappl_pr_options_t *papplJobCreatePrintOptions(pappl_job_t *job, unsigned num_pages, bool color) _PAPPL_PUBLIC;
extern pappl_sc_options_t *papplJobCreateScanOptions(pappl_job_t *job) _PAPPL_PUBLIC;
extern pappl_job_t	*papplJobCreateWithFile(pappl_printer_t *printer, const char *username, const char *format, const char *job_name, int num_options, cups_option_t *options, const char *filename);

extern void		papplJobDeletePrintOptions(pappl_pr_options_t *options);
extern void		papplJobDeleteScanOptions(pappl_sc_options_t *options);

extern bool		papplJobFilterImage(pappl_job_t *job, pappl_device_t *device, pappl_pr_options_t *options, const unsigned char *pixels, int width, int height, int depth, int ppi, bool smoothing) _PAPPL_PUBLIC;

extern ipp_attribute_t	*papplJobGetAttribute(pappl_job_t *job, const char *name) _PAPPL_PUBLIC;
extern int		papplJobGetCopies(pappl_job_t *job) _PAPPL_PUBLIC;
extern int		papplJobGetCopiesCompleted(pappl_job_t *job) _PAPPL_PUBLIC;
extern void		*papplJobGetData(pappl_job_t *job) _PAPPL_PUBLIC;
extern const char	*papplJobGetFilename(pappl_job_t *job) _PAPPL_PUBLIC;
extern const char	*papplJobGetFormat(pappl_job_t *job) _PAPPL_PUBLIC;
extern int		papplJobGetID(pappl_job_t *job) _PAPPL_PUBLIC;
extern int		papplJobGetImpressions(pappl_job_t *job) _PAPPL_PUBLIC;
extern int		papplJobGetImpressionsCompleted(pappl_job_t *job) _PAPPL_PUBLIC;
extern const char	*papplJobGetMessage(pappl_job_t *job) _PAPPL_PUBLIC;
extern const char	*papplJobGetName(pappl_job_t *job) _PAPPL_PUBLIC;
extern pappl_printer_t	*papplJobGetPrinter(pappl_job_t *job) _PAPPL_PUBLIC;
extern pappl_scanner_t	*papplJobGetScanner(pappl_job_t *job) _PAPPL_PUBLIC;
extern pappl_jreason_t	papplJobGetReasons(pappl_job_t *job) _PAPPL_PUBLIC;
extern ipp_jstate_t	papplJobGetState(pappl_job_t *job) _PAPPL_PUBLIC;
extern time_t		papplJobGetTimeCompleted(pappl_job_t *job) _PAPPL_PUBLIC;
extern time_t		papplJobGetTimeCreated(pappl_job_t *job) _PAPPL_PUBLIC;
extern time_t		papplJobGetTimeProcessed(pappl_job_t *job) _PAPPL_PUBLIC;
extern const char	*papplJobGetUsername(pappl_job_t *job) _PAPPL_PUBLIC;

extern bool		papplJobHold(pappl_job_t *job, const char *username, const char *until, time_t until_time) _PAPPL_PUBLIC;

extern bool		papplJobIsCanceled(pappl_job_t *job) _PAPPL_PUBLIC;

extern int		papplJobOpenFile(pappl_job_t *job, char *fname, size_t fnamesize, const char *directory, const char *ext, const char *mode) _PAPPL_PUBLIC;

extern bool		papplJobRelease(pappl_job_t *job, const char *username) _PAPPL_PUBLIC;
extern void		papplJobResume(pappl_job_t *job, pappl_jreason_t remove) _PAPPL_PUBLIC;
extern bool		papplJobRetain(pappl_job_t *job, const char *username, const char *until, int until_interval, time_t until_time) _PAPPL_PUBLIC;

extern void		papplJobSetCopiesCompleted(pappl_job_t *job, int add) _PAPPL_PUBLIC;
extern void		papplJobSetData(pappl_job_t *job, void *data) _PAPPL_PUBLIC;
extern void		papplJobSetImpressions(pappl_job_t *job, int impressions) _PAPPL_PUBLIC;
extern void		papplJobSetImpressionsCompleted(pappl_job_t *job, int add) _PAPPL_PUBLIC;
extern void		papplJobSetMessage(pappl_job_t *job, const char *message, ...) _PAPPL_PUBLIC _PAPPL_FORMAT(2,3);
extern void		papplJobSetReasons(pappl_job_t *job, pappl_jreason_t add, pappl_jreason_t remove) _PAPPL_PUBLIC;
extern void		papplJobSuspend(pappl_job_t *job, pappl_jreason_t add) _PAPPL_PUBLIC;


#  ifdef __cplusplus
}
#  endif // __cplusplus
#endif // !_PAPPL_JOB_H_
