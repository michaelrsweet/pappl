//
// External command helper for the Printer Application Framework
//
// Copyright © 2025 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//
// Usage:
//
//   pappl-exec [OPTIONS] PROGRAM ARGUMENT(S)
//
// Options:
//
//   --help                  Show program help.
//   --version               Show PAPPL version.
//   -N/--no-access PATH     Do not allow access to the specified file or directory.
//   -R/--read-only PATH     Allow read access to the specified file or directory.
//   -W/--read-write PATH    Allow read/write access to the specified file or directory.
//   -X/--read-exec PATH     Allow read/execute access to the specified file or directory.
//   -g/--group GROUP        Specify run group.
//   -n/--allow-networking   Allow outgoing network connections.
//   -u/--user USER          Specify run user.
//
// This program is used by PAPPL to run external commands such as "ipptransform"
// to do work outside the main executable.  On systems that support user-level
// security handlers, it also constructs a basic profile for the external
// command to run in with the following properties:
//
// - Read-only access to the filesystem as permitted by the current user.
// - No access to directories and files listed by the "-N" option, typically the
//   spool directory and state file.
// - Read-write access to TMPDIR and any directories or files listed by the "-W"
//   option.
// - Read-execute access to the directories and files listed by the "-X" option
//   and to the program.
// - Read-only access to the directories or files listed by the "-R" option and
//   any file listed in the program arguments.
// - Optional outgoing network socket support.
//
// We currently support landlock on Linux and sandbox on macOS.
//

#include "loc-private.h"
#if _WIN32
#else
#  include <spawn.h>
#  include <pwd.h>
#  include <grp.h>
#endif // _WIN32
#ifdef __APPLE__
#  include <sandbox.h>
#  ifndef SANDBOX_NAMED_EXTERNAL
#    define SANDBOX_NAMED_EXTERNAL  0x0003
#  endif /* !SANDBOX_NAMED_EXTERNAL */
#  pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif // __APPLE__
#ifdef HAVE_LINUX_LANDLOCK_H
#  include <linux/landlock.h>
#  include <sys/syscall.h>
#endif // HAVE_LINUX_LANDLOCK_H


//
// Local functions...
//

static void	load_profile(char **program_args, bool allow_networking, cups_array_t *no_access, cups_array_t *read_execute, cups_array_t *read_only, cups_array_t *read_write);
#ifdef __APPLE__
static bool	path_rule(cups_file_t *fp, const char *comment, const char *allow, const char *deny, const char *path, bool is_exec);
#endif // __APPLE__
static int	usage(FILE *fp);


//
// 'main()' - Main entry.
//

