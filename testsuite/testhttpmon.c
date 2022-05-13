//
// HTTP monitor unit tests for the Printer Application Framework
//
// Copyright © 2021-2022 by Michael R Sweet.
// Copyright © 2012 by Apple Inc.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

//
// Include necessary headers...
//

#include <pappl/httpmon-private.h>
#include "test.h"


//
// Local test data for the unit tests.
//
// Tests are a series of character strings; each string starts with a C if the
// data comes from the client/USB host and S if it comes from the server/USB
// device. The final element in the array must be NULL...
//

static const char * const good_basic_get[] =
{
  "CGET / HTTP/1.1\r\n"
      "Host: localhost:1234\r\n"
      "\r\n",
  "SHTTP/1.1 200 OK\r\n"
      "Content-Type: text/plain\r\n"
      "Content-Length: 13\r\n"
      "\r\n",
  "SHello, World!",
  NULL
};

static const char * const bad_basic_get[] =
{
  "CGET /badresource HTTP/1.1\r\n"
      "Host: localhost:1234\r\n"
      "\r\n",
  "SHTTP/1.1 400 Bad Request\r\n"
      "Content-Type: text/plain\r\n"
      "Content-Length: 12\r\n"
      "\r\n",
  "SBad Request!",
  NULL
};

static const char * const basic_head[] =
{
  "CHEAD / HTTP/1.1\r\n"
      "Host: localhost:1234\r\n"
      "\r\n",
  "SHTTP/1.1 200 OK\r\n"
      "Content-Type: text/plain\r\n"
      "Content-Length: 13\r\n"
      "\r\n",
  NULL
};

static const char * const basic_post[] =
{
  "CPOST / HTTP/1.1\r\n"
      "Host: localhost:1234\r\n"
      "Content-Type: application/ipp\r\n"
      "Content-Length: 13\r\n"
      "\r\n",
  "CHello, World!",	// This doesn't need to be real IPP message data */
  "SHTTP/1.1 200 OK\r\n"
      "Content-Type: application/ipp\r\n"
      "Content-Length: 13\r\n"
      "\r\n",
  "SHello, World!",	// This doesn't need to be real IPP message data */
  NULL
};

static const char * const post_continue[] =
{
  "CPOST / HTTP/1.1\r\n"
      "Host: localhost:1234\r\n"
      "Content-Type: application/ipp\r\n"
      "Content-Length: 26\r\n"
      "Expect: 100-continue\r\n"
      "\r\n",
  "CHello, World!",	// This doesn't need to be real IPP message data */
  "SHTTP/1.1 100 Continue\r\n"
      "\r\n",
  "CHello, World!",	// This doesn't need to be real IPP message data */
  "SHTTP/1.1 200 OK\r\n"
      "Content-Type: application/ipp\r\n"
      "Content-Length: 13\r\n"
      "\r\n",
  "SHello, World!",	// This doesn't need to be real IPP message data */
  NULL
};

static const char * const post_no_continue[] =
{
  "CPOST / HTTP/1.1\r\n"
      "Host: localhost:1234\r\n"
      "Content-Type: application/ipp\r\n"
      "Content-Length: 13\r\n"
      "Expect: 100-continue\r\n"
      "\r\n",
  "CHello, World!",	// This doesn't need to be real IPP message data */
  "SHTTP/1.1 200 OK\r\n"
      "Content-Type: application/ipp\r\n"
      "Content-Length: 13\r\n"
      "\r\n",
  "SHello, World!",	// This doesn't need to be real IPP message data */
  NULL
};

static const char * const good_chunked_get[] =
{
  "CGET / HTTP/1.1",
  "C\r\n",
  "CHost: localhost:1234",
  "C\r\n",
  "C\r\n",
  "SHTTP/1.1 200 OK",
  "S\r\n",
  "SContent-Type: text/plain",
  "S\r\n",
  "STransfer-Encoding: chunked",
  "S\r\n",
  "S\r\n",
  "SD\r\n",
  "SHello, World!",
  "S\r\n",
  "S0\r\n",
  "S\r\n",
  NULL
};

static const char * const bad_chunked_get[] =
{
  "CGET / HTTP/1.1",
  "C\r\n",
  "CHost: localhost:1234",
  "C\r\n",
  "C\r\n",
  "S200 OK",
  "S\r\n",
  "SContent-Type: text/plain",
  "S\r\n",
  "STransfer-Encoding: chunked",
  "S\r\n",
  "S\r\n",
  "SD\r\n",
  "SHello, World!",
  "S\r\n",
  // Missing trailing 0 chunk...
  NULL
};

