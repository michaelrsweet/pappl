//
// Private HTTP monitor implementation for the Printer Application Framework
//
// Copyright © 2021 by Michael R Sweet.
// Copyright © 2012 by Apple Inc.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#include "httpmon-private.h"


//
// Local functions...
//

static size_t	http_buffer_add(_pappl_http_buffer_t *hb, const char **data, size_t *datasize);
static size_t	http_buffer_consume(_pappl_http_buffer_t *hb, const char **data, size_t *datasize, size_t bytes);
static char	*http_buffer_line(_pappl_http_monitor_t *hm, _pappl_http_buffer_t *hb, const char **data, size_t *datasize, char *line, size_t linesize);


//
// '_papplHTTPMonitorGetError()' - Get the current HTTP monitor error, if any.
//
// This function returns the current error message for the monitor.  If there
// is no error, `NULL` is returned.
//

const char *				// O - Error message or `NULL` for none
_papplHTTPMonitorGetError(
    _pappl_http_monitor_t *hm)		// I - HTTP monitor
{
  return (hm->error);
}


//
// '_papplHTTPMonitorGetState()' - Get the current state of the HTTP monitor.
//
// This function returns the current HTTP state of the HTTP monitor.  The
// value `HTTP_STATE_WAITING` specifies that there is no active HTTP request or
// response traffic.
//

http_state_t				// O - HTTP state
_papplHTTPMonitorGetState(
    _pappl_http_monitor_t *hm)		// I - HTTP monitor
{
  return (hm->state);
}


//
// '_papplHTTPMonitorInit()' - Initialize a HTTP monitoring structure.
//
// This function initializes a `_pappl_http_monitor_t` structure to the
// `HTTP_WAITING` state.  Each connection needs an instance of this structure.
//
// When data comes in from the client/USB host, call the
// @link _papplHTTPMonitorProcessHostData@ function.  Similarly, when data
// comes in from the printer/USB device, call the
// @link _papplHTTPMonitorProcessDeviceData@ function.
//
// To get the current HTTP state, call the @link _papplHTTPMonitorGetState@
// function.
//

void
_papplHTTPMonitorInit(
    _pappl_http_monitor_t *hm)		// I - HTTP monitor
{
  memset(hm, 0, sizeof(_pappl_http_monitor_t));

  hm->status = HTTP_STATUS_CONTINUE;
}


//
// '_papplHTTPMonitorProcessHostData()' - Process data from the USB host.
//
// This function processes data provided by the HTTP client/USB host,
// returning the current HTTP status for the "connection".
//
// `HTTP_STATUS_ERROR` is returned for errors in the HTTP client request.
// `HTTP_STATUS_CONTINUE` is returned when there is no error.
//