int					// O - Exit status
main(int  argc,				// I - Number of command-line arguments
     char *argv[])			// I - Command-line arguments
{
  int		i;			// Looping var
  const char	*opt;			// Current option letter
  char		**program_args = NULL;	// Program arguments
  cups_array_t	*no_access = NULL,	// Files and directories with no access
		*read_only = NULL,	// Files and directories with read access
		*read_write = NULL,	// Files and directories with read/write access
		*read_execute = NULL;	// Files and directories with read/execute access
  bool		allow_networking = false;
					// Allow network connections?
  const char	*group = NULL,		// Group for program
		*user = NULL;		// User account for program
#if !_WIN32
  gid_t		gid = 0;		// Group ID
  uid_t		uid = 0;		// User ID
#endif // !_WIN32


  // Parse the command-line...
  for (i = 1; i < argc; i ++)
  {
    if (!strcmp(argv[i], "--allow-networking"))
    {
      allow_networking = true;
    }
    else if (!strcmp(argv[i], "--group"))
    {
      // --group GROUP-NAME
      i ++;
      if (i >= argc)
      {
        _papplLocPrintf(stderr, _PAPPL_LOC("pappl-exec: Missing group name after '%s'."), "--group");
        return (usage(stderr));
      }

      group = argv[i];
    }
    else if (!strcmp(argv[i], "--help"))
    {
      return (usage(stdout));
    }
    else if (!strcmp(argv[i], "--no-access"))
    {
      // --no-access PATH
      i ++;
      if (i >= argc)
      {
        _papplLocPrintf(stderr, _PAPPL_LOC("pappl-exec: Missing path after '%s'."), "--no-access");
        return (usage(stderr));
      }

      if (!no_access)
        no_access = cupsArrayNew(NULL, NULL, NULL, 0, NULL, NULL);

      cupsArrayAdd(no_access, argv[i]);
    }
    else if (!strcmp(argv[i], "--read-exec"))
    {
      i ++;
      if (i >= argc)
      {
        _papplLocPrintf(stderr, _PAPPL_LOC("pappl-exec: Missing path after '%s'."), "--read-exec");
        return (usage(stderr));
      }

      if (!read_execute)
        read_execute = cupsArrayNew(NULL, NULL, NULL, 0, NULL, NULL);

      cupsArrayAdd(read_execute, argv[i]);
    }
    else if (!strcmp(argv[i], "--read-only"))
    {
      i ++;
      if (i >= argc)
      {
        _papplLocPrintf(stderr, _PAPPL_LOC("pappl-exec: Missing path after '%s'."), "--read-only");
        return (usage(stderr));
      }

      if (!read_only)
        read_only = cupsArrayNew(NULL, NULL, NULL, 0, NULL, NULL);

      cupsArrayAdd(read_only, argv[i]);
    }
    else if (!strcmp(argv[i], "--read-write"))
    {
      i ++;
      if (i >= argc)
      {
        _papplLocPrintf(stderr, _PAPPL_LOC("pappl-exec: Missing path after '%s'."), "--read-write");
        return (usage(stderr));
      }

      if (!read_write)
        read_write = cupsArrayNew(NULL, NULL, NULL, 0, NULL, NULL);

      cupsArrayAdd(read_write, argv[i]);
    }
    else if (!strcmp(argv[i], "--user"))
    {
      i ++;
      if (i >= argc)
      {
        _papplLocPrintf(stderr, _PAPPL_LOC("pappl-exec: Missing user name after '%s'."), "--user");
        return (usage(stderr));
      }

      user = argv[i];
    }
    else if (!strcmp(argv[i], "--version"))
    {
      puts(PAPPL_VERSION);
      return (0);
    }
    else if (!strncmp(argv[i], "--", 2))
    {
      _papplLocPrintf(stderr, _PAPPL_LOC("pappl-exec: Unknown option '%s'."), argv[i]);
      return (usage(stderr));
    }
    else if (argv[i][0] == '-')
    {
      // Parse single-letter options...
      for (opt = argv[i] + 1; *opt; opt ++)
      {
        switch (*opt)
        {
          case 'N' : // -N PATH
	      i ++;
	      if (i >= argc)
	      {
		_papplLocPrintf(stderr, _PAPPL_LOC("pappl-exec: Missing path after '%s'."), "-N");
		return (usage(stderr));
	      }

	      if (!no_access)
		no_access = cupsArrayNew(NULL, NULL, NULL, 0, NULL, NULL);

	      cupsArrayAdd(no_access, argv[i]);
              break;
          case 'R' : // -R PATH
	      i ++;
	      if (i >= argc)
	      {
		_papplLocPrintf(stderr, _PAPPL_LOC("pappl-exec: Missing path after '%s'."), "-R");
		return (usage(stderr));
	      }

	      if (!read_only)
		read_only = cupsArrayNew(NULL, NULL, NULL, 0, NULL, NULL);

	      cupsArrayAdd(read_only, argv[i]);
              break;
          case 'W' : // -W PATH
	      i ++;
	      if (i >= argc)
	      {
		_papplLocPrintf(stderr, _PAPPL_LOC("pappl-exec: Missing path after '%s'."), "-W");
		return (usage(stderr));
	      }

	      if (!read_write)
		read_write = cupsArrayNew(NULL, NULL, NULL, 0, NULL, NULL);

	      cupsArrayAdd(read_write, argv[i]);
              break;
          case 'X' : // -X PATH
	      i ++;
	      if (i >= argc)
	      {
		_papplLocPrintf(stderr, _PAPPL_LOC("pappl-exec: Missing path after '%s'."), "-X");
		return (usage(stderr));
	      }

	      if (!read_execute)
		read_execute = cupsArrayNew(NULL, NULL, NULL, 0, NULL, NULL);

	      cupsArrayAdd(read_execute, argv[i]);
              break;
          case 'g' : // -g GROUP
	      i ++;
	      if (i >= argc)
	      {
		_papplLocPrintf(stderr, _PAPPL_LOC("pappl-exec: Missing group name after '%s'."), "-g");
		return (usage(stderr));
	      }

	      group = argv[i];
              break;
          case 'n' : // -n
              allow_networking = true;
              break;
          case 'u' : // -u USER
	      i ++;
	      if (i >= argc)
	      {
		_papplLocPrintf(stderr, _PAPPL_LOC("pappl-exec: Missing user name after '%s'."), "-U");
		return (usage(stderr));
	      }

	      user = argv[i];
              break;
          default :
              _papplLocPrintf(stderr, _PAPPL_LOC("pappl-exec: Unknown option '-%c'."), *opt);
              return (usage(stderr));
        }
      }
    }
    else
    {
      // Got to the program arguments...
      program_args = argv + i;
      break;
    }
  }

  // Make sure we have a program to run...
  if (!program_args)
  {
    _papplLocPrintf(stderr, _PAPPL_LOC("pappl-exec: No program or arguments specified."));
    return (usage(stderr));
  }

#if !_WIN32
  // Validate any user or group
  if (user)
  {
    struct passwd *pw;			// User account information

    if (isdigit(*user & 255))
    {
      // Numeric UID
      char *ptr;			// Pointer into UID
      long val = strtol(user, &ptr, 10);// UID value

      if (val < 0 || *ptr)
      {
	_papplLocPrintf(stderr, _PAPPL_LOC("pappl-exec: Invalid user ID '%s'."), user);
	return (1);
      }

      uid = (uid_t)val;
    }
    else if ((pw = getpwnam(user)) == NULL)
    {
      // Named user not found...
      _papplLocPrintf(stderr, _PAPPL_LOC("pappl-exec: User '%s' not found."), user);
      return (1);
    }
    else
    {
      // Use the UID and GID for the named user...
      uid = pw->pw_uid;
      gid = pw->pw_gid;
    }
  }

  if (group)
  {
    struct group *grp;			// Group information

    if (isdigit(*group & 255))
    {
      // Numeric GID
      char *ptr;			// Pointer into GID
      long val = strtol(group, &ptr, 10);// UID value

      if (val < 0 || *ptr)
      {
	_papplLocPrintf(stderr, _PAPPL_LOC("pappl-exec: Invalid group ID '%s'."), group);
	return (1);
      }

      gid = (gid_t)val;
    }
    else if ((grp = getgrnam(group)) == NULL)
    {
      // Group not found...
      _papplLocPrintf(stderr, _PAPPL_LOC("pappl-exec: Group '%s' not found."), user);
      return (1);
    }
    else
    {
      // Use the GID for the named group...
      gid = grp->gr_gid;
    }
  }
#endif // !_WIN32

  // Load any restrictions...
  load_profile(program_args, allow_networking, no_access, read_execute, read_only, read_write);

#if !_WIN32
  // Change user/group as needed...
  if (uid)
    setuid(uid);

  if (gid)
    setgid(uid);
#endif // !_WIN32

  // Execute the program...
  execvp(program_args[0], program_args);

  // If we get here then there was an error...
  _papplLocPrintf(stderr, _PAPPL_LOC("pappl-exec: Unable to execute '%s': %s"), program_args[0], strerror(errno));

  cupsArrayDelete(no_access);
  cupsArrayDelete(read_execute);
  cupsArrayDelete(read_only);
  cupsArrayDelete(read_write);

  return (1);
}


