//
// File device support code for the Printer Application Framework
//
// Copyright © 2019-2020 by Michael R Sweet.
// Copyright © 2007-2019 by Apple Inc.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

//
// Include necessary headers...
//

#include "device-private.h"


//
// Local functions...
//

static void	pappl_file_close(pappl_device_t *device);
static bool	pappl_file_open(pappl_device_t *device, const char *device_uri, const char *name);
static ssize_t	pappl_file_write(pappl_device_t *device, const void *buffer, size_t bytes);


//
// '_papplDeviceAddFileScheme()' - Add the "file" device URI scheme.
//

void
_papplDeviceAddFileScheme(void)
{
  papplDeviceAddScheme("file", PAPPL_DTYPE_FILE, NULL, pappl_file_open, pappl_file_close, NULL, pappl_file_write, NULL);
}


//
// 'pappl_file_close()' - Close a file.
//

static void
pappl_file_close(pappl_device_t *device)// I - Device
{
  int		*fd;			// File descriptor


  // Make sure we have a valid file descriptor...
  if ((fd = papplDeviceGetData(device)) == NULL || *fd < 0)
    return;

  close(*fd);
  free(fd);

  papplDeviceSetData(device, NULL);
}


//
// 'pappl_file_open()' - Open a file.
//

static bool				// O - `true` on success, `false` otherwise
pappl_file_open(
    pappl_device_t *device,		// I - Device
    const char     *device_uri,		// I - Device URI
    const char     *name)		// I - Job name
{
  int		*fd;			// File descriptor
  char		scheme[32],		// URI scheme
		userpass[32],		// Username/password (not used)
		host[256],		// Host name or make
		resource[256],		// Resource path, if any
		*options,		// Pointer to options, if any
		filename[1024],		// Filename
		*fileptr;		// Pointer into filename
  int		port;			// Port number
  struct stat	resinfo;		// Resource path information


  // Allocate memory for a file descriptor
  if ((fd = (int *)malloc(sizeof(int))) == NULL)
  {
    papplDeviceError(device, "Unable to allocate memory for file: %s", strerror(errno));
    return (false);
  }

  // Get the resource path for the filename...
  httpSeparateURI(HTTP_URI_CODING_ALL, device_uri, scheme, sizeof(scheme), userpass, sizeof(userpass), host, sizeof(host), &port, resource, sizeof(resource));

  if ((options = strchr(resource, '?')) != NULL)
    *options++ = '\0';

  if (!stat(resource, &resinfo))
    resinfo.st_mode = S_IFREG | 0644;

  if (S_ISDIR(resinfo.st_mode))
  {
    // Resource is a directory, so create an output filename using the job name
    snprintf(filename, sizeof(filename), "%s/%s.prn", resource, name);

    for (fileptr = filename + strlen(resource) + 1; *fileptr; fileptr ++)
    {
      if (*fileptr < ' ' || *fileptr == 0x7f || *fileptr & 0x80)
        *fileptr = '_';
    }

    *fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0666);
  }
  else if (S_ISCHR(resinfo.st_mode))
  {
    // Resource is a character device...
    *fd = open(resource, O_WRONLY | O_EXCL);
  }
  else if (S_ISREG(resinfo.st_mode))
  {
    // Resource is a regular file...
    *fd = open(resource, O_WRONLY | O_APPEND | O_CREAT, 0666);
  }

  // If we were unable to open the file, return an error...
  if (*fd < 0)
  {
    papplDeviceError(device, "Unable to open '%s': %s", resource, strerror(errno));
    free(fd);
    return (false);
  }

  // Otherwise, save the file descriptor and return success...
  papplDeviceSetData(device, fd);
  return (true);
}


//
// 'pappl_file_write()' - Write to a file.
//

static ssize_t				// O - Bytes written
pappl_file_write(pappl_device_t *device,// I - Device
                 const void     *buffer,// I - Buffer to write
                 size_t         bytes)	// I - Bytes to write
{
  int		*fd;			// File descriptor
  const char	*ptr;			// Pointer into buffer
  ssize_t	count,			// Total bytes written
		written;		// Bytes written this time


  // Make sure we have a valid file descriptor...
  if ((fd = papplDeviceGetData(device)) == NULL || *fd < 0)
    return (-1);

  for (count = 0, ptr = (const char *)buffer; count < (ssize_t)bytes; count += written, ptr += written)
  {
    if ((written = write(*fd, ptr, bytes - (size_t)count)) < 0)
    {
      if (errno == EINTR || errno == EAGAIN)
      {
        written = 0;
	continue;
      }

      count = -1;
      break;
    }
  }

  return (count);
}
