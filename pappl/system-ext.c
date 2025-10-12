//
// External command support for the Printer Application Framework
//
// Copyright © 2025 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#include "pappl-private.h"
#if _WIN32
#  include <process.h>
#  define WEXITSTATUS(s) (s)
#else
#  include <spawn.h>
#  include <sys/wait.h>
#endif // _WIN32

extern char **environ;


//
// Command state/data...
//

typedef struct _pappl_command_s		// Per-process data for a command
{
  pappl_system_t	*system;	// System
  pappl_printer_t	*printer;	// Printer, if any
  pappl_job_t		*job;		// Job, if any
  char			name[256];	// Command name
  pid_t			pid;		// Process ID
  int			stderr_pipe;	// stderr pipe for messages
  char			buffer[8192];	// Message buffer
  size_t		bufused;	// Number of bytes used in buffer
} _pappl_command_t;


//
// Local functions...
//

#if !_WIN32
static char	*read_line(_pappl_command_t *command, char *line, size_t linesize);
static void	*wait_command(_pappl_command_t *command);
#endif // !_WIN32


//
// 'papplSystemRunExtCommand()' - Execute a program with restrictions.
//

int					// O - Process ID or `0` on error
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
#if _WIN32
  // TODO: Implement Windows external command support
  (void)system;
  (void)printer;
  (void)job;
  (void)args;
  (void)env;
  (void)infd;
  (void)outfd;
  (void)allow_networking;

#else
  cups_len_t		i,		// Looping var
			count;		// Number of values
  int			pargc;		// Number of arguments to pappl-exec
  const char 		*pargv[1000];	// Arguments to pappl-exec
  _pappl_command_t	*command;	// Command state/date
  const char		*name;		// Command name
  int			stderr_pipe[2];	// Pipe for stderr messages
  posix_spawn_file_actions_t pactions;	// Actions for posix_spawn
  int			perr;		// Error from posix_spawn


  // Range check input...
  if (!system || (job && !printer) || !args)
  {
    errno = EINVAL;
    return (0);
  }

  if (!env)
    env = (const char **)environ;

  // Create a pipe for stderr output from the command...
  if (pipe(stderr_pipe))
    return (0);

  fcntl(stderr_pipe[0], F_SETFD, fcntl(stderr_pipe[0], F_GETFD) | FD_CLOEXEC);
  fcntl(stderr_pipe[1], F_SETFD, fcntl(stderr_pipe[1], F_GETFD) | FD_CLOEXEC);

  // Allocate and initialize command data...
  if ((command = calloc(1, sizeof(_pappl_command_t))) == NULL)
    goto error;

  command->system  = system;
  command->printer = printer;
  command->job     = job;

  if ((name = strrchr(args[0], '/')) != NULL)
    name ++;
  else
    name = args[0];

  cupsCopyString(command->name, name, sizeof(command->name));

  command->stderr_pipe = stderr_pipe[0];

  // Build the command-line...
  // TODO: Document and/or lock down the location of pappl-exec
  if ((pargv[0] = getenv("PAPPL_EXEC")) == NULL)
    pargv[0] = "pappl-exec";

  pargc = 1;

  for (i = 0, count = cupsArrayGetCount(system->ext_readexec); i < count; i ++)
  {
    if (pargc >= (int)(sizeof(pargv) / sizeof(pargv[0]) - 3))
    {
      errno = E2BIG;
      goto error;
    }

    pargv[pargc++] = "-X";
    pargv[pargc++] = (char *)cupsArrayGetElement(system->ext_readexec, i);
  }

  for (i = 0, count = cupsArrayGetCount(system->ext_readonly); i < count; i ++)
  {
    if (pargc >= (int)(sizeof(pargv) / sizeof(pargv[0]) - 3))
    {
      errno = E2BIG;
      goto error;
    }

    pargv[pargc++] = "-R";
    pargv[pargc++] = (char *)cupsArrayGetElement(system->ext_readonly, i);
  }

  for (i = 0, count = cupsArrayGetCount(system->ext_readwrite); i < count; i ++)
  {
    if (pargc >= (int)(sizeof(pargv) / sizeof(pargv[0]) - 3))
    {
      errno = E2BIG;
      goto error;
    }

    pargv[pargc++] = "-W";
    pargv[pargc++] = (char *)cupsArrayGetElement(system->ext_readwrite, i);
  }

  if (system->ext_user)
  {
    if (pargc >= (int)(sizeof(pargv) / sizeof(pargv[0]) - 3))
    {
      errno = E2BIG;
      goto error;
    }

    pargv[pargc++] = "-u";
    pargv[pargc++] = system->ext_user;
  }

  if (system->ext_group)
  {
    if (pargc >= (int)(sizeof(pargv) / sizeof(pargv[0]) - 3))
    {
      errno = E2BIG;
      goto error;
    }

    pargv[pargc++] = "-g";
    pargv[pargc++] = system->ext_group;
  }

  if (allow_networking)
  {
    if (pargc >= (int)(sizeof(pargv) / sizeof(pargv[0]) - 2))
    {
      errno = E2BIG;
      goto error;
    }

    pargv[pargc++] = "-n";
  }

  for (i = 0; args[i]; i ++)
  {
    if (pargc >= (int)(sizeof(pargv) / sizeof(pargv[0]) - 2))
    {
      errno = E2BIG;
      goto error;
    }

    pargv[pargc++] = (char *)args;
  }

  pargv[pargc] = NULL;

  // File actions...
  posix_spawn_file_actions_init(&pactions);

  // stdin
  if (infd < 0)
    posix_spawn_file_actions_addopen(&pactions, /*filedes*/0, "/dev/null", O_RDONLY, /*omode*/0);
  else
    posix_spawn_file_actions_adddup2(&pactions, infd, /*newfiledes*/0);

  // stdout
  if (outfd < 0)
    posix_spawn_file_actions_addopen(&pactions, /*filedes*/1, "/dev/null", O_WRONLY, /*omode*/0);
  else
    posix_spawn_file_actions_adddup2(&pactions, infd, /*newfiledes*/1);

  // stderr
  posix_spawn_file_actions_adddup2(&pactions, stderr_pipe[1], /*newfiledes*/2);

  // Execute the command...
  if ((perr = posix_spawnp(&command->pid, (char *)pargv[0], &pactions, /*addrp*/NULL, (char **)pargv, (char **)env)) < 0)
  {
    errno = perr;
    posix_spawn_file_actions_destroy(&pactions);
    goto error;
  }

  posix_spawn_file_actions_destroy(&pactions);
  close(stderr_pipe[1]);

  cupsThreadCreate((cups_thread_func_t)wait_command, command);

  return (command->pid);

  // If we get here, something bad happened...
  error:

  close(stderr_pipe[0]);
  close(stderr_pipe[1]);
  free(command);