http_status_t				// O  - Current HTTP status
_papplHTTPMonitorProcessHostData(
    _pappl_http_monitor_t *hm,		// I  - HTTP monitor
    const char            **data,	// IO - Data from USB host
    size_t                *datasize)	// IO - Number of bytes of data
{
  char		line[2048],		// Header line
		*ptr;			// Pointer into line
  size_t	bytes;			// Bytes remaining


  while (hm->status != HTTP_STATUS_ERROR && (hm->host.used > 0 || *datasize > 0))
  {
    switch (hm->state)
    {
      case HTTP_STATE_WAITING :
	  // Get request: "METHOD PATH HTTP/major.minor"
	  if (!http_buffer_line(hm, &hm->host, data, datasize, line, sizeof(line)))
	    return (hm->status);

	  // Split the leading request method from the line...
	  if ((ptr = strchr(line, ' ')) == NULL)
	  {
	    // No whitespace, so the request line is probably malformed
	    hm->status = HTTP_STATUS_ERROR;
	    hm->error  = "Bad request line.";
	    break;
	  }

	  *ptr = '\0';

	  // Update the state based on the method...
	  hm->status        = HTTP_STATUS_CONTINUE;
	  hm->data_encoding = HTTP_ENCODING_LENGTH;
	  hm->data_length   = hm->data_remaining = 0;

	  if (!strcasecmp(line, "OPTIONS"))
	  {
	    hm->state = HTTP_STATE_OPTIONS;
	  }
	  else if (!strcasecmp(line, "GET"))
	  {
	    hm->state = HTTP_STATE_GET;
	  }
	  else if (!strcasecmp(line, "HEAD"))
	  {
	    hm->state = HTTP_STATE_HEAD;
	  }
	  else if (!strcasecmp(line, "POST"))
	  {
	    hm->state = HTTP_STATE_POST;
	  }
	  else if (!strcasecmp(line, "PUT"))
	  {
	    hm->state = HTTP_STATE_PUT;
	  }
	  else if (!strcasecmp(line, "DELETE"))
	  {
	    hm->state = HTTP_STATE_DELETE;
	  }
	  else
	  {
	    hm->status = HTTP_STATUS_ERROR;
	    hm->error  = "Unknown request method seen.";
	    break;
	  }
	  break;

      case HTTP_STATE_OPTIONS :
      case HTTP_STATE_GET :
      case HTTP_STATE_HEAD :
      case HTTP_STATE_POST :
      case HTTP_STATE_PUT :
      case HTTP_STATE_DELETE :
	  switch (hm->phase)
	  {
	      case _PAPPL_HTTP_PHASE_CLIENT_HEADERS : /* Waiting for blank line */
		  if (!http_buffer_line(hm, &hm->host, data, datasize, line, sizeof(line)))
		    return (hm->status);

		  if (!line[0])
		  {
		    // Got a blank line, advance state machine...
		    if (hm->state == HTTP_STATE_POST || hm->state == HTTP_STATE_PUT)
		    {
		      hm->phase = _PAPPL_HTTP_PHASE_CLIENT_DATA;
		    }
		    else
		    {
		      hm->phase = _PAPPL_HTTP_PHASE_SERVER_HEADERS;
		      return (HTTP_STATUS_CONTINUE);
		    }
		    break;
		  }

		  // Try to get the "header: value" bits on this line...
		  if ((ptr = strchr(line, ':')) == NULL)
		  {
		    hm->status = HTTP_STATUS_ERROR;
		    hm->error  = "No separator seen in request header line.";
		    break;
		  }

		  *ptr++ = '\0';	// Nul-terminate header name
		  while (isspace(*ptr & 255))
		    ptr ++;		// Skip whitespace

		  if (!strcasecmp(line, "Transfer-Encoding") && !strcasecmp(ptr, "chunked"))
		  {
		    // Using chunked encoding...
		    hm->data_encoding = HTTP_ENCODING_CHUNKED;
		    hm->data_length   = hm->data_remaining = 0;
		    hm->data_chunk    = _PAPPL_HTTP_CHUNK_HEADER;
		  }
		  else if (!strcasecmp(line, "Content-Length"))
		  {
		    // Using fixed Content-Length...
		    hm->data_encoding = HTTP_ENCODING_LENGTH;
		    hm->data_length   = hm->data_remaining = strtol(ptr, NULL, 10);

		    if (hm->data_length < 0)
		    {
		      hm->status = HTTP_STATUS_ERROR;
		      hm->error  = "Bad (negative) Content-Length value.";
		      break;
		    }
		  }
		  break;

	      case _PAPPL_HTTP_PHASE_CLIENT_DATA : // Sending data
		  if (hm->data_encoding == HTTP_ENCODING_CHUNKED)
		  {
		    // Skip chunked data...
		    switch (hm->data_chunk)
		    {
		      case _PAPPL_HTTP_CHUNK_HEADER :
			  if (!http_buffer_line(hm, &hm->host, data, datasize, line, sizeof(line)))
			    return (hm->status);

			  // Get chunk length (hex)
			  if (!line[0])
			  {
			    hm->status = HTTP_STATUS_ERROR;
			    hm->error  = "Bad (empty) chunk length.";
			    break;
			  }

			  hm->data_length = hm->data_remaining = strtol(line, NULL, 16);

			  if (hm->data_length == 0)
			  {
			    // 0-length chunk signals end-of-message
			    hm->data_chunk = _PAPPL_HTTP_CHUNK_TRAILER;
			  }
			  else if (hm->data_length < 0)
			  {
			    // Negative lengths not allowed
			    hm->status = HTTP_STATUS_ERROR;
			    hm->error  = "Bad (negative) chunk length.";
			    break;
			  }
			  else
			  {
			    // Advance to the data phase of chunk processing
			    hm->data_chunk = _PAPPL_HTTP_CHUNK_DATA;
			  }
			  break;

		      case _PAPPL_HTTP_CHUNK_DATA :
			  if (hm->data_remaining > 0)
			  {
			    bytes = http_buffer_consume(&hm->host, data, datasize, (size_t)hm->data_remaining);
			    hm->data_remaining -= (off_t)bytes;
			  }

			  if (hm->data_remaining == 0)
			  {
			    // End of data, expect chunk trailer...
			    hm->data_chunk = _PAPPL_HTTP_CHUNK_TRAILER;
			    break;
			  }
			  break;

		      case _PAPPL_HTTP_CHUNK_TRAILER :
			  // Look for blank line at end of chunk
			  if (!http_buffer_line(hm, &hm->host, data, datasize, line, sizeof(line)))
			    return (hm->status);

			  if (line[0])
			  {
			    // Expected blank line...
			    hm->status = HTTP_STATUS_ERROR;
			    hm->error  = "Expected blank line at end of chunk.";
			    break;
			  }

			  // Change states...
			  if (hm->data_length == 0)
			  {
			    // Got a 0-length chunk, transition to the server headers phase
			    hm->phase         = _PAPPL_HTTP_PHASE_SERVER_HEADERS;
			    hm->status        = HTTP_STATUS_CONTINUE;
			    hm->data_encoding = HTTP_ENCODING_LENGTH;
			    hm->data_length   = hm->data_remaining = 0;

			    return (HTTP_STATUS_CONTINUE);
			  }
			  else
			  {
			    // Normal chunk, look for the next one...
			    hm->data_chunk = _PAPPL_HTTP_CHUNK_HEADER;
			  }
			  break;
		    }
		  }
		  else
		  {
		    // Skip fixed-length data...
		    if (hm->data_remaining > 0)
		    {
		      bytes = http_buffer_consume(&hm->host, data, datasize, (size_t)hm->data_remaining);
		      hm->data_remaining -= (off_t)bytes;
		    }

		    if (hm->data_remaining == 0)
		    {
		      // End of data, expect server headers in response...
		      hm->phase         = _PAPPL_HTTP_PHASE_SERVER_HEADERS;
		      hm->status        = HTTP_STATUS_CONTINUE;
		      hm->data_encoding = HTTP_ENCODING_LENGTH;
		      hm->data_length   = hm->data_remaining = 0;

		      return (HTTP_STATUS_CONTINUE);
		    }
		  }
		  break;

	      default :
		  /* Expecting something from the server... */
		  hm->status = HTTP_STATUS_ERROR;
		  hm->error  = "Client data sent while expecting response from server.";
		  break;
	    }
	    break;

      default :
	    /* Error out if we get here */
	    hm->status = HTTP_STATUS_ERROR;
	    hm->error  = "Unexpected HTTP state.";
	    break;
    }
  }

  return (hm->status);
}


