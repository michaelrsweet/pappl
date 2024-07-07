//
// Scanner accessor functions for the Scanner Application Framework
//
// Copyright © 2020-2024 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#include "scanner-private.h"
#include "scanner.h"
#include "job-private.h"
#include "system-private.h"

//
// 'papplScannerCloseDevice()' - Close the device associated with the scanner .
//
// This function closes the device for a scanner.  The device must have been
// previously opened using the @link papplScannerOpenDevice@ function.
//

void
papplScannerCloseDevice(
  pappl_scanner_t *scanner)		// I - Scanner
{
  if (!scanner)
  return;

  _papplRWLockWrite(scanner);
  if (scanner->device && scanner->device_in_use)
  {
  scanner->device_in_use = false;

  if (scanner->state != ESCL_SSTATE_PROCESSING)
  {
  papplDeviceClose(scanner->device);
  scanner->device = NULL;
  }
  }

  _papplRWUnlock(scanner);
}

//
// 'papplScannerDisable()' - Stop accepting jobs on a scanner.
//
// This function stops accepting jobs on a scanner.
//

void
papplScannerDisable(
  pappl_scanner_t *scanner)		// I - Scanner
{
  if (scanner)
  {
  scanner->is_accepting = false;
  papplSystemAddScannerEvent(scanner->system, scanner, NULL, PAPPL_EVENT_SCANNER_STATE_CHANGED, NULL);
  }
}

//
// 'papplScannerEnable()' - Start accepting jobs on a scanner.
//
// This function starts accepting jobs on a scanner.
//

void
papplScannerEnable(
  pappl_scanner_t *scanner)		// I - Scanner
{
  if (scanner)
  {
  scanner->is_accepting = true;
  papplSystemAddScannerEvent(scanner->system, scanner, NULL, PAPPL_EVENT_SCANNER_STATE_CHANGED, NULL);
  }
}


//
// 'papplScannerGetContact()' - Get the "scanner-contact" value.
//
// This function copies the current scanner contact information to the buffer
// pointed to by the "contact" argument.
//

pappl_contact_t *			// O - Contact
papplScannerGetContact(
  pappl_scanner_t *scanner,		// I - Scanner
  pappl_contact_t *contact)		// O - Contact
{
  if (!scanner || !contact)
  {
  if (contact)
  memset(contact, 0, sizeof(pappl_contact_t));

  return (contact);
  }

  _papplRWLockRead(scanner);

  *contact = scanner->contact;

  _papplRWUnlock(scanner);

  return (contact);
}


//
// 'papplScannerGetDeviceID()' - Get the device ID of the scanner.
//
// This function returns the device ID of the scanner.
//

const char *				// O - Device ID string
papplScannerGetDeviceID(
  pappl_scanner_t *scanner)		// I - Scanner
{
  return (scanner ? scanner->device_id : NULL);
}

//
// 'papplScannerGetDeviceURI()' - Get the URI of the device associated with the
//                                scanner.
//
// This function returns the device URI for the scanner.
//

const char *				// O - Device URI string
papplScannerGetDeviceURI(
  pappl_scanner_t *scanner)		// I - Scanner
{
  return (scanner ? scanner->device_uri : "file:///dev/null");
}


//
// 'papplScannerGetDNSSDName()' - Get the current DNS-SD service name.
//
// This function copies the current DNS-SD service name to the buffer pointed
// to by the "buffer" argument.
//

char *					// O - DNS-SD service name or `NULL` for none
papplScannerGetDNSSDName(
  pappl_scanner_t *scanner,		// I - Scanner
  char            *buffer,		// I - String buffer
  size_t          bufsize)		// I - Size of string buffer
{
  if (!scanner || !scanner->dns_sd_name || !buffer || bufsize == 0)
  {
  if (buffer)
  *buffer = '\0';

  return (NULL);
  }

  _papplRWLockRead(scanner);
  papplCopyString(buffer, scanner->dns_sd_name, bufsize);
  _papplRWUnlock(scanner);

  return (buffer);
}