#endif // _WIN32

  return (0);
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
  if (system && !system->is_running)
  {
    free(system->ext_user);
    system->ext_user = username ? strdup(username) : NULL;

    free(system->ext_group);
    system->ext_group = groupname ? strdup(groupname) : NULL;
  }
}


//
// 'papplSystemAddExtCommandPath()' - Add a file or directory that can be executed.
//

void
papplSystemAddExtCommandPath(
    pappl_system_t *system,		// I - System
    const char     *path)		// I - File or directory name to add
{
  if (system && !system->is_running && !path)
  {
    if (!system->ext_readexec)
      system->ext_readexec = cupsArrayNewStrings(NULL, '\0');

    cupsArrayAdd(system->ext_readexec, (void *)path);
  }
}


//
// 'papplSystemAddExtReadOnlyPath()' - Add a file or directory that can be read.
//

void
papplSystemAddExtReadOnlyPath(
    pappl_system_t *system,		// I - System
    const char     *path)		// I - File or directory name to add
{
  if (system && !system->is_running && !path)
  {
    if (!system->ext_readonly)
      system->ext_readonly = cupsArrayNewStrings(NULL, '\0');

    cupsArrayAdd(system->ext_readonly, (void *)path);
  }
}


//
// 'papplSystemAddExtReadWritePath()' - Add a file or directory that can be read and written.
//

void
papplSystemAddExtReadWritePath(
    pappl_system_t *system,		// I - System
    const char     *path)		// I - File or directory name to add
{
  if (system && !system->is_running && !path)
  {
    if (!system->ext_readwrite)
      system->ext_readwrite = cupsArrayNewStrings(NULL, '\0');

    cupsArrayAdd(system->ext_readwrite, (void *)path);
  }
}


#if !_WIN32
//
// 'read_line()' - Read a line from the stderr pipe.
//