//
// '_papplHTTPMonitorProcessDeviceData()' - Process data from the USB device.
//
// This function processes data provided by the HTTP server/IPP Printer/USB
// device, returning the current HTTP status for the "connection".
//
// `HTTP_STATUS_ERROR` is returned for errors in the HTTP server response.
// `HTTP_STATUS_CONTINUE` is returned when there is no error.
//

http_status_t				// O - Current HTTP status
_papplHTTPMonitorProcessDeviceData(
    _pappl_http_monitor_t *hm,		// I - HTTP monitor
    const char            *data,	// I - Data
    size_t                datasize)	// I - Number of bytes of data
{
  char		line[2048],		// Header line from server
		*ptr;			// Pointer into line
  size_t	bytes;			// Bytes consumed


  while (hm->status != HTTP_STATUS_ERROR && (hm->host.used > 0 || datasize > 0))
  {
    switch (hm->state)
    {
      case HTTP_STATE_OPTIONS :
      case HTTP_STATE_GET :
      case HTTP_STATE_HEAD :
      case HTTP_STATE_POST :
      case HTTP_STATE_PUT :
      case HTTP_STATE_DELETE :
	  switch (hm->phase)
	  {
	    case _PAPPL_HTTP_PHASE_SERVER_HEADERS : /* Waiting for blank line */
		if (!http_buffer_line(hm, &hm->host, &data, &datasize, line, sizeof(line)))
		  return (hm->status);

		if (!line[0])
		{
		  if (hm->status)
		  {
		    // Got a blank line, advance state machine...
		    if (hm->state != HTTP_STATE_HEAD && (hm->data_remaining > 0 || hm->data_encoding == HTTP_ENCODING_CHUNKED))
		    {
		      hm->phase = _PAPPL_HTTP_PHASE_SERVER_DATA;
		    }
		    else if (hm->status != HTTP_STATUS_CONTINUE)
		    {
		      hm->state = HTTP_STATE_WAITING;
		      hm->phase = _PAPPL_HTTP_PHASE_CLIENT_HEADERS;
		    }
		  }

		  break;
		}

		// See if the line has a status code value...
		if (hm->status == HTTP_STATUS_CONTINUE && !strncmp(line, "HTTP/", 5))
		{
		  // Got the beginning of a response...
		  int	intstatus;	// Status value as an integer
		  int	major, minor;	// HTTP version numbers

		  if (sscanf(line, "HTTP/%d.%d%d", &major, &minor, &intstatus) != 3)
		  {
		    hm->status = HTTP_STATUS_ERROR;
		    hm->error  = "Malformed HTTP header seen in response.";
		    break;
		  }

		  hm->status = (http_status_t)intstatus;
		  if (hm->status < 100)
		  {
		    hm->status = HTTP_STATUS_ERROR;
		    hm->error  = "Bad HTTP status seen in response.";
		  }
		  break;
		}

		// Try to get the "header: value" bits on this line...
		if ((ptr = strchr(line, ':')) == NULL)
		{
		  hm->status = HTTP_STATUS_ERROR;
		  hm->error  = "No separator seen in response header line.";
		  break;
		}

		*ptr++ = '\0';		// Nul-terminate header name
		while (isspace(*ptr & 255))
		  ptr ++;		// Skip whitespace

		if (!strcasecmp(line, "Transfer-Encoding") && !strcasecmp(ptr, "chunked"))
		{
		  // Using chunked encoding...
		  hm->data_encoding = HTTP_ENCODING_CHUNKED;
		  hm->data_length   = hm->data_remaining = 0;
		  hm->data_chunk    = _PAPPL_HTTP_CHUNK_HEADER;
		}
		else if (!strcasecmp(line, "Content-Length"))
		{
		  // Using fixed Content-Length...
		  hm->data_encoding = HTTP_ENCODING_LENGTH;
		  hm->data_length   = hm->data_remaining = strtol(ptr, NULL, 10);

		  if (hm->data_length < 0)
		  {
		    hm->status = HTTP_STATUS_ERROR;
		    hm->error  = "Bad (negative) Content-Length value.";
		    break;
		  }
		}
		break;

	  case _PAPPL_HTTP_PHASE_SERVER_DATA : // Receiving data
	      if (hm->data_encoding == HTTP_ENCODING_CHUNKED)
	      {
		// Skip chunked data...
		switch (hm->data_chunk)
		{
		  case _PAPPL_HTTP_CHUNK_HEADER :
		      if (!http_buffer_line(hm, &hm->host, &data, &datasize, line, sizeof(line)))
			return (hm->status);

		      // Get chunk length (hex)
		      if (!line[0])
		      {
			hm->status = HTTP_STATUS_ERROR;
			hm->error  = "Bad (empty) chunk length.";
			break;
		      }

		      hm->data_length = hm->data_remaining = strtol(line, NULL, 16);

		      if (hm->data_length == 0)
		      {
			// 0-length chunk signals end-of-message
			hm->data_chunk = _PAPPL_HTTP_CHUNK_TRAILER;
		      }
		      else if (hm->data_length < 0)
		      {
			// Negative lengths not allowed
			hm->status = HTTP_STATUS_ERROR;
			hm->error  = "Bad (negative) chunk length.";
			break;
		      }
		      else
		      {
			// Advance to the data phase of chunk processing
			hm->data_chunk = _PAPPL_HTTP_CHUNK_DATA;
		      }
		      break;

		  case _PAPPL_HTTP_CHUNK_DATA : // Consume chunk data...
		      if (hm->data_remaining > 0)
		      {
			bytes = http_buffer_consume(&hm->host, &data, &datasize, (size_t)hm->data_remaining);
			hm->data_remaining -= (off_t)bytes;
		      }

		      if (hm->data_remaining == 0)
		      {
			// End of data, expect chunk trailer...
			hm->data_chunk = _PAPPL_HTTP_CHUNK_TRAILER;
			break;
		      }
		      break;

		  case _PAPPL_HTTP_CHUNK_TRAILER : // Look for blank line at end of chunk
		      if (!http_buffer_line(hm, &hm->host, &data, &datasize, line, sizeof(line)))
			return (hm->status);

		      if (line[0])
		      {
			// Expected blank line...
			hm->status = HTTP_STATUS_ERROR;
			hm->error  = "Expected blank line at end of chunk.";
			break;
		      }

		      // Change states...
		      if (hm->data_length == 0)
		      {
			// Got a 0-length chunk, transition to the waiting state
			hm->phase = _PAPPL_HTTP_PHASE_CLIENT_HEADERS;
			hm->state = HTTP_STATE_WAITING;
			break;
		      }
		      else
		      {
			// Normal chunk, look for the next one...
			hm->data_chunk = _PAPPL_HTTP_CHUNK_HEADER;
		      }
		      break;
		}
	      }
	      else
	      {
		// Skip fixed-length data...
		if (hm->data_remaining > 0)
		{
		  bytes = http_buffer_consume(&hm->host, &data, &datasize, (size_t)hm->data_remaining);
		  hm->data_remaining -= (off_t)bytes;
		}

		if (hm->data_remaining == 0)
		{
		  // End of data, expect new request from client...
		  hm->phase = _PAPPL_HTTP_PHASE_CLIENT_HEADERS;
		  hm->state = HTTP_STATE_WAITING;
		  break;
		}
	      }
	      break;

	  case _PAPPL_HTTP_PHASE_CLIENT_HEADERS : // Expecting headers from the client...
	      hm->status = HTTP_STATUS_ERROR;
	      hm->error  = "Server cannot respond while client is sending request headers.";
	      break;

	  case _PAPPL_HTTP_PHASE_CLIENT_DATA : // Server may send failure response before client completes POST/PUT
	      if (!http_buffer_line(hm, &hm->host, &data, &datasize, line, sizeof(line)))
		return (hm->status);

	      if (!strncmp(line, "HTTP/", 5))
	      {
		// Got the beginning of a response...
		int	intstatus;	// Status value as an integer
		int	major, minor;	// HTTP version numbers

		if (sscanf(line, "HTTP/%d.%d%d", &major, &minor, &intstatus) != 3)
		{
		  hm->status = HTTP_STATUS_ERROR;
		  hm->error  = "Malformed HTTP header seen in early response.";
		  break;
		}

		hm->status = (http_status_t)intstatus;
		if (intstatus != 100 && (intstatus < 400 || intstatus >= 500))
		{
		  hm->status = HTTP_STATUS_ERROR;
		  hm->error  = "Bad server status code seen during client data phase.";
		}
		else if (hm->status != HTTP_STATUS_CONTINUE)
		  hm->phase = _PAPPL_HTTP_PHASE_SERVER_HEADERS;
	      }
	      else if (line[0])
	      {
		hm->status = HTTP_STATUS_ERROR;
		hm->error  = "Unexpected server response seen during client data phase.";
	      }
	      break;
	  }
	  break;

      default :
	  hm->status = HTTP_STATUS_ERROR;
	  hm->error  = "Unexpected HTTP state.";
	  break;
    }
  }

  return (hm->status);
}