//
// 'papplScannerGetGeoLocation()' - Get the current geo-location as a "geo:"
//                                  URI.
//
// This function copies the currently configured geographic location as a "geo:"
// URI to the buffer pointed to by the "buffer" argument.
//

char *					// O - "geo:" URI or `NULL` for unknown
papplScannerGetGeoLocation(
  pappl_scanner_t *scanner,		// I - Scanner
  char            *buffer,		// I - String buffer
  size_t          bufsize)		// I - Size of string buffer
{
  if (!scanner || !scanner->geo_location || !buffer || bufsize == 0)
  {
  if (buffer)
  *buffer = '\0';

  return (NULL);
  }

  _papplRWLockRead(scanner);
  papplCopyString(buffer, scanner->geo_location, bufsize);
  _papplRWUnlock(scanner);

  return (buffer);
}

//
// 'papplScannerGetID()' - Get the scanner ID.
//
// This function returns the scanner's unique positive integer identifier.
//

int					// O - "scanner-id" value or `0` for none
papplScannerGetID(
  pappl_scanner_t *scanner)		// I - Scanner
{
  return (scanner ? scanner->scanner_id : 0);
}

//
// 'papplScannerGetLocation()' - Get the location string.
//
// This function copies the scanner's human-readable location to the buffer
// pointed to by the "buffer" argument.
//

char *					// O - Location or `NULL` for none
papplScannerGetLocation(
  pappl_scanner_t *scanner,		// I - Scanner
  char            *buffer,		// I - String buffer
  size_t          bufsize)		// I - Size of string buffer
{
  if (!scanner || !scanner->location || !buffer || bufsize == 0)
  {
  if (buffer)
  *buffer = '\0';

  return (NULL);
  }

  _papplRWLockRead(scanner);
  papplCopyString(buffer, scanner->location, bufsize);
  _papplRWUnlock(scanner);

  return (buffer);
}

//
// 'papplScannerGetName()' - Get the scanner name.
//
// This function returns the scanner's human-readable name.
//


const char *				// O - Scanner name
papplScannerGetName(
  pappl_scanner_t *scanner)		// I - scanner
{
  return (scanner ? scanner->name : NULL);
}

//
// 'papplScannerGetNextJobID()' - Get the next job ID.
//
// This function returns the positive integer identifier that will be used for
// the next job that is created.
//

int					// O - Next job ID or `0` for none
papplScannerGetNextJobID(
  pappl_scanner_t *scanner)		// I - Scanner
{
  return (scanner ? scanner->next_job_id : 0);
}

//
// 'papplScannerGetOrganization()' - Get the organization name.
//
// This function copies the scanner's organization name to the buffer pointed
// to by the "buffer" argument.
//

char *					// O - Organization name or `NULL` for none
papplScannerGetOrganization(
  pappl_scanner_t *scanner,		// I - Scanner
  char            *buffer,		// I - String buffer
  size_t          bufsize)		// I - Size of string buffer
{
  if (!scanner || !scanner->organization || !buffer || bufsize == 0)
  {
  if (buffer)
  *buffer = '\0';

  return (NULL);
  }

  _papplRWLockRead(scanner);
  papplCopyString(buffer, scanner->organization, bufsize);
  _papplRWUnlock(scanner);

  return (buffer);
}

//
// 'papplScannerGetOrganizationalUnit()' - Get the organizational unit name.
//
// This function copies the scanner's organizational unit name to the buffer
// pointed to by the "buffer" argument.
//

char *					// O - Organizational unit name or `NULL` for none
papplScannerGetOrganizationalUnit(
  pappl_scanner_t *scanner,		// I - Scanner
  char            *buffer,		// I - String buffer
  size_t          bufsize)		// I - Size of string buffer
{
  if (!scanner || !scanner->org_unit || !buffer || bufsize == 0)
  {
  if (buffer)
  *buffer = '\0';

  return (NULL);
  }

  _papplRWLockRead(scanner);
  papplCopyString(buffer, scanner->org_unit, bufsize);
  _papplRWUnlock(scanner);

  return (buffer);
}

