//
// Utility to parse the PAPPL rwlock logs.
//
// Usage:
//
//   ./parse-lock-log FILENAME.log
//
// Copyright © 2022 by Michael R Sweet
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>


// Uncomment the following to see verbose output...
//#define SHOW_BLOCKING_LOCKS 1


//
// Limits...
//

#define MAX_THREADS	1000
#define MAX_OBJS	10000


//
// Types/structures...
//

typedef unsigned long long log_addr_t;	// Thread/object address

typedef struct log_obj_s		// Object information
{
  log_addr_t		address;	// Object address
  char			name[256];	// Object name
  int			num_wr_threads;	// Number of writer threads, if any
  log_addr_t		wr_threads[MAX_THREADS];
					// Writer threads
  char			wr_functions[MAX_THREADS][32];
					// Writer functions
  int			num_rd_threads;	// Number of reader threads, if any
  log_addr_t		rd_threads[MAX_THREADS];
					// Reader threads
  char			rd_functions[MAX_THREADS][32];
} log_obj_t;


//
// 'main()' - Main entry.
//

int					// O - Exit status
main(int  argc,				// I - Number of command-line arguments
     char *argv[])			// I - Command-line arguments
{
  FILE		*fp;			// Log file
  char		line[1024],		// Line
		*ptr;			// Pointer into line
  log_addr_t	thread;			// Thread
  char		*function;		// Function name
  char		action;			// First character of action ("rdlock", "wrlock", "unlock")
  log_addr_t	obj;			// Object
  char		*objname;		// Object name
  int		i, j;			// Looping vars
  int		num_objs = 0;		// Number of objects
  log_obj_t	*objs,			// Objects
		*curobj;		// Current object
  int		errors = 0;		// Number of errors


  // Check command-line...
  if (argc != 2)
  {
    fputs("Usage: parse-lock-log FILENAME.log\n", stderr);
    return (1);
  }

  if ((fp = fopen(argv[1], "r")) == NULL)
  {
    perror(argv[1]);
    return (1);
  }

  if ((objs = calloc(MAX_OBJS, sizeof(log_obj_t))) == NULL)
  {
    perror("Unable to allocate objects");
    return (1);
  }

  // Scan log...
  while (fgets(line, sizeof(line), fp))
  {
    // Parse log line of the following format:
    //
    //   THREAD/FUNCTION: ACTION OBJ(OBJNAME)
    thread = strtoull(line, &ptr, 16);
    if (!ptr || *ptr != '/' || !ptr[1])
      continue;

    function = ptr + 1;
    if ((ptr = strchr(function, ':')) == NULL)
      continue;
    *ptr++ = '\0';

    while (*ptr && isspace(*ptr))
      ptr ++;

    if (strncmp(ptr, "rdlock ", 7) && strncmp(ptr, "unlock ", 7) && strncmp(ptr, "wrlock ", 7))
      continue;
    action = *ptr;

    obj = strtoull(ptr + 7, &ptr, 16);
    if (!ptr || *ptr != '(')
      continue;

    objname = ptr + 1;
    if ((ptr = strchr(objname, ')')) == NULL)
      continue;
    *ptr = '\0';

    // Find the object...
    for (i = 0, curobj = objs; i < num_objs; i ++, curobj ++)
    {
      if (curobj->address == obj)
        break;
    }

    if (i >= num_objs)
    {
      // New object...
      num_objs ++;
      if (num_objs > MAX_OBJS)
      {
        fprintf(stderr, "ERROR: Reached limit of %d objects.\n", MAX_OBJS);
        return (1);
      }

      memset(curobj, 0, sizeof(log_obj_t));
      strncpy(curobj->name, objname, sizeof(curobj->name) - 1);
      curobj->address = obj;
    }

    // Now process the rdlock, unlock, or wrlock action...
    if (action == 'r')
    {
      // rdlock
      if (curobj->num_wr_threads)
      {
        // Report on active writer locks...
        for (i = 0; i < curobj->num_wr_threads; i ++)
        {
          if (curobj->wr_threads[i] == thread)
          {
            printf("%llX/%s trying to get a read lock while holding a write lock (%s).\n", thread, function, curobj->wr_functions[i]);
            errors ++;
            break;
          }
        }

#if SHOW_BLOCKING_LOCKS
        if (i >= curobj->num_wr_threads)
        {
	  printf("DEBUG: %llX/%s trying to get a read lock while write lock held by", thread, function);
	  for (i = 0; i < curobj->num_wr_threads; i ++)
	    printf(" %llX", curobj->wr_threads[i]);
	  putchar('\n');
	}
#endif // SHOW_BLOCKING_LOCKS
      }

      // Check for extra read locks...
      for (i = 0; i < curobj->num_rd_threads; i ++)
      {
        if (curobj->rd_threads[i] == thread)
        {
	  printf("%llX/%s trying to get a read lock while holding a read lock.\n", thread, function);
	  errors ++;
	  continue;
	}
      }

      // Add read lock...
      if (curobj->num_rd_threads >= MAX_THREADS)
      {
	fprintf(stderr, "ERROR: Reached limit of %d threads.\n", MAX_THREADS);
	return (1);
      }

      curobj->rd_threads[curobj->num_rd_threads] = thread;
      strncpy(curobj->rd_functions[curobj->num_rd_threads], function, sizeof(curobj->rd_functions[0]) - 1);
      curobj->num_rd_threads ++;
    }
    else if (action == 'u')
    {
      // unlock
      if (curobj->num_rd_threads == 0 && curobj->num_wr_threads == 0)
      {
        printf("%llX/%s trying to unlock but there are no locks.\n", thread, function);
        errors ++;
        continue;
      }

      for (i = 0; i < curobj->num_rd_threads; i ++)
      {
        if (curobj->rd_threads[i] == thread)
          break;
      }

      for (j = 0; j < curobj->num_wr_threads; j ++)
      {
        if (curobj->wr_threads[j] == thread)
          break;
      }

      if (i >= curobj->num_rd_threads && j >= curobj->num_wr_threads)
      {
        printf("%llX/%s trying to unlock but does not hold a lock.\n", thread, function);
        errors ++;
        continue;
      }

      if (i < curobj->num_rd_threads)
      {
        // Remove thread from read locks
        curobj->num_rd_threads --;
        if (i < curobj->num_rd_threads)
        {
          memmove(curobj->rd_threads + i, curobj->rd_threads + i + 1, (size_t)(curobj->num_rd_threads - i) * sizeof(log_addr_t));
          memmove(curobj->rd_functions[i], curobj->rd_functions[i + 1], (size_t)(curobj->num_rd_threads - i) * sizeof(curobj->rd_functions[0]));
        }
      }

      if (j < curobj->num_wr_threads)
      {
        // Remove thread from write locks
        curobj->num_wr_threads --;
        if (j < curobj->num_wr_threads)
        {
          memmove(curobj->wr_threads + j, curobj->wr_threads + j + 1, (size_t)(curobj->num_wr_threads - j) * sizeof(log_addr_t));
          memmove(curobj->wr_functions[j], curobj->wr_functions[j + 1], (size_t)(curobj->num_wr_threads - j) * sizeof(curobj->wr_functions[0]));
        }
      }
    }
    else
    {
      // wrlock
      for (i = 0; i < curobj->num_rd_threads; i ++)
      {
        if (curobj->rd_threads[i] == thread)
        {
          printf("%llX/%s trying to get a write lock while holding a read lock (%s).\n", thread, function, curobj->rd_functions[i]);
          errors ++;
          break;
        }
      }

#if SHOW_BLOCKING_LOCKS
      if (i >= curobj->num_rd_threads && i)
      {
	printf("DEBUG: %llX/%s trying to get a write lock while read lock held by", thread, function);
	for (i = 0; i < curobj->num_rd_threads; i ++)
	  printf(" %llX", curobj->rd_threads[i]);
	putchar('\n');
      }
#endif // SHOW_BLOCKING_LOCKS

      for (i = 0; i < curobj->num_wr_threads; i ++)
      {
        if (curobj->wr_threads[i] == thread)
        {
          printf("%llX/%s trying to get a write lock while holding a write lock.\n", thread, function);
          errors ++;
          continue;
        }
      }

#if SHOW_BLOCKING_LOCKS
      if (i >= curobj->num_wr_threads && i)
      {
	printf("DEBUG: %llX/%s trying to get a write lock while write lock held by", thread, function);
	for (i = 0; i < curobj->num_wr_threads; i ++)
	  printf(" %llX", curobj->wr_threads[i]);
	putchar('\n');
      }
#endif // SHOW_BLOCKING_LOCKS

      // Add write lock...
      if (curobj->num_wr_threads >= MAX_THREADS)
      {
	fprintf(stderr, "ERROR: Reached limit of %d threads.\n", MAX_THREADS);
	return (1);
      }

      curobj->wr_threads[curobj->num_wr_threads] = thread;
      strncpy(curobj->wr_functions[curobj->num_wr_threads], function, sizeof(curobj->wr_functions[0]) - 1);
      curobj->num_wr_threads ++;
    }
  }

  // Check objects...
  puts("\nSummary:\n");
  printf("  %d object(s) with %d error(s) in run\n", num_objs, errors);

  for (i = num_objs, curobj = objs; i > 0; i --, curobj ++)
  {
    if (curobj->num_wr_threads)
    {
      printf("  %llX(%s) still has %d write lock(s):", curobj->address, curobj->name, curobj->num_wr_threads);
      for (j = 0; j < curobj->num_wr_threads; j ++)
        printf(" %llX/%s", curobj->wr_threads[j], curobj->wr_functions[j]);
      putchar('\n');
    }

    if (curobj->num_rd_threads)
    {
      printf("  %llX(%s) still has %d read lock(s):", curobj->address, curobj->name, curobj->num_rd_threads);
      for (j = 0; j < curobj->num_rd_threads; j ++)
        printf(" %llX/%s", curobj->rd_threads[j], curobj->rd_functions[j]);
      putchar('\n');
    }
  }

  // Clean up and return...
  fclose(fp);

  return (0);
}
