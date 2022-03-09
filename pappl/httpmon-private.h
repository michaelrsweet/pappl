//
// Private HTTP monitor definitions for the Printer Application Framework
//
// Copyright © 2021-2022 by Michael R Sweet.
// Copyright © 2012 by Apple Inc.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#ifndef PAPPL_HTTPMON_PRIVATE_H
#  define PAPPL_HTTPMON_PRIVATE_H
#  include "base-private.h"
#  include <cups/http.h>


//
// Types and structures...
//

typedef enum _pappl_http_chunk_e	// HTTP chunk phases
{
  _PAPPL_HTTP_CHUNK_HEADER,		// Reading chunk header (lengths)
  _PAPPL_HTTP_CHUNK_DATA,		// Reading chunk data
  _PAPPL_HTTP_CHUNK_TRAILER		// Reading chunk trailer (hashes, etc.; not generally used)
} _pappl_http_chunk_t;


typedef enum _pappl_http_phase_e	// HTTP state phases (sub-states)
{
  _PAPPL_HTTP_PHASE_CLIENT_HEADERS,	// Headers going to server
  _PAPPL_HTTP_PHASE_CLIENT_DATA,	// Data going to server
  _PAPPL_HTTP_PHASE_SERVER_HEADERS,	// Headers coming back from server
  _PAPPL_HTTP_PHASE_SERVER_DATA		// Data coming back from server
} _pappl_http_phase_t;


typedef struct _pappl_http_buffer_s	// HTTP data buffer
{
  size_t	used;			// Bytes used in buffer
  char		data[HTTP_MAX_BUFFER];	// Data in buffer
} _pappl_http_buffer_t;


typedef struct _pappl_http_monitor_s	// HTTP state monitoring data
{
  http_state_t		state;		// Current HTTP state
  _pappl_http_phase_t	phase;		// Current HTTP state phase
  http_status_t		status;		// Status of most recent request
  const char		*error;		// Error message, if any
  http_encoding_t	data_encoding;	// Chunked or not
  off_t			data_length,	// Original length of data/chunk
			data_remaining;	// Number of bytes left
  _pappl_http_chunk_t	data_chunk;	// Phase for chunked data
  _pappl_http_buffer_t	host;		// Data from client/host
  _pappl_http_buffer_t	device;		// Data from server/device
} _pappl_http_monitor_t;


//
// Functions...
//

extern const char	*_papplHTTPMonitorGetError(_pappl_http_monitor_t *hm) _PAPPL_PRIVATE;
extern http_state_t	_papplHTTPMonitorGetState(_pappl_http_monitor_t *hm) _PAPPL_PRIVATE;
extern void		_papplHTTPMonitorInit(_pappl_http_monitor_t *hm) _PAPPL_PRIVATE;
extern http_status_t	_papplHTTPMonitorProcessDeviceData(_pappl_http_monitor_t *hm, const char *data, size_t datasize) _PAPPL_PRIVATE;
extern http_status_t	_papplHTTPMonitorProcessHostData(_pappl_http_monitor_t *hm, const char **data, size_t *datasize) _PAPPL_PRIVATE;


#endif // !PAPPL_HTTPMON_PRIVATE_H