//
// 'load_profile()' - Load restrictions for the program we are executing.
//

static void
load_profile(
    char         **program_args,	// I - Program arguments
    bool         allow_networking,	// I - Allow networking?
    cups_array_t *no_access,		// I - List of files/directories that cannot be accessed
    cups_array_t *read_execute,		// I - List of files/directories that can be read and executed
    cups_array_t *read_only,		// I - List of files/directories that can be read
    cups_array_t *read_write)		// I - List of files/directories that can be read and written
{
#ifdef __APPLE__
  int		i;			// Looping var
  char		profile[PATH_MAX];	// Sandbox profile file
  cups_file_t	*fp;			// File pointer
  const char	*tmpdir,		// TMPDIR environment variable
		*path;			// Current pathname
  char		*error;			// Sandbox error, if any


  // Create a profile...
  if ((fp = cupsCreateTempFile(/*prefix*/NULL, ".sb", profile, sizeof(profile))) == NULL)
  {
    _papplLocPrintf(stderr, _PAPPL_LOC("pappl-exec: Unable to create sandbox profile: %s"), cupsGetErrorString());
    exit(1);
  }

  // The basics...
  cupsFilePuts(fp, ";; Sandbox profile generated by pappl-exec\n");
  cupsFilePuts(fp, "(version 1)\n");
  cupsFilePuts(fp, "(allow default)\n");
  cupsFilePuts(fp, "(debug deny)\n");
  if (!path_rule(fp, /*comment*/NULL, /*allow*/NULL, "file-write*", "/", /*is_exec*/false))
    goto fail;

  // Allow networking?
  cupsFilePuts(fp, "(deny network*)\n");
  if (allow_networking)
  {
    cupsFilePuts(fp, ";; --allow-networking\n");
    cupsFilePuts(fp, "(allow network-inbound)\n");
    cupsFilePuts(fp, "(allow network-outbound)\n");
  }

  // No access file/path list...
  for (path = (const char *)cupsArrayGetFirst(no_access); path; path = (const char *)cupsArrayGetNext(no_access))
  {
    if (!path_rule(fp, "--no-access", /*allow*/NULL, "file-read-data file-read-metadata file-write*", path, /*is_exec*/false))
      goto fail;
  }

  // Read-only file/path list...
  for (path = (const char *)cupsArrayGetFirst(read_only); path; path = (const char *)cupsArrayGetNext(read_only))
  {
    if (!path_rule(fp, "--read-only", "file-read-data file-read-metadata", "file-write*", path, /*is_exec*/false))
      goto fail;
  }

  for (i = 1; program_args[i]; i ++)
  {
    // See if this argument is a file or directory...
    if (access(program_args[i], 0))
      continue;

    // Add the rule...
    if (!path_rule(fp, "--read-only (ARGUMENT)", "file-read-data file-read-metadata", "file-write*", program_args[i], /*is_exec*/false))
      goto fail;
  }

  // Read-write file/path list...
  for (path = (const char *)cupsArrayGetFirst(read_write); path; path = (const char *)cupsArrayGetNext(read_write))
  {
    if (!path_rule(fp, "--read-write", "file-read-data file-read-metadata file-write*", /*deny*/NULL, path, /*is_exec*/false))
      goto fail;
  }

  // Allow read/write to the temporary directory...
  if ((tmpdir = getenv("TMPDIR")) == NULL)
    tmpdir = "/private/tmp";

  if (!path_rule(fp, "--read-write (TMPDIR)", "file-read-data file-read-metadata file-write*", /*deny*/NULL, tmpdir, /*is_exec*/false))
    goto fail;

  // Read-execute file/path list...
  if (cupsArrayGetCount(read_execute) > 0)
  {
    // Disallow executing anything...
    cupsFilePuts(fp, "(allow process*)\n");
    cupsFilePuts(fp, "(deny process-exec (regex #\"^/\"))\n");

    // Allow read/execute for program...
    if (!path_rule(fp, "--read-execute (PROGRAM)", "file-read-data file-read-metadata process-exec", "file-write*", program_args[0], /*is_exec*/true))
      goto fail;
  }

  for (path = (const char *)cupsArrayGetFirst(read_execute); path; path = (const char *)cupsArrayGetNext(read_execute))
  {
    if (!path_rule(fp, "--read-execute", "file-read-data file-read-metadata process-exec", "file-write*", path, /*is_exec*/true))
      goto fail;
  }

  // Finally make sure all of the normal macOS system stuff can be done...
  cupsFilePuts(fp, "(import \"system.sb\")\n");
  cupsFileClose(fp);

  // Apply the sandbox profile...
  if (sandbox_init(profile, SANDBOX_NAMED_EXTERNAL, &error) < 0)
  {
    _papplLocPrintf(stderr, _PAPPL_LOC("pappl-exec: Unable to apply sandbox profile '%s': %s"), profile, error);
    exit(1);
  }

//  fprintf(stderr, "profile='%s'\n", profile);
  unlink(profile);
  return;

  // If we get here we had a problem...
  fail:

  cupsFileClose(fp);
  unlink(profile);
  exit(1);

#elif defined(HAVE_LINUX_LANDLOCK_H)
  (void)program_args;
  (void)allow_networking;
  (void)no_access;
  (void)read_execute;
  (void)read_only;
  (void)read_write;

#else // No sandboxing
  (void)program_args;
  (void)allow_networking;
  (void)no_access;
  (void)read_execute;
  (void)read_only;
  (void)read_write;
#endif // __APPLE__
}


