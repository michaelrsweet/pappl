//
// Private main header file for the Printer Application Framework
//
// Copyright Â©Â 2020 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#ifndef _PAPPL_MAIN_PRIVATE_H_
#  define _PAPPL_MAIN_PRIVATE_H_

//
// Include necessary headers
//

#  include "main.h"
#  include "base-private.h"


//
// C++ magic...
//

#  ifdef __cplusplus
extern "C" {
#  endif // __cplusplus


extern int    _papplMainAdd(int num_options, cups_option_t *options) _PAPPL_PRIVATE;
extern int    _papplMainCancel(int num_options, cups_option_t *options) _PAPPL_PRIVATE;
extern int    _papplMainDefault(int num_options, cups_option_t *options) _PAPPL_PRIVATE;
extern int    _papplMainDelete(int num_options, cups_option_t *options) _PAPPL_PRIVATE;
extern int    _papplMainJobs(int num_options, cups_option_t *options) _PAPPL_PRIVATE;
extern int    _papplMainListPrinters(void) _PAPPL_PRIVATE;
extern int    _papplMainModify(int num_options, cups_option_t *options) _PAPPL_PRIVATE;
extern int    _papplMainOptions(int num_options, cups_option_t *options) _PAPPL_PRIVATE;
extern int    _papplMainServer(int num_options, cups_option_t *options, pappl_driver_cb_t cb) _PAPPL_PRIVATE;
extern int    _papplMainShutdown(int num_options, cups_option_t *options) _PAPPL_PRIVATE;
extern int    _papplMainStatus(int num_options, cups_option_t *options) _PAPPL_PRIVATE;
extern int    _papplMainSubmit(int num_options, cups_option_t *options, int num_files, char **files) _PAPPL_PRIVATE;

extern void   _papplMainAddPrinterURI(ipp_t *request, const char *printer_name, char *resource,size_t rsize) _PAPPL_PRIVATE;	
extern http_t *_papplMainConnect(int auto_start) _PAPPL_PRIVATE;
extern http_t *_papplMainConnectURI(const char *printer_uri, char  *resource, size_t rsize) _PAPPL_PRIVATE;
extern char   *_papplMainGetDefaultPrinter(http_t *http, char *buffer, size_t bufsize) _PAPPL_PRIVATE;
extern char   *_papplMainGetServerPath(char *buffer, size_t bufsize) _PAPPL_PRIVATE;
extern void   _papplMainAddOptions(ipp_t *request, int num_options, cups_option_t *options) _PAPPL_PRIVATE;


//
// C++ magic...
//

#  ifdef __cplusplus
}
#  endif // __cplusplus

#  endif // !_PAPPL_MAIN_PRIVATE_H
