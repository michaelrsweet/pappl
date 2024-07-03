//
// Infrastructure proxy functions for the Printer Application Framework
//
// Copyright © 2024 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#include "printer-private.h"
#include "system-private.h"
#include "job-private.h"


//
// Local types...
//

typedef struct _pappl_proxy_job_s	// Proxy job data
{
  pappl_job_t	*job;			// Local job
  int		parent_job_id;		// parent-job-id value
  char		parent_job_uuid[46];	// parent-job-uuid value
} _pappl_proxy_job_t;


//
// Local functions...
//

static int		compare_proxy_jobs(_pappl_proxy_job_t *pja, _pappl_proxy_job_t *pjb, void *data);
static _pappl_proxy_job_t *copy_proxy_job(_pappl_proxy_job_t *pj, void *data);
static void		free_proxy_job(_pappl_proxy_job_t *pj, void *data);
static bool		update_proxy_jobs(pappl_printer_t *printer);


//
// '_papplPrinterRunProxy()' - Run the proxy thread until the printer is deleted or system is shutdown.
//

void *					// O - Thread exit status
_papplPrinterRunProxy(
    pappl_printer_t *printer)		// I - Printer
{
  papplLogPrinter(printer, PAPPL_LOGLEVEL_DEBUG, "Running proxy thread.");

  _papplRWLockWrite(printer);
  printer->proxy_active = true;
  update_proxy_jobs(printer);
  _papplRWUnlock(printer);

  while (!papplPrinterIsDeleted(printer) && papplSystemIsRunning(printer->system))
  {
    // TODO: Get notifications...
    sleep(1);
  }

  _papplRWLockWrite(printer);
  printer->proxy_active = false;
  _papplRWUnlock(printer);

  return (NULL);
}


//
// 'compare_proxy_jobs()' - Compare two proxy jobs.
//

static int				// O - Result of comparison
compare_proxy_jobs(
    _pappl_proxy_job_t *pja,		// I - First proxy job
    _pappl_proxy_job_t *pjb,		// I - Second proxy job
    void               *data)		// I - Callback data (unused)
{
  (void)data;

  return (pjb->parent_job_id - pja->parent_job_id);
}


//
// 'copy_proxy_job()' - Copy a proxy job.
//

static _pappl_proxy_job_t *		// O - New proxy job
copy_proxy_job(_pappl_proxy_job_t *pj,	// I - Proxy job
	       void               *data)// I - Callback data (unused)
{
  _pappl_proxy_job_t *npj;		// New proxy job


  (void)data;

  if ((npj = (_pappl_proxy_job_t *)calloc(1, sizeof(_pappl_proxy_job_t))) != NULL)
    memcpy(npj, pj, sizeof(_pappl_proxy_job_t));

  return (npj);
}


//
// 'free_proxy_job()' - Free a proxy job.
//

static void
free_proxy_job(_pappl_proxy_job_t *pj,	// I - Proxy job
               void               *data)// I - Callback data (unused)
{
  (void)data;

  free(pj);
}


//
// 'update_proxy_jobs()' - Update the available proxy jobs.
//

static bool				// O - `true` on success, `false` on error
update_proxy_jobs(
    pappl_printer_t *printer)		// I - Printer
{
  if (!printer->proxy_jobs)
  {
    pappl_job_t		*job;		// Current job
    ipp_attribute_t	*attr;		// Job attribute
    _pappl_proxy_job_t	pj;		// Proxy job

    // Create the proxy jobs array...
    printer->proxy_jobs = cupsArrayNew((cups_array_cb_t)compare_proxy_jobs, /*cbdata*/NULL, /*hash_cb*/NULL, /*hash_size*/0, (cups_acopy_cb_t)copy_proxy_job, (cups_afree_cb_t)free_proxy_job);

    // Scan existing jobs for parent-job-xxx attributes...
    for (job = (pappl_job_t *)cupsArrayGetFirst(printer->all_jobs); job; job = (pappl_job_t *)cupsArrayGetNext(printer->all_jobs))
    {
      if ((attr = ippFindAttribute(job->attrs, "parent-job-id", IPP_TAG_INTEGER)) != NULL)
      {
        pj.job           = job;
        pj.parent_job_id = ippGetInteger(attr, 0);
        if ((attr = ippFindAttribute(job->attrs, "parent-job-uuid", IPP_TAG_URI)) != NULL)
        {
          // Saw parent-job-id and parent-job-uuid, add it...
          cupsCopyString(pj.parent_job_uuid, ippGetString(attr, 0, NULL), sizeof(pj.parent_job_uuid));
          cupsArrayAdd(printer->proxy_jobs, &pj);
        }
      }
    }
  }

  return (printer->proxy_jobs != NULL);
}