//
// 'papplScannerGetPath()' - Get the URL path for a scanner web page.
//
// This function generates and returns the URL path for the scanner's web page.
// The "subpath" argument specifies an optional sub-path for a specific printer
// web page.
//

char *					// O - URI path or `NULL` on error
papplScannerGetPath(
  pappl_scanner_t *scanner,		// I - Scanner
  const char      *subpath,		// I - Sub-path or `NULL` for none
  char            *buffer,		// I - String buffer
  size_t          bufsize)		// I - Size of string buffer
{
  if (!scanner || !buffer || bufsize < 32)
  {
  if (buffer)
  *buffer = '\0';

  return (NULL);
  }

  if (subpath)
  snprintf(buffer, bufsize, "%s/%s", scanner->uriname, subpath);
  else
  papplCopyString(buffer, scanner->uriname, bufsize);

  return (buffer);
}

//
// 'papplScannerGetReasons()' - Get the current "scanner-state-reasons" bit values.
//
// This function returns the current scanner state reasons bitfield, which can
// be updated by the scanner driver and/or by the @link papplScannerSetReasons@
// function.
//

pappl_sreason_t				// O - "scanner-state-reasons" bit values
papplScannerGetReasons(
  pappl_scanner_t *scanner)	   // I - Scanner
{
  pappl_sreason_t ret = PAPPL_SREASON_NONE;
      // Return value

  if (scanner)
  {
  _papplRWLockRead(scanner);

  if (!scanner->device_in_use && !scanner->processing_job && (time(NULL) - scanner->status_time) > 1 && scanner->driver_data.status_cb)
  {
  // Update scanner status...
  _papplRWUnlock(scanner);
  (scanner->driver_data.status_cb)(scanner);

  _papplRWLockRead(scanner);
  scanner->status_time = time(NULL);
  }

  ret = scanner->state_reasons;

  _papplRWUnlock(scanner);
  }

  return (ret);
}

//
// 'papplScannerGetState()' - Get the current "scanner-state" value.
//
// This function returns the current scanner state as an enumeration:
//

escl_sstate_t 				// O - "scanner-state" value
papplScannerGetState(
  pappl_scanner_t *scanner)		// I - Scanner
{
  return (scanner ? scanner->state : ESCL_SSTATE_STOPPED);
}


//
// 'papplScannerGetSystem()' - Get the system associated with the scanner.
//
// This function returns a pointer to the system object that contains the
// scanner.
//

pappl_system_t *			// O - System
papplScannerGetSystem(
  pappl_scanner_t *scanner)		// I - Scanner
{
  return (scanner ? scanner->system : NULL);
}

//
// 'papplScannerIsAcceptingJobs()' - Return whether the scanner is accepting jobs.
//
// This function returns a boolean value indicating whether a scanner is
// accepting jobs.
//

bool					// O - `true` if the scanner is accepting jobs, `false` otherwise
papplScannerIsAcceptingJobs(
  pappl_scanner_t *scanner)		// I - Scanner
{
  bool	is_accepting;			// Return value


  // Range check input...
  if (!scanner)
  return (false);

  // Lock and grab value...
  _papplRWLockRead(scanner);
  is_accepting = scanner->is_accepting;
  _papplRWUnlock(scanner);

  return (is_accepting);
}

//
// 'papplScannerIsDeleted()' - Return whether a scanner is in the process of being deleted.
//
// This function returns a boolean value indicating whether a scanner is being
// deleted.
//

bool					// O - `true` is scanner is being deleted, `false` otherwise
papplScannerIsDeleted(
  pappl_scanner_t *scanner)		// I - Scanner
{
  bool	is_deleted;			// Return value


  // Range check input...
  if (!scanner)
  return (false);

  // Lock and grab value...
  _papplRWLockRead(scanner);
  is_deleted = scanner->is_deleted;
  _papplRWUnlock(scanner);

  return (is_deleted);
}


//
// 'papplScannerOpenDevice()' - Open the device associated with a scanner.
//
// This function opens the scanners's device.  `NULL` is returned if the device
// is already in use, for example while a job is being scanned.
//
// The returned device must be closed using the @link papplScannerCloseDevice@
// function.
//

