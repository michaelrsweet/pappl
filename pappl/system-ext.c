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
#  include <signal.h>
#  include <sys/wait.h>

extern char **environ;
#endif // _WIN32


//
// Command state/data...
//

typedef struct _pappl_command_s		// Per-process data for a command
{
  int			number;		// Command number
  pappl_system_t	*system;	// System
  pappl_printer_t	*printer;	// Printer, if any
  pappl_job_t		*job;		// Job, if any
  char			name[256];	// Command name
#if _WIN32
  HANDLE		phandle;	// Process handle
#else
  pid_t			pid;		// Process ID
#endif // _WIN32
  int			stderr_pipe;	// stderr pipe for messages
  char			buffer[8192];	// Message buffer
  size_t		bufused;	// Number of bytes used in buffer
} _pappl_command_t;


//
// Local functions...
//

static int	compare_commands(_pappl_command_t *a, _pappl_command_t *b, void *data);
static char	*read_line(_pappl_command_t *command, char *line, size_t linesize);
static void	stop_command(_pappl_command_t *command);
static void	*wait_command(_pappl_command_t *command);


//
// 'papplSystemAddExtCommandPath()' - Add a file or directory that can be executed.
//
// This function adds a file or directory that can be executed by external
// commands.
//
// > Note: This function can only be used when the system is not running.
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
// This function adds a file or directory that can be read by external commands.
//
// > Note: This function can only be used when the system is not running.
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
// This function adds a file or directory that can be read and written by
// external commands.
//
// > Note: This function can only be used when the system is not running.
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


//
// 'papplSystemRunExtCommand()' - Execute a program with restrictions.
//
// This function executes an external program with restrictions.
//
// The "args" parameter is a NULL-terminated array of strings corresponding to
// the "argv" passed to the external program.
//
// The "env" parameter is a NULL-terminated array of strings corresponding to
// the environment passed to the extern program. Each string is of the form
// "NAME=VALUE".
//