#ifdef __APPLE__
//
// 'regex_quote()' - Make a regular-expression version of a string.
//


static bool				// O - `true` on success, `false` on failure
path_rule(cups_file_t *fp,		// I - Profile file
          const char  *comment,		// I - Comment title for file
          const char  *allow,		// I - Allow operations, if any
          const char  *deny,		// I - Deny operations, if any
          const char  *path,		// I - File or directory path
          bool        is_exec)		// I - Is the path for an executable?
{
  char		abspath[PATH_MAX],	// Absolute path
		*absptr,		// Pointer into absolute path
		repath[PATH_MAX * 2],	// Regular expression of path
		*reptr,			// Pointer into regular expression
		*reend;			// Pointer to end of regular expression buffer
  struct stat	pathinfo;		// Path information


  // Convert path to absolute...
  if (is_exec && !strchr(path, '/'))
  {
    // Look up the executable in the PATH...
    if (!cupsFileFind(path, getenv("PATH"), /*executable*/1, abspath, sizeof(abspath)))
    {
      _papplLocPrintf(stderr, _PAPPL_LOC("pappl-exec: Unable to find program '%s' for sandbox profile."), path);
      return (false);
    }
  }
  else if (!realpath(path, abspath))
  {
    _papplLocPrintf(stderr, _PAPPL_LOC("pappl-exec: Unable to access '%s' for sandbox profile: %s"), path, strerror(errno));
    return (false);
  }

  // See if this is actually a file...
  if (stat(abspath, &pathinfo))
  {
    _papplLocPrintf(stderr, _PAPPL_LOC("pappl-exec: Unable to access '%s' for sandbox profile: %s"), path, strerror(errno));
    return (false);
  }

  // Convert the absolute path into a regular expression...
  reptr = repath;
  reend = repath + sizeof(repath) - 2;

  for (absptr = abspath; *absptr && reptr < reend; absptr ++)
  {
    if (absptr[0] == '/' && !absptr[1])
      break;				// Don't add a trailing slash

    // Escape any special regular expression characters...
    if (strchr(".?*()[]^$\\\"", *absptr))
      *reptr++ = '\\';

    *reptr++ = *absptr;
  }

  *reptr = '\0';

  // Add the rule(s)...
  if (comment)
    cupsFilePrintf(fp, ";; %s %s\n", comment, path);
  if (S_ISDIR(pathinfo.st_mode))
  {
    // Allow/deny access to directory and its children...
    if (allow)
    {
      cupsFilePrintf(fp, "(allow %s (regex #\"^%s$\"))\n", allow, repath);
      cupsFilePrintf(fp, "(allow %s (regex #\"^%s/\"))\n", allow, repath);
    }

    if (deny)
    {
      cupsFilePrintf(fp, "(deny %s (regex #\"^%s$\"))\n", deny, repath);
      cupsFilePrintf(fp, "(deny %s (regex #\"^%s/\"))\n", deny, repath);
    }
  }
  else
  {
    // Allow/deny access to file...
    if (allow)
      cupsFilePrintf(fp, "(allow %s (regex #\"^%s$\"))\n", allow, repath);
    if (deny)
      cupsFilePrintf(fp, "(deny %s (regex #\"^%s$\"))\n", deny, repath);
  }

  return (true);
}
#endif // __APPLE__


