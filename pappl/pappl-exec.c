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
// - Read-only access to the core OS filesystem.
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
#  ifndef LANDLOCK_ACCESS_FS_TRUNCATE
#    define LANDLOCK_ACCESS_FS_TRUNCATE 0
#  endif // !LANDLOCK_ACCESS_FS_TRUNCATE
#  include <sys/prctl.h>
#  include <sys/syscall.h>
#  define landlock_add_rule(fd,type,attr,flags)		syscall(__NR_landlock_add_rule, fd, type, attr, flags)
#  define landlock_create_ruleset(attr,size,flags)	syscall(__NR_landlock_create_ruleset, attr, size, flags)
#  define landlock_restrict_self(fd,flags)		syscall(__NR_landlock_restrict_self, fd, flags)
#endif // HAVE_LINUX_LANDLOCK_H


//
// Local functions...
//

static void	load_profile(char **program_args, bool allow_networking, cups_array_t *read_exec, cups_array_t *read_only, cups_array_t *read_write);
#ifdef __APPLE__
static bool	path_rule(cups_file_t *fp, const char *comment, const char *allow, const char *deny, const char *path, bool is_exec);
#elif defined(HAVE_LINUX_LANDLOCK_H)
static bool	path_rule(int ruleset_fd, __u64 flags, const char *path, bool is_exec);
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
  cups_array_t	*read_only = NULL,	// Files and directories with read access
		*read_write = NULL,	// Files and directories with read/write access
		*read_exec = NULL;	// Files and directories with read/execute access
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
    else if (!strcmp(argv[i], "--read-exec"))
    {
      i ++;
      if (i >= argc)
      {
        _papplLocPrintf(stderr, _PAPPL_LOC("pappl-exec: Missing path after '%s'."), "--read-exec");
        return (usage(stderr));
      }

      if (!read_exec)
        read_exec = cupsArrayNew(NULL, NULL, NULL, 0, NULL, NULL);

      cupsArrayAdd(read_exec, argv[i]);
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

	      if (!read_exec)
		read_exec = cupsArrayNew(NULL, NULL, NULL, 0, NULL, NULL);

	      cupsArrayAdd(read_exec, argv[i]);
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
  load_profile(program_args, allow_networking, read_exec, read_only, read_write);

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

  cupsArrayDelete(read_exec);
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
    cups_array_t *read_exec,		// I - List of files/directories that can be read and executed
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
  cupsFilePuts(fp, "(deny default)\n");
  cupsFilePuts(fp, "(debug deny)\n");
  cupsFilePuts(fp, "(allow ipc* mach* process-fork)\n");
  cupsFilePuts(fp, "(deny network*)\n");
  cupsFilePuts(fp, "(allow network-inbound network-outbound (regex #\"^/private/var/run/\" #\"^/var/run/\"))\n");

  // Allow TCP/UDP networking?
  if (allow_networking)
  {
    cupsFilePuts(fp, ";; --allow-networking\n");
    cupsFilePuts(fp, "(allow network-inbound)\n");
    cupsFilePuts(fp, "(allow network-outbound)\n");
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
    if (program_args[i][0] != '/' || access(program_args[i], 0))
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
  for (path = (const char *)cupsArrayGetFirst(read_exec); path; path = (const char *)cupsArrayGetNext(read_exec))
  {
    if (!path_rule(fp, "--read-exec", "file-read-data file-read-metadata process-exec", "file-write*", path, /*is_exec*/true))
      goto fail;
  }

  // Allow read/execute for program...
  if (!path_rule(fp, "--read-exec (PROGRAM)", "file-read-data file-read-metadata process-exec", "file-write*", program_args[0], /*is_exec*/true))
    goto fail;

  // Finally make sure all of the normal macOS system stuff can be done...
  cupsFilePuts(fp, "(import \"system.sb\")\n");
  cupsFilePuts(fp, "(import \"com.apple.corefoundation.sb\")\n");

  if (!path_rule(fp, /*comment*/NULL, "file-read-data file-read-metadata", /*deny*/NULL, "/Library", /*is_exec*/true))
    goto fail;

  if (!path_rule(fp, /*comment*/NULL, "file-read-data file-read-metadata", /*deny*/NULL, "/System/Library", /*is_exec*/true))
    goto fail;

  if (!path_rule(fp, /*comment*/NULL, "file-read-data file-read-metadata process-exec", /*deny*/NULL, "/bin", /*is_exec*/true))
    goto fail;

  if (!path_rule(fp, /*comment*/NULL, "file-read-data file-read-metadata", /*deny*/NULL, "/private", /*is_exec*/true))
    goto fail;

  if (!path_rule(fp, /*comment*/NULL, "file-read-data file-read-metadata process-exec", /*deny*/NULL, "/sbin", /*is_exec*/true))
    goto fail;

  if (!path_rule(fp, /*comment*/NULL, "file-read-data file-read-metadata process-exec", /*deny*/NULL, "/usr", /*is_exec*/true))
    goto fail;

  cupsFileClose(fp);

  // Apply the sandbox profile...
  if (sandbox_init(profile, SANDBOX_NAMED_EXTERNAL, &error) < 0)
  {
    _papplLocPrintf(stderr, _PAPPL_LOC("pappl-exec: Unable to apply sandbox profile '%s': %s"), profile, error);
    exit(1);
  }

#  ifdef DEBUG
  fprintf(stderr, "DEBUG: profile='%s'\n", profile);
#  else
  unlink(profile);
#  endif // DEBUG

  return;

  // If we get here we had a problem...
  fail:

  cupsFileClose(fp);
  unlink(profile);
  exit(1);

#elif defined(HAVE_LINUX_LANDLOCK_H)
  int		i,			// Looping var
		abi,			// Landlock ABI version
		ruleset_fd;		// Ruleset file descriptor
  struct landlock_ruleset_attr attr;	// Ruleset attributes
  const char	*tmpdir,		// TMPDIR environment variable
		*path;			// Current pathname


  // See what version of Landlock we have available...
  if ((abi = landlock_create_ruleset(/*attr*/NULL, /*size*/0, LANDLOCK_CREATE_RULESET_VERSION)) < 0)
  {
    _papplLocPrintf(stderr, _PAPPL_LOC("pappl-exec: Landlock does not appear to be supported by the running kernel."));
    return;
  }

  // Create the base ruleset...
  memset(&attr, 0, sizeof(attr));

  attr.handled_access_fs = LANDLOCK_ACCESS_FS_EXECUTE | LANDLOCK_ACCESS_FS_WRITE_FILE | LANDLOCK_ACCESS_FS_READ_FILE | LANDLOCK_ACCESS_FS_TRUNCATE | LANDLOCK_ACCESS_FS_READ_DIR | LANDLOCK_ACCESS_FS_REMOVE_DIR | LANDLOCK_ACCESS_FS_REMOVE_FILE | LANDLOCK_ACCESS_FS_MAKE_DIR | LANDLOCK_ACCESS_FS_MAKE_REG | LANDLOCK_ACCESS_FS_MAKE_SYM;

#ifdef LANDLOCK_ACCESS_NET_CONNECT_TCP
  if (abi >= 4 && !allow_networking)
    attr.handled_access_net = LANDLOCK_ACCESS_NET_CONNECT_TCP;
#endif // LANDLOCK_ACCESS_NET_CONNECT_TCP

  if ((ruleset_fd = landlock_create_ruleset(&attr, sizeof(attr), /*flags*/0)) < 0)
  {
    if (errno == EOPNOTSUPP)
    {
      _papplLocPrintf(stderr, _PAPPL_LOC("pappl-exec: Landlock does not appear to be supported by the running kernel."));
      return;
    }

    _papplLocPrintf(stderr, _PAPPL_LOC("pappl-exec: Unable to create landlock rule set: %s"), strerror(errno));
    exit(1);
  }

  // Read-only file/path list...
  for (path = (const char *)cupsArrayGetFirst(read_only); path; path = (const char *)cupsArrayGetNext(read_only))
  {
    if (!path_rule(ruleset_fd, LANDLOCK_ACCESS_FS_READ_FILE | LANDLOCK_ACCESS_FS_READ_DIR, path, /*is_exec*/false))
      goto fail;
  }

  if (!path_rule(ruleset_fd, LANDLOCK_ACCESS_FS_READ_FILE | LANDLOCK_ACCESS_FS_READ_DIR, "/dev", /*is_exec*/false))
    goto fail;

  if (!path_rule(ruleset_fd, LANDLOCK_ACCESS_FS_READ_FILE | LANDLOCK_ACCESS_FS_READ_DIR, "/etc", /*is_exec*/false))
    goto fail;

  if (!path_rule(ruleset_fd, LANDLOCK_ACCESS_FS_READ_FILE | LANDLOCK_ACCESS_FS_READ_DIR, "/proc", /*is_exec*/false))
    goto fail;

  if (!path_rule(ruleset_fd, LANDLOCK_ACCESS_FS_READ_FILE | LANDLOCK_ACCESS_FS_READ_DIR, "/run", /*is_exec*/false))
    goto fail;

  if (!path_rule(ruleset_fd, LANDLOCK_ACCESS_FS_READ_FILE | LANDLOCK_ACCESS_FS_READ_DIR, "/var", /*is_exec*/false))
    goto fail;

  for (i = 1; program_args[i]; i ++)
  {
    // See if this argument is a file or directory...
    if (program_args[i][0] != '/' || access(program_args[i], 0))
      continue;

    // Add the rule...
    if (!path_rule(ruleset_fd, LANDLOCK_ACCESS_FS_READ_FILE | LANDLOCK_ACCESS_FS_READ_DIR, program_args[i], /*is_exec*/false))
      goto fail;
  }

  // Read-write file/path list...
  for (path = (const char *)cupsArrayGetFirst(read_write); path; path = (const char *)cupsArrayGetNext(read_write))
  {
    if (!path_rule(ruleset_fd, LANDLOCK_ACCESS_FS_READ_FILE | LANDLOCK_ACCESS_FS_WRITE_FILE | LANDLOCK_ACCESS_FS_TRUNCATE | LANDLOCK_ACCESS_FS_READ_DIR | LANDLOCK_ACCESS_FS_REMOVE_DIR | LANDLOCK_ACCESS_FS_REMOVE_FILE | LANDLOCK_ACCESS_FS_MAKE_DIR | LANDLOCK_ACCESS_FS_MAKE_REG | LANDLOCK_ACCESS_FS_MAKE_SYM, path, /*is_exec*/false))
      goto fail;
  }

  // Allow read/write to the temporary directory...
  if ((tmpdir = getenv("TMPDIR")) == NULL)
    tmpdir = "/tmp";

  if (!path_rule(ruleset_fd, LANDLOCK_ACCESS_FS_READ_FILE | LANDLOCK_ACCESS_FS_WRITE_FILE | LANDLOCK_ACCESS_FS_TRUNCATE | LANDLOCK_ACCESS_FS_READ_DIR | LANDLOCK_ACCESS_FS_REMOVE_DIR | LANDLOCK_ACCESS_FS_REMOVE_FILE | LANDLOCK_ACCESS_FS_MAKE_DIR | LANDLOCK_ACCESS_FS_MAKE_REG | LANDLOCK_ACCESS_FS_MAKE_SYM, tmpdir, /*is_exec*/false))
    goto fail;

  // Read-execute file/path list...
  if (!path_rule(ruleset_fd, LANDLOCK_ACCESS_FS_EXECUTE | LANDLOCK_ACCESS_FS_READ_FILE | LANDLOCK_ACCESS_FS_READ_DIR, program_args[0], /*is_exec*/true))
    goto fail;

  for (path = (const char *)cupsArrayGetFirst(read_exec); path; path = (const char *)cupsArrayGetNext(read_exec))
  {
    if (!path_rule(ruleset_fd, LANDLOCK_ACCESS_FS_EXECUTE | LANDLOCK_ACCESS_FS_READ_FILE | LANDLOCK_ACCESS_FS_READ_DIR, path, /*is_exec*/true))
      goto fail;
  }

  if (!path_rule(ruleset_fd, LANDLOCK_ACCESS_FS_EXECUTE | LANDLOCK_ACCESS_FS_READ_FILE | LANDLOCK_ACCESS_FS_READ_DIR, "/bin", /*is_exec*/true))
    goto fail;

  if (!path_rule(ruleset_fd, LANDLOCK_ACCESS_FS_EXECUTE | LANDLOCK_ACCESS_FS_READ_FILE | LANDLOCK_ACCESS_FS_READ_DIR, "/lib", /*is_exec*/true))
    goto fail;

  if (!path_rule(ruleset_fd, LANDLOCK_ACCESS_FS_EXECUTE | LANDLOCK_ACCESS_FS_READ_FILE | LANDLOCK_ACCESS_FS_READ_DIR, "/opt", /*is_exec*/true))
    goto fail;

  if (!path_rule(ruleset_fd, LANDLOCK_ACCESS_FS_EXECUTE | LANDLOCK_ACCESS_FS_READ_FILE | LANDLOCK_ACCESS_FS_READ_DIR, "/sbin", /*is_exec*/true))
    goto fail;

  if (!path_rule(ruleset_fd, LANDLOCK_ACCESS_FS_EXECUTE | LANDLOCK_ACCESS_FS_READ_FILE | LANDLOCK_ACCESS_FS_READ_DIR, "/snap", /*is_exec*/true))
    goto fail;

  if (!path_rule(ruleset_fd, LANDLOCK_ACCESS_FS_EXECUTE | LANDLOCK_ACCESS_FS_READ_FILE | LANDLOCK_ACCESS_FS_READ_DIR, "/usr", /*is_exec*/true))
    goto fail;

  // Apply the ruleset...
  if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) < 0)
  {
    _papplLocPrintf(stderr, _PAPPL_LOC("pappl-exec: Unable to prepare landlock rule set: %s"), strerror(errno));
    exit(1);
  }

  if (landlock_restrict_self(ruleset_fd, /*flags*/0) < 0)
  {
    _papplLocPrintf(stderr, _PAPPL_LOC("pappl-exec: Unable to apply landlock rule set: %s"), strerror(errno));
    exit(1);
  }

  close(ruleset_fd);
  return;

  // If we get here we had a problem...
  fail:

  close(ruleset_fd);
  exit(1);

