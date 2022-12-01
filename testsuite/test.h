//
// Unit test header for C/C++ programs.
//
// Copyright © 2021-2022 by Michael R Sweet.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//

#ifndef TEST_H
#  define TEST_H
#  include <stdio.h>
#  include <stdlib.h>
#  include <stdarg.h>
#  include <stdbool.h>
#  include <string.h>
#  if _WIN32
#    define isatty(f) _isatty(f)
#  else
#    include <unistd.h>
#  endif // !_WIN32
#  ifdef __cplusplus
extern "C" {
#  endif // __cplusplus
#  if _WIN32
#    define TEST_FORMAT(a,b)
#  elif defined(__has_extension) || defined(__GNUC__)
#    define TEST_FORMAT(a,b)     __attribute__ ((__format__(__printf__, a,b)))
#  else
#    define TEST_FORMAT(a,b)
#  endif // _WIN32

//
// This header implements a simple unit test framework for C/C++ programs.  Inline
// functions are provided to write a test summary to stdout and the details to stderr.
//

static int		test_progress;	// Current progress
static inline void	testBegin(const char *title, ...) TEST_FORMAT(1,2);
static inline void	testEnd(bool pass);
static inline void	testEndMessage(bool pass, const char *message, ...) TEST_FORMAT(2,3);
static inline void	testError(const char *error, ...) TEST_FORMAT(1,2);
static inline void	testHexDump(const unsigned char *buffer, size_t bytes);
static inline void	testMessage(const char *error, ...) TEST_FORMAT(1,2);
static inline void	testProgress(void);


// Start a test
static inline void
testBegin(const char *title, ...)	// I - printf-style title string
{
  char		buffer[1024];		// Formatted title string
  va_list	ap;			// Pointer to additional arguments


  // Format the title string
  va_start(ap, title);
  vsnprintf(buffer, sizeof(buffer), title, ap);
  va_end(ap);

  // Send the title to stdout and stderr...
  test_progress = 0;

  printf("%s: ", buffer);
  fflush(stdout);

#ifndef DEBUG
  if (!isatty(2))
    fprintf(stderr, "%s: ", buffer);
#endif // !DEBUG
}

// End a test with no additional information
static inline void
testEnd(bool pass)			// I - `true` if the test passed, `false` otherwise
{
  // Send the test result to stdout and stderr
  if (test_progress)
    putchar('\b');

  puts(pass ? "PASS" : "FAIL");
#ifndef DEBUG
  if (!isatty(2))
    fputs(pass ? "PASS\n" : "FAIL\n", stderr);
#endif // !DEBUG
}


// End a test with no additional information
static inline void
testEndMessage(bool       pass,		// I - `true` if the test passed, `false` otherwise
               const char *message, ...)// I - printf-style message
{
  char		buffer[1024];		// Formatted title string
  va_list	ap;			// Pointer to additional arguments


  // Format the title string
  va_start(ap, message);
  vsnprintf(buffer, sizeof(buffer), message, ap);
  va_end(ap);

  // Send the test result to stdout and stderr
  if (test_progress)
    putchar('\b');

  printf(pass ? "PASS (%s)\n" : "FAIL (%s)\n", buffer);
#ifndef DEBUG
  if (!isatty(2))
    fprintf(stderr, pass ? "PASS (%s)\n" : "FAIL (%s)\n", buffer);
#endif // !DEBUG
}

// Show/update a progress spinner
static inline void
testProgress(void)
{
  if (test_progress)
    putchar('\b');
  putchar("-\\|/"[test_progress & 3]);
  fflush(stdout);

  test_progress ++;
}

// Show an error to stderr...
static inline void
testError(const char *error, ...)	// I - printf-style error string
{
  char		buffer[1024];		// Formatted title string
  va_list	ap;			// Pointer to additional arguments


  // Format the error string
  va_start(ap, error);
  vsnprintf(buffer, sizeof(buffer), error, ap);
  va_end(ap);

  // Send the error to stderr...
  fprintf(stderr, "%s\n", buffer);
}

// Show a message to stdout and stderr...
static inline void
testMessage(const char *error, ...)	// I - printf-style error string
{
  char		buffer[1024];		// Formatted title string
  va_list	ap;			// Pointer to additional arguments


  // Format the error string
  va_start(ap, error);
  vsnprintf(buffer, sizeof(buffer), error, ap);
  va_end(ap);

  // Send the message to stdout and stderr too if needed...
  printf("%s\n", buffer);
#ifndef DEBUG
  if (!isatty(2))
    fprintf(stderr, "%s\n", buffer);
#endif // !DEBUG
}

// Show a hex dump of a buffer to stderr...
static inline void
testHexDump(const unsigned char *buffer,// I - Buffer
            size_t              bytes)	// I - Number of bytes
{
  size_t	i, j;			// Looping vars
  int		ch;			// Current ASCII char


  // Show lines of 16 bytes at a time...
  for (i = 0; i < bytes; i += 16)
  {
    // Show the offset...
    fprintf(stderr, "%04x ", (unsigned)i);

    // Then up to 16 bytes in hex...
    for (j = 0; j < 16; j ++)
    {
      if ((i + j) < bytes)
        fprintf(stderr, " %02x", buffer[i + j]);
      else
        fputs("   ", stderr);
    }

    // Then the ASCII representation of the bytes...
    fputs("  ", stderr);

    for (j = 0; j < 16 && (i + j) < bytes; j ++)
    {
      ch = buffer[i + j] & 127;

      if (ch < ' ' || ch == 127)
        fputc('.', stderr);
      else
        fputc(ch, stderr);
    }

    fputc('\n', stderr);
  }
}

#  ifdef __cplusplus
}
#  endif // __cplusplus
#endif // !TEST_H