static char *				// O - Line or `NULL` on EOF
read_line(_pappl_command_t *command,	// I - Command data/state
          char             *line,	// I - Line buffer
          size_t           linesize)	// I - Size of line buffer
{
  char		*lfptr = NULL;		// Pointer to line feed in buffer
  size_t	lflen;			// Length of line in buffer


  // Make sure we have a message line from the command...
  while (command->bufused == 0 || (lfptr = memchr(command->buffer, '\n', command->bufused)) == NULL)
  {
    ssize_t	bytes;			// Bytes read

    if ((bytes = read(command->stderr_pipe, command->buffer + command->bufused, sizeof(command->buffer) - command->bufused)) < 0)
    {
      // Continue if this is a temporary error...
      if (errno == EINTR || errno == EAGAIN)
        continue;

      // Otherwise stop...
      break;
    }
    else if (bytes == 0)
    {
      // End of file so stop now...
      break;
    }

    command->bufused += (size_t)bytes;
  }

  // Now see what we have...
  if (lfptr == NULL)
    lfptr = command->buffer + command->bufused;

  if ((lflen = (size_t)(lfptr - command->buffer)) == 0)
    return (NULL);			// Nothing more, end-of-file...

  if (lflen > (linesize - 1))
  {
    // Copy a partial message line...
    memcpy(line, command->buffer, linesize - 1);
    line[linesize - 1] = '\0';
  }
  else
  {
    // Copy the whole message line...
    memcpy(line, command->buffer, lflen);
    line[lflen] = '\0';
  }

  // Discard the line in the buffer...
  if (lflen < command->bufused)
    lflen ++;				// Also discard the newline...

  if (lflen < command->bufused)
    memmove(command->buffer, command->buffer + lflen, command->bufused - lflen);

  command->bufused -= lflen;

  // Return the line we copied...
  return (line);
}


//
// 'wait_command()' - Wait for the command to finish, processing any messages the command sends.
//

static void *				// O - Thread exit status (`NULL`)
wait_command(_pappl_command_t *command)	// I - Command data/state
{
  int		status;			// Exit status
  char		line[2048],		// Message from the command on stderr
		*lineptr;		// Pointer into the line
  pappl_loglevel_t level;		// Log level for the message


  // Read messages until the process is done...
  while (read_line(command, line, sizeof(line)))
  {
    lineptr = line;

    if (!strncmp(line, "FATAL:", 6))
    {
      level   = PAPPL_LOGLEVEL_FATAL;
      lineptr += 6;
    }
    else if (!strncmp(line, "ERROR:", 6))
    {
      level   = PAPPL_LOGLEVEL_ERROR;
      lineptr += 6;
    }
    else if (!strncmp(line, "WARN:", 5))
    {
      level   = PAPPL_LOGLEVEL_WARN;
      lineptr += 5;
    }
    else if (!strncmp(line, "INFO:", 5))
    {
      level   = PAPPL_LOGLEVEL_INFO;
      lineptr += 5;
    }
    else if (!strncmp(line, "DEBUG:", 6))
    {
      level = PAPPL_LOGLEVEL_DEBUG;
      lineptr += 6;
    }
    else
    {
      level = PAPPL_LOGLEVEL_DEBUG;
    }

    // Skip leading whitespace...
    while (*lineptr && isspace(*lineptr & 255))
      lineptr ++;

    // Log it to the corresponding place...
    if (command->job)
      papplLogJob(command->job, level, "[%s] %s", command->name, lineptr);
    else if (command->printer)
      papplLogPrinter(command->printer, level, "[%s] %s", command->name, lineptr);
    else
      papplLog(command->system, level, "[%s] %s", command->name, lineptr);
  }

  // Get the exit status of the program...
  while (waitpid(command->pid, &status, 0) < 0)
  {
    if (errno != EINTR)
    {
      status = -1;
      break;
    }
  }

  // Log why the program exited...
  level = PAPPL_LOGLEVEL_ERROR;

  if (status < 0)
  {
    snprintf(line, sizeof(line), "Unable to get exit status: %s", strerror(errno));
  }
  else if (status == 0)
  {
    cupsCopyString(line, "Completed successfully.", sizeof(line));
    level = PAPPL_LOGLEVEL_INFO;
  }
  else if (WIFEXITED(status))
  {
    snprintf(line, sizeof(line), "Completed with status %d.", WEXITSTATUS(status));
  }
  else if (WIFSIGNALED(status))
  {
    if (WCOREDUMP(status))
      snprintf(line, sizeof(line), "Crashed on signal %d.", WTERMSIG(status));
    else
      snprintf(line, sizeof(line), "Terminated on signal %d.", WTERMSIG(status));
  }
  else
  {
    snprintf(line, sizeof(line), "Stopped on signal %d.", WSTOPSIG(status));
  }

  if (command->job)
    papplLogJob(command->job, level, "[%s] %s", command->name, line);
  else if (command->printer)
    papplLogPrinter(command->printer, level, "[%s] %s", command->name, line);
  else
    papplLog(command->system, level, "[%s] %s", command->name, line);

  // Close the stderr pipe and free memory used for data/state...
  close(command->stderr_pipe);
  free(command);

  // Exit the monitoring thread...
  return (NULL);
}
#endif // !_WIN32