#else // No sandboxing
  (void)program_args;
  (void)allow_networking;
  (void)read_exec;
  (void)read_only;
  (void)read_write;
#endif // __APPLE__
}


#ifdef __APPLE__
//
// 'path_rule()' - Add a path-based rule to a sandbox profile.
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


#elif defined(HAVE_LINUX_LANDLOCK_H)
//
// 'path_rule()' - Add a path rule to a ruleset.
//

static bool				// O - `true` on success, `false` on failure
path_rule(int ruleset_fd,		// I - Rule set file descriptor
          __u64       flags,		// I - Filesystem flags
          const char  *path,		// I - File or directory path
          bool        is_exec)		// I - Is the path for an executable?
{
  char		abspath[PATH_MAX];	// Absolute path of program
  struct stat	pathinfo;		// Path information
  int		fd;			// File descriptor for file/directory
  struct landlock_path_beneath_attr attr;
					// File access attributes
#  ifdef DEBUG
  int		bit;			// Current bit
  static const char *bits[] =		// Strings for bits
  {
    "EXECUTE",
    "WRITE_FILE",
    "READ_FILE",
    "READ_DIR",
    "REMOVE_DIR",
    "REMOVE_FILE",
    "MAKE_CHAR",
    "MAKE_DIR",
    "MAKE_REG",
    "MAKE_SOCK",
    "MAKE_FIFO",
    "MAKE_BLOCK",
    "MAKE_SYM",
    "REFER",
    "TRUNCATE"
  };
#  endif // DEBUG


  // Convert path to absolute...
  if (is_exec && !strchr(path, '/'))
  {
    // Look up the executable in the PATH...
    if (!cupsFileFind(path, getenv("PATH"), /*executable*/1, abspath, sizeof(abspath)))
    {
      _papplLocPrintf(stderr, _PAPPL_LOC("pappl-exec: Unable to find program '%s' for sandbox profile."), path);
      return (false);
    }

    path = abspath;
  }

  // See if this is a file or directory...
  if ((fd = open(path, O_PATH | O_CLOEXEC)) < 0)
  {
    _papplLocPrintf(stderr, _PAPPL_LOC("pappl-exec: Unable to access '%s' for rule: %s"), path, strerror(errno));
    return (false);
  }

  if (fstat(fd, &pathinfo))
  {
    _papplLocPrintf(stderr, _PAPPL_LOC("pappl-exec: Unable to access '%s' for rule: %s"), path, strerror(errno));
    close(fd);
    return (false);
  }

  if (S_ISDIR(pathinfo.st_mode))
    attr.allowed_access = flags;
  else
    attr.allowed_access = flags & (LANDLOCK_ACCESS_FS_READ_FILE | LANDLOCK_ACCESS_FS_WRITE_FILE | LANDLOCK_ACCESS_FS_TRUNCATE | LANDLOCK_ACCESS_FS_EXECUTE);
  attr.parent_fd = fd;

  if (landlock_add_rule(ruleset_fd, LANDLOCK_RULE_PATH_BENEATH, &attr, 0) < 0)
  {
    _papplLocPrintf(stderr, _PAPPL_LOC("pappl-exec: Unable to add '%s' to rule set: %s"), path, strerror(errno));
    close(fd);
    return (false);
  }

#  ifdef DEBUG
  fprintf(stderr, "DEBUG: Added path rule '%s'", path);
  for (bit = 0; bit < (int)(sizeof(bits) / sizeof(bits[0])); bit ++)
  {
    if (attr.allowed_access & (1 << bit))
      fprintf(stderr, " %s", bits[bit]);
  }
  fputs("\n", stderr);
#  endif // DEBUG

  close(fd);

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
  _papplLocPrintf(fp, _PAPPL_LOC("   -R/--read-only PATH     Allow read access to the specified file or directory."));
  _papplLocPrintf(fp, _PAPPL_LOC("   -W/--read-write PATH    Allow read/write access to the specified file or directory."));
  _papplLocPrintf(fp, _PAPPL_LOC("   -X/--read-exec PATH     Allow read/execute access to the specified file or directory."));
  _papplLocPrintf(fp, _PAPPL_LOC("   -g/--group GROUP        Specify run group."));
  _papplLocPrintf(fp, _PAPPL_LOC("   -n/--allow-networking   Allow outgoing network connections."));
  _papplLocPrintf(fp, _PAPPL_LOC("   -u/--user USER          Specify run user."));

  return (fp == stderr ? 1 : 0);
}