static const char * const chunked_post[] =
{
  "CPOST / HTTP/1.1\r\n",
  "CHost: localhost:1234\r\n",
  "CContent-Type: application/ipp\r\n",
  "CTransfer-Encoding: chunked\r\n",
  "C\r\n",
  "CD\r\n",
  "CHello, World!",	// This doesn't need to be real IPP message data */
  "C\r\n",
  "C0\r\n",
  "C\r\n",
  "SHTTP/1.1 200 OK\r\n",
  "SContent-Type: application/ipp\r\n",
  "STransfer-Encoding: chunked\r\n",
  "S\r\n",
  "SD\r\n",
  "SHello, World!",
  "S\r\n",
  "SD\r\nHello, World!\r\n"
      "D\r\nHello, World!\r\n",	// 2 chunks in one buffer
  "S0\r\n",
  "S\r\n",
  NULL
};

static const char * const no_content_length_response[] =
{
  "CPOST /eSCL/ScanJobs HTTP/1.1\r\n"
      "Host: localhost:1234\r\n"
      "Content-Type:text/xml\r\n"
      "Content-Length: 13\r\n",
  "C\r\n",
  "CHello, World!",	// This doesn't need to be real XML eSCL job settings */
  "SHTTP/1.1 201 Created\r\n"
      "Accept-Encoding: identity\r\n"
      "Date: Mon, 01 Jun 2015 22:01:12GMT\r\n"
      "Server: KM-MFP-http/V0.0.1\r\n"
      "Location: http://localhost:1234/eSCL/ScanJobs/1001\r\n",
  "S\r\n",
  NULL
};


//
// Local functions...
//

static bool	run_tests(const char *name, _pappl_http_monitor_t *hm, const char * const *strings, http_status_t expected);


//
// 'main()' - Test the HTTP monitor code.
//

int					// O - Exit status
main(void)
{
  bool			pass = true;	// Pass or fail
  _pappl_http_monitor_t	hm;		// HTTP monitor


  _papplHTTPMonitorInit(&hm);

  pass &= run_tests("Good Basic GET", &hm, good_basic_get, HTTP_STATUS_OK);
  hm.data_remaining = 1;
  pass &= run_tests("No Content Length Response", &hm, no_content_length_response, HTTP_STATUS_CREATED);
  pass &= run_tests("Bad Basic GET", &hm, bad_basic_get, HTTP_STATUS_BAD_REQUEST);
  pass &= run_tests("Basic HEAD", &hm, basic_head, HTTP_STATUS_OK);
  pass &= run_tests("Basic POST", &hm, basic_post, HTTP_STATUS_OK);
  pass &= run_tests("POST Expect w/Continue", &hm, post_continue, HTTP_STATUS_OK);
  pass &= run_tests("POST Expect w/o Continue", &hm, post_no_continue, HTTP_STATUS_OK);
  pass &= run_tests("Good Chunked GET", &hm, good_chunked_get, HTTP_STATUS_OK);
  pass &= run_tests("Chunked POST", &hm, chunked_post, HTTP_STATUS_OK);
  pass &= run_tests("Bad Chunked GET", &hm, bad_chunked_get, HTTP_STATUS_ERROR);

  return (pass ? 0 : 1);
}


//
// 'run_tests()' - Run tests from an array of client/server data strings
//

static bool				// O - `true` on success, `false` on failure
run_tests(
    const char            *name,	// I - Test name
    _pappl_http_monitor_t *hm,		// I - HTTP monitor
    const char * const    *strings,	// I - Array of test data
    http_status_t         expected)	// I - Expected HTTP status
{
  http_status_t	status = HTTP_STATUS_CONTINUE;
					// Current HTTP status
  const char	*s;			// Current string
  size_t	len;			// Length of string


  // Show the test name...
  testBegin("%s: ", name);

  // Loop until we get an error or run out of data...
  while (status != HTTP_STATUS_ERROR && *strings)
  {
    s   = *strings++;
    len = strlen(s + 1);
    if (*s == 'C')
    {
      s ++;
      status = _papplHTTPMonitorProcessHostData(hm, &s, &len);
    }
    else
    {
      status = _papplHTTPMonitorProcessDeviceData(hm, s + 1, len);
    }
  }

  if (status != HTTP_STATUS_ERROR && _papplHTTPMonitorGetState(hm) != HTTP_STATE_WAITING)
  {
    hm->status = status = HTTP_STATUS_ERROR;
    hm->error  = "Not in the HTTP_WAITING state.";
  }

  if (status == expected)
    testEnd(true);
  else if (status == HTTP_STATUS_ERROR)
    testEndMessage(false, "%s", _papplHTTPMonitorGetError(hm));
  else if (status != expected)
    testEndMessage(false, "got status %d", status);

  return (status == expected);
}