//
// 'http_buffer_add()' - Add bytes to the buffer from the data stream.
//

static size_t				// O  - Number of bytes added
http_buffer_add(
    _pappl_http_buffer_t *hb,		// I  - HTTP buffer
    const char           **data,	// IO - Pointer to data
    size_t               *datasize)	// IO - Bytes of data (remaining)
{
  size_t	bytes = 0;		// Bytes to add


  if (*datasize > 0 && hb->used < HTTP_MAX_BUFFER)
  {
    // Copy more data into client_data
    if ((bytes = HTTP_MAX_BUFFER - hb->used) > *datasize)
      bytes = *datasize;

    memcpy(hb->data + hb->used, *data, bytes);

    (*data)     += bytes;
    (*datasize) -= bytes;
    hb->used    += bytes;
  }

  return (bytes);
}


//
// 'http_buffer_consume()' - Consume bytes from the buffer or data stream.
//

static size_t				// O  - Total bytes consumed
http_buffer_consume(
    _pappl_http_buffer_t *hb,		// I  - HTTP buffer
    const char           **data,	// IO - Pointer to (new) data
    size_t               *datasize,	// IO - Bytes of (new) data
    size_t               bytes)		// I  - Bytes to consume
{
  size_t	total = 0;		// Total bytes consumed


  // Consume first from the buffer
  if (hb->used > 0)
  {
    if (bytes >= hb->used)
    {
      // Consume all of the buffer
      bytes -= hb->used;
      total += hb->used;
      hb->used = 0;
    }
    else
    {
      // Consume part of the buffer and move things around
      memmove(hb->data, hb->data + bytes, hb->used - bytes);
      hb->used -= bytes;
      total    += bytes;
      bytes    = 0;
    }
  }

  if (bytes > 0 && *datasize > 0)
  {
    // Didn't consume everything requested, pull from the data stream
    if (bytes >= *datasize)
    {
      // Consume all of the data stream
      (*data)   += *datasize;
      total     += *datasize;
      *datasize = 0;
    }
    else
    {
      // Consume part of the data stream
      (*data)     += bytes;
      (*datasize) -= bytes;
      total       += bytes;
    }
  }

  // Return the total number of bytes consumed
  return (total);
}