//
// 'usage()' - Show program usage.
//

static int				// O - Exit status
usage(FILE *fp)				// I - Output file
{
  _papplLocPrintf(fp, _PAPPL_LOC("Usage: pappl-exec [OPTIONS] PROGRAM ARGUMENT(S)\n"));
  puts("");
  _papplLocPrintf(fp, _PAPPL_LOC("Options:"));
  _papplLocPrintf(fp, _PAPPL_LOC("   --help                  Show program help."));
  _papplLocPrintf(fp, _PAPPL_LOC("   --version               Show PAPPL version."));
  _papplLocPrintf(fp, _PAPPL_LOC("   -N/--no-access PATH     Do not allow access to the specified file or directory."));
  _papplLocPrintf(fp, _PAPPL_LOC("   -R/--read-only PATH     Allow read access to the specified file or directory."));
  _papplLocPrintf(fp, _PAPPL_LOC("   -W/--read-write PATH    Allow read/write access to the specified file or directory."));
  _papplLocPrintf(fp, _PAPPL_LOC("   -X/--read-exec PATH     Allow read/execute access to the specified file or directory."));
  _papplLocPrintf(fp, _PAPPL_LOC("   -g/--group GROUP        Specify run group."));
  _papplLocPrintf(fp, _PAPPL_LOC("   -n/--allow-networking   Allow outgoing network connections."));
  _papplLocPrintf(fp, _PAPPL_LOC("   -u/--user USER          Specify run user."));

  return (fp == stderr ? 1 : 0);
}
