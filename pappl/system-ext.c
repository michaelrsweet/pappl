//
// External command support for the Printer Application Framework
//
// Copyright © 2025 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#include "pappl-private.h"


//
// 'papplSystemRunExtCommand()' - Execute a program with restrictions.
//

int					// O - Process ID or `-1` on error
papplSystemRunExtCommand(
    pappl_system_t  *system,		// I - System
    pappl_printer_t *printer,		// I - Printer or `NULL` for none
    pappl_job_t     *job,		// I - Job or `NULL` for none
    const char      **args,		// I - Command arguments
    const char      **env,		// I - Environment variables or `NULL` for default
    int             infd,		// I - Standard input file descriptor
    int             outfd,		// I - Standard output file descriptor
    bool            allow_networking)	// I - `true` to allow outgoing network connections, `false` otherwise
{
  (void)system;
  (void)printer;
  (void)job;
  (void)args;
  (void)env;
  (void)infd;
  (void)outfd;
  (void)allow_networking;

  return (-1);
}


//
// 'papplSystemSetExtUserGroup()' - Set an alternate user and group for external programs.
//

void
papplSystemSetExtUserGroup(
    pappl_system_t *system,		// I - System
    const char     *username,		// I - User to use or `NULL` for current user
    const char     *groupname)		// I - Group to use or `NULL` for current group
{
  (void)system;
  (void)username;
  (void)groupname;
}


//
// 'papplSystemAddExtCommandPath()' - Add a file or directory that can be executed.
//

void
papplSystemAddExtCommandPath(
    pappl_system_t *system,		// I - System
    const char     *path)		// I - File or directory name to add
{
  (void)system;
  (void)path;
}


//
// 'papplSystemAddExtReadOnlyPath()' - Add a file or directory that can be read.
//

void
papplSystemAddExtReadOnlyPath(
    pappl_system_t *system,		// I - System
    const char     *path)		// I - File or directory name to add
{
  (void)system;
  (void)path;
}


//
// 'papplSystemAddExtReadWritePath()' - Add a file or directory that can be read and written.
//

void
papplSystemAddExtReadWritePath(
    pappl_system_t *system,		// I - System
    const char     *path)		// I - File or directory name to add
{
  (void)system;
  (void)path;
}