pappl_device_t *			// O - Device or `NULL` if not possible
papplScannerOpenDevice(
  pappl_scanner_t *scanner)		// I - Scanner
{
  pappl_device_t	*device = NULL;	// Open device


  if (!scanner)
  return (NULL);

  _papplRWLockWrite(scanner);

  if (!scanner->device_in_use && !scanner->processing_job && scanner->device_uri)
  {
  scanner->device        = device = papplDeviceOpen(scanner->device_uri, "scanner", papplLogDevice, scanner->system);
  scanner->device_in_use = device != NULL;
  }

  _papplRWUnlock(scanner);

  return (device);
}

//
// 'papplScannerPause()' - Pause (stop) a scanner.
//
// This function pauses a scanner.  If the scanner is currently processing
// (scanning) a job, it will be completed before the scanner is stopped.
//

void
papplScannerPause(
  pappl_scanner_t *scanner)		// I - Scanner
{
  if (!scanner)
  return;

  _papplRWLockWrite(scanner);

  if (scanner->processing_job)
  scanner->is_stopped = true;
  else
  scanner->state = ESCL_SSTATE_STOPPED;

  papplSystemAddScannerEvent(scanner->system, scanner, NULL, PAPPL_EVENT_SCANNER_STATE_CHANGED | PAPPL_EVENT_SCANNER_STOPPED, NULL);

  _papplRWUnlock(scanner);
}

//
// 'papplScannerResume()' - Resume (start) a scanner.
//
// This function resumes a scanner.
//

void
papplScannerResume(
  pappl_scanner_t *scanner)		// I - Scanner
{
  if (!scanner)
    return;

  _papplRWLockWrite(scanner);

  scanner->is_stopped = false;
  scanner->state      = ESCL_SSTATE_IDLE;

  papplSystemAddScannerEvent(scanner->system, scanner, NULL, PAPPL_EVENT_SCANNER_STATE_CHANGED, "Resumed scanner.");

  _papplRWUnlock(scanner);
}


//
// 'papplScannerSetContact()' - Set the "scanner-contact" value.
//
// This function sets the scanner's contact information.
//

void
papplScannerSetContact(
  pappl_scanner_t *scanner,		// I - Scanner
  pappl_contact_t *contact)		// I - Contact
{
  if (!scanner || !contact)
  return;

  _papplRWLockWrite(scanner);

  scanner->contact     = *contact;
  scanner->config_time = time(NULL);

  _papplRWUnlock(scanner);

  _papplSystemConfigChanged(scanner->system);
}

//
// 'papplScannerSetDNSSDName()' - Set the DNS-SD service name.
//
// This function sets the scanner's DNS-SD service name.  If `NULL`, the scanner
// will stop advertising the scanner.
//

void
papplScannerSetDNSSDName(
  pappl_scanner_t *scanner,		// I - Scanner
  const char      *value)		// I - DNS-SD service name or `NULL` for none
{
  if (!scanner)
  return;

  _papplRWLockWrite(scanner);

  free(scanner->dns_sd_name);
  scanner->dns_sd_name      = value ? strdup(value) : NULL;
  scanner->dns_sd_collision = false;
  scanner->dns_sd_serial    = 0;
  scanner->config_time      = time(NULL);

  // TODO
  if (!value)
  _papplScannerUnregisterDNSSDNoLock(scanner);
  else
  _papplScannerRegisterDNSSDNoLock(scanner);

  _papplRWUnlock(scanner);

  _papplSystemConfigChanged(scanner->system);
}

//
// 'papplScannerSetGeoLocation()' - Set the geo-location value as a "geo:" URI.
//
// This function sets the scanner's geographic location as a "geo:" URI.  If
// `NULL`, the location is cleared to the 'unknown' value.
//