//
// 'http_buffer_line()' - Copy a single line from the buffer or data stream,
//                        stripping CR and LF.
//

static char *				// O - Pointer to line or `NULL` if none
http_buffer_line(
    _pappl_http_monitor_t *hm,		// I  - HTTP monitor
    _pappl_http_buffer_t  *hb,		// I  - HTTP buffer
    const char            **data,	// IO - Pointer to data
    size_t                *datasize,	// IO - Number of bytes of data
    char                  *line,	// I  - Line buffer
    size_t                linesize)	// I  - Size of line buffer
{
  char		*lineptr,		// Pointer into line buffer
		*lineend;		// Pointer to end of line buffer
  const char	*dataptr,		// Pointer into data buffer
		*dataend;		// Pointer to end of data buffer
  bool		eol = false;		// EOL seen?


  // See if the buffer or data stream contains a newline...
  if ((hb->used == 0 || !memchr(hb->data, '\n', hb->used)) && (*datasize == 0 || !memchr(*data, '\n', *datasize)))
  {
    // No, try to add the data stream to the buffer and return...
    http_buffer_add(hb, data, datasize);
    *line = '\0';

    if (*datasize > 0)
    {
      // Line is too long...
      hm->status = HTTP_STATUS_ERROR;
      hm->error  = "Line too large for buffer.";
    }

    return (NULL);
  }

  // Grab one line from the input buffer
  lineptr = line;
  lineend = line + linesize - 1;
  dataptr = hb->data;
  dataend = hb->data + hb->used;

  while (!eol && dataptr < dataend)
  {
    if (*dataptr == '\n')
      eol = true;
    else if (*dataptr != '\r' && lineptr < lineend)
      *lineptr++ = *dataptr;

    dataptr ++;
  }

  // If we consumed any data in the buffer, move the remainder to the front
  // of the buffer...
  if (dataptr < dataend)
    memmove(hb->data, dataptr, dataend - dataptr);

  hb->used -= (size_t)(dataptr - hb->data);

  if (!eol)
  {
    // Didn't get the whole line in our buffer, grab the rest from the data
    // stream...
    dataptr = *data;
    dataend = *data + *datasize;

    while (!eol && dataptr < dataend)
    {
      if (*dataptr == '\n')
	eol = true;
      else if (*dataptr != '\r' && lineptr < lineend)
	*lineptr++ = *dataptr;

      dataptr ++;
    }

    *data     = dataptr;
    *datasize = (size_t)(dataend - dataptr);
  }

  // Add a trailing nul and return the line...
  *lineptr = '\0';

  return (line);
}