int					// O - Command number or `0` on error
papplSystemRunExtCommand(
    pappl_system_t  *system,		// I - System
    pappl_printer_t *printer,		// I - Printer or `NULL` for none
    pappl_job_t     *job,		// I - Job or `NULL` for none
    const char      **args,		// I - Command arguments
    const char      **env,		// I - Environment variables or `NULL` for default
    int             infd,		// I - Standard input file descriptor or `-1` for none
    int             outfd,		// I - Standard output file descriptor or `-1` for none
    bool            allow_networking)	// I - `true` to allow outgoing network connections, `false` otherwise
{
  cups_len_t		i;		// Looping var
  _pappl_command_t	*command;	// Command state/date
  const char		*name;		// Command name
  int			stderr_pipe[2];	// Pipe for stderr messages


  // Range check input...
  if (!system || (job && !printer) || !args)
  {
    errno = EINVAL;
    return (0);
  }

  // Create a pipe for stderr output from the command...
  if (!papplCreatePipe(stderr_pipe, true))
    return (0);

  // Get the base name of the command...
  if ((name = strrchr(args[0], '/')) != NULL)
    name ++;
  else if ((name = strrchr(args[0], '\\')) != NULL)
    name ++;
  else
    name = args[0];

  // Allocate and initialize command data...
  if ((command = calloc(1, sizeof(_pappl_command_t))) == NULL)
    goto error;

  command->system  = system;
  command->printer = printer;
  command->job     = job;

  cupsCopyString(command->name, name, sizeof(command->name));

  command->stderr_pipe = stderr_pipe[0];

#if _WIN32
  char		*cmdline,		// Command-line as a string
		*cmdptr;		// Pointer into command-line string
  size_t	length;			// Length of command-line string
  STARTUPINFOA	startinfo;		// Startup information
  PROCESS_INFORMATION pinfo;		// Process information


  // Make a command-line string
  for (i = 1, length = 1; args[i]; i ++)
    length += strlen(args[i]) + 3;

  if ((cmdline = malloc(length)) == NULL)
    goto error;

  for (i = 1, cmdptr = cmdline; args[i]; i ++)
  {
    if (cmdptr > cmdline)
      *cmdptr++ = ' ';

    if (strchr(args[i], ' '))
    {
      *cmdptr++ = '\"';
      cupsCopyString(cmdptr, args[i], length - (size_t)(cmdptr - cmdline));
      cmdptr += strlen(cmdptr);
      *cmdptr++ = '\"';
    }
    else
    {
      cupsCopyString(cmdptr, args[i], length - (size_t)(cmdptr - cmdline));
      cmdptr += strlen(cmdptr);
    }
  }
  *cmdptr = '\0';

  // Map stdin/out/err as needed...
  memset(&startinfo, 0, sizeof(startinfo));
  startinfo.cb      = sizeof(startinfo);
  startinfo.dwFlags = STARTF_USESTDHANDLES;

  if (infd >= 0)
    startinfo.hStdInput = (HANDLE)_get_osfhandle(infd);

  if (outfd >= 0)
    startinfo.hStdOutput = (HANDLE)_get_osfhandle(outfd);

  startinfo.hStdError = (HANDLE)_get_osfhandle(stderr_pipe[1]);

  // TODO: Map env to "environment block"...
  if (CreateProcessA(args[0], cmdline, /*lpProcessAttributes*/NULL, /*lpThreadAttributes*/NULL, /*bInheritHandles*/FALSE, CREATE_NO_WINDOW, /*lpEnvironment*/NULL, /*lpCurrentDirectory*/NULL, &startinfo, &pinfo))
  {
    free(cmdline);
    goto error;
  }

  free(cmdline);

  command->phandle = pinfo.hProcess;

#else
  cups_len_t		count;		// Number of values
  int			pargc = 0;	// Number of arguments to pappl-exec
  const char 		*pargv[1000];	// Arguments to pappl-exec
  posix_spawn_file_actions_t pactions;	// Actions for posix_spawn
  int			perr;		// Error from posix_spawn

  // Build the command-line...
  if ((pargv[0] = getenv("PAPPL_EXEC")) == NULL)
    pargv[0] = "pappl-exec";

  pargc ++;

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

    pargv[pargc++] = (char *)args[i];
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
    posix_spawn_file_actions_adddup2(&pactions, outfd, /*newfiledes*/1);

  // stderr
  posix_spawn_file_actions_adddup2(&pactions, stderr_pipe[1], /*newfiledes*/2);

  // Execute the command...
  if ((perr = posix_spawnp(&command->pid, (char *)pargv[0], &pactions, /*addrp*/NULL, (char **)pargv, env ? (char **)env : environ)) != 0)
  {
    errno = perr;
    posix_spawn_file_actions_destroy(&pactions);
    goto error;
  }

  posix_spawn_file_actions_destroy(&pactions);
#endif // _WIN32

  // Monitor the command for output...
  close(stderr_pipe[1]);

  cupsThreadCreate((cups_thread_func_t)wait_command, command);

  // Add the command to the system-wide
  cupsMutexLock(&system->ext_mutex);

  command->number = system->ext_next_number ++;

  if (!system->ext_commands)
    system->ext_commands = cupsArrayNew((cups_array_cb_t)compare_commands, /*data*/NULL, /*hash*/NULL, /*hsize*/0, /*copy_cb*/NULL, /*free_cb*/NULL);

  cupsArrayAdd(system->ext_commands, command);

  cupsMutexUnlock(&system->ext_mutex);

  if (job)
    papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "[%s] Started.", name);
  else if (printer)
    papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "[%s] Started.", name);
  else
    papplLog(system, PAPPL_LOGLEVEL_ERROR, "[%s] Started.", name);

  return (command->number);

  // If we get here, something bad happened...
  error:

  if (job)
    papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "[%s] Unable to start: %s", name, strerror(errno));
  else if (printer)
    papplLogPrinter(printer, PAPPL_LOGLEVEL_ERROR, "[%s] Unable to start: %s", name, strerror(errno));
  else
    papplLog(system, PAPPL_LOGLEVEL_ERROR, "[%s] Unable to start: %s", name, strerror(errno));

  close(stderr_pipe[0]);
  close(stderr_pipe[1]);
  free(command);

  return (0);
}