void
papplScannerSetGeoLocation(
  pappl_scanner_t *scanner,		// I - Scanner
  const char      *value)		// I - "geo:" URI or `NULL` for unknown
{
  float	lat, lon;			// Latitude and longitude from geo: URI


  if (!scanner)
  return;

  // Validate geo-location - must be NULL or a "geo:" URI...
  if (value && *value && sscanf(value, "geo:%f,%f", &lat, &lon) != 2)
  return;

  _papplRWLockWrite(scanner);

  free(scanner->geo_location);
  scanner->geo_location = value && *value ? strdup(value) : NULL;
  scanner->config_time  = time(NULL);

  // TODO
  _papplScannerRegisterDNSSDNoLock(scanner);

  _papplRWUnlock(scanner);

  _papplSystemConfigChanged(scanner->system);
}

//
// 'papplScannerSetLocation()' - Set the location string.
//
// This function sets the scanner's human-readable location string.  If `NULL`,
// the location is cleared.
//

void
papplScannerSetLocation(
  pappl_scanner_t *scanner,		// I - Scanner
  const char      *value)		// I - Location ("Bob's Office", etc.) or `NULL` for none
{
  if (!scanner)
  return;

  _papplRWLockWrite(scanner);

  free(scanner->location);
  scanner->location    = value ? strdup(value) : NULL;
  scanner->config_time = time(NULL);

  //TODO
  _papplScannerRegisterDNSSDNoLock(scanner);

  _papplRWUnlock(scanner);

  _papplSystemConfigChanged(scanner->system);
}

//
// 'papplScannerSetNextJobID()' - Set the next "job-id" value.
//
// This function sets the next unique positive integer identifier that will be
// used for a job.
//
// > Note: This function is normally only called once to restore the previous
// > state of the scanner.
//
void
papplScannerSetNextJobID(
  pappl_scanner_t *scanner,		// I - Scanner
  int             next_job_id)	// I - Next "job-id" value
{
  if (!scanner || next_job_id < 1)
  return;

  _papplRWLockWrite(scanner);

  scanner->next_job_id = next_job_id;
  scanner->config_time = time(NULL);

  _papplRWUnlock(scanner);

  _papplSystemConfigChanged(scanner->system);
}

//
// 'papplScannerSetOrganization()' - Set the organization name.
//
// This function sets the scanner's organization name.  If `NULL` the value is
// cleared.
//

void
papplScannerSetOrganization(
  pappl_scanner_t *scanner,		// I - Scanner
  const char      *value)		// I - Organization name or `NULL` for none
{
  if (!scanner)
  return;

  _papplRWLockWrite(scanner);

  free(scanner->organization);
  scanner->organization = value ? strdup(value) : NULL;
  scanner->config_time  = time(NULL);

  _papplRWUnlock(scanner);

  _papplSystemConfigChanged(scanner->system);
}

//
// 'papplScannerSetOrganizationalUnit()' - Set the organizational unit name.
//
// This function sets the scanner's organizational unit name.  If `NULL` the
// value is cleared.
//

void
papplScannerSetOrganizationalUnit(
  pappl_scanner_t *scanner,		// I - Scanner
  const char      *value)		// I - Organizational unit name or `NULL` for none
{
  if (!scanner)
  return;

  _papplRWLockWrite(scanner);

  free(scanner->org_unit);
  scanner->org_unit    = value ? strdup(value) : NULL;
  scanner->config_time = time(NULL);

  _papplRWUnlock(scanner);

  _papplSystemConfigChanged(scanner->system);
}


//
// 'papplScannerSetReasons()' - Add or remove values from
//                              "scanner-state-reasons".
//
// This function updates the scanner state reasons bitfield by clearing any bit
// values in the "remove" argument and setting any bit values in the "add"
// argument.
//

void
papplScannerSetReasons(
  pappl_scanner_t *scanner,		// I - Scanner
  pappl_sreason_t add,		// I - "scanner-state-reasons" bit values to add or `PAPPL_SREASON_NONE` for none
  pappl_sreason_t remove)		// I - "scanner-state-reasons" bit values to remove or `PAPPL_SREASON_NONE` for none
{
  if (!scanner)
  return;

  _papplRWLockWrite(scanner);

  scanner->state_reasons &= ~remove;
  scanner->state_reasons |= add;
  scanner->state_time    = scanner->status_time = time(NULL);

  _papplRWUnlock(scanner);
}
