//
// Job accessor functions for the Printer Application Framework
//
// Copyright © 2020 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

//
// Include necessary headers...
//

#include "job-private.h"


//
// 'papplJob()' - .
//

void		*papplJobGetData(pappl_job_t *job)
{
}



//
// 'papplJob()' - .
//

const char	*papplJobGetFilename(pappl_job_t *job)
{
}



//
// 'papplJob()' - .
//

const char	*papplJobGetFormat(pappl_job_t *job)
{
}



//
// 'papplJob()' - .
//

int		papplJobGetId(pappl_job_t *job)
{
}



//
// 'papplJob()' - .
//

int		papplJobGetImpressions(pappl_job_t *job)
{
}



//
// 'papplJob()' - .
//

int		papplJobGetImpressionsCompleted(pappl_job_t *job)
{
}



//
// 'papplJob()' - .
//

const char	*papplJobGetName(pappl_job_t *job)
{
}



//
// 'papplJob()' - .
//

ipp_jstate_t	papplJobGetState(pappl_job_t *job)
{
}



//
// 'papplJob()' - .
//

const char	*papplJobGetStateMessage(pappl_job_t *job)
{
}



//
// 'papplJob()' - .
//

pappl_jreason_t	papplJobGetStateReasons(pappl_job_t *job)
{
}



//
// 'papplJob()' - .
//

time_t		papplJobGetTimeCompleted(pappl_job_t *job)
{
}



//
// 'papplJob()' - .
//

time_t		papplJobGetTimeCreated(pappl_job_t *job)
{
}



//
// 'papplJob()' - .
//

time_t		papplJobGetTimeProcessed(pappl_job_t *job)
{
}


//
// 'papplJob()' - .
//

const char	*papplJobGetUsername(pappl_job_t *job)
{
}


//
// 'papplJob()' - .
//

const char	*_papplJobReasonString(pappl_jreason_t reason)
{
}


//
// 'papplJob()' - .
//

pappl_jreason_t	_papplJobReasonValue(const char *reason)
{
}


//
// 'papplJob()' - .
//

void		papplJobSetData(pappl_job_t *job, void *data)
{
}


//
// 'papplJob()' - .
//

void		papplJobSetImpressions(pappl_job_t *job, int impressions)
{
}



//
// 'papplJob()' - .
//

void		papplJobSetImpressionsCompleted(pappl_job_t *job, int impressions)
{
}


//
// 'papplJob()' - .
//

void		papplJobSetMessage(pappl_job_t *job, const char *message, ...)
{
}


//
// 'papplJob()' - .
//

void		papplJobSetReasons(pappl_job_t *job, pappl_jreason_t add, pappl_jreason_t remove)
{
}


//
// 'papplJob()' - .
//

void		_papplJobSetState(pappl_job_t *job, ipp_jstate_t state)
{
}