//
// 'papplSystemSetExtUserGroup()' - Set an alternate user and group for external programs.
//
// This function sets an alternate user and group to use when running external
// programs.  The user and group are only used when the printer application is
// running as the root user.
//
// > Note: This function can only be used when the system is not running.
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
// '_papplSystemStopAllExtCommands()' - Stop all external commands.
//

void
_papplSystemStopAllExtCommands(
    pappl_system_t *system)		// I - System
{
  _pappl_command_t	*command;	// Current command


  cupsMutexLock(&system->ext_mutex);
  for (command = (_pappl_command_t *)cupsArrayGetFirst(system->ext_commands); command; command = (_pappl_command_t *)cupsArrayGetNext(system->ext_commands))
    stop_command(command);
  cupsMutexUnlock(&system->ext_mutex);
}


//
// 'papplSystemStopExtCommand()' - Stop an external command.
//
// This function stops the specified command.  The "pid" argument is the
// integer returned by the @link papplSystemRunExtCommand@ function.
//

void
papplSystemStopExtCommand(
    pappl_system_t *system,		// I - System
    int            number)		// I - Command number
{
  _pappl_command_t *command,		// Matching command
		key;			// Search key


  cupsMutexLock(&system->ext_mutex);
  key.number = number;
  if ((command = (_pappl_command_t *)cupsArrayFind(system->ext_commands, &key)) != NULL)
    stop_command(command);
  cupsMutexUnlock(&system->ext_mutex);
}


//
// 'compare_commands()' - Compare two commands.
//

static int				// O - Result of comparison
compare_commands(_pappl_command_t *a,	// I - First command
                 _pappl_command_t *b,	// I - Second command
                 void             *data)// I - Callback data (unused)
{
  (void)data;

  return (b->number - a->number);
}


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
// 'stop_command()' - Stop a command.
//

static void
stop_command(_pappl_command_t *command)	// I - Command
{
#if _WIN32
  TerminateProcess(command->phandle, 255);
#else
  kill(command->pid, SIGTERM);
#endif // _WIN32
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
    else if (!strncmp(line, "ATTR:", 5))
    {
      // Ignore ATTR lines for now...
      continue;
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
#if _WIN32
  DWORD code;				// Exit code

  GetExitCodeProcess(command->phandle, &code);
  status = code;

#else
  while (waitpid(command->pid, &status, 0) < 0)
  {
    if (errno != EINTR)
    {
      status = -1;
      break;
    }
  }
#endif // _WIN32

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
#if _WIN32
  else if (status == 255)
  {
    cupsCopyString(line, "Terminated.", sizeof(line));
  }
  else
  {
    snprintf(line, sizeof(line), "Completed with status %d.", WEXITSTATUS(status));
  }
#else
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
#endif // _WIN32

  if (command->job)
    papplLogJob(command->job, level, "[%s] %s", command->name, line);
  else if (command->printer)
    papplLogPrinter(command->printer, level, "[%s] %s", command->name, line);
  else
    papplLog(command->system, level, "[%s] %s", command->name, line);

  // Close the stderr pipe and free memory used for data/state...
  close(command->stderr_pipe);

  cupsMutexLock(&command->system->ext_mutex);
  cupsArrayRemove(command->system->ext_commands, command);
  cupsMutexUnlock(&command->system->ext_mutex);

  free(command);

  // Exit the monitoring thread...
  return (NULL);
}
