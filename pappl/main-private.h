//
// Private main header file for the Printer Application Framework
//
// Copyright Â©Â 2020 by Michael R Sweet.
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


extern bool    _papplMainAddPrinter(char *base_name, int num_options, cups_option_t *options) _PAPPL_PRIVATE;
extern bool    _papplMainCancelJob(char *base_name, int num_options, cups_option_t *options) _PAPPL_PRIVATE;
extern bool    _papplMainGetSetDefaultPrinter(char *base_name, int num_options, cups_option_t *options) _PAPPL_PRIVATE;
extern bool    _papplMainDeletePrinter(char *base_name, int num_options, cups_option_t *options) _PAPPL_PRIVATE;
extern bool    _papplMainShowJobs(char *base_name, int num_options, cups_option_t *options) _PAPPL_PRIVATE;
extern bool    _papplMainShowPrinters(char *base_name) _PAPPL_PRIVATE;
extern bool    _papplMainModifyPrinter(char *base_name, int num_options, cups_option_t *options) _PAPPL_PRIVATE;
extern bool    _papplMainShowOptions(char *base_name, int num_options, cups_option_t *options) _PAPPL_PRIVATE;
extern bool    _papplMainRunServer(char *base_name, int num_options, cups_option_t *options, pappl_main_system_cb_t system_cb) _PAPPL_PRIVATE;
extern bool    _papplMainShutdownServer(char *base_name, int num_options, cups_option_t *options) _PAPPL_PRIVATE;
extern bool    _papplMainShowStatus(char *base_name, int num_options, cups_option_t *options) _PAPPL_PRIVATE;
extern bool    _papplMainSubmitJob(char *base_name, int num_options, cups_option_t *options, int num_files, char **files) _PAPPL_PRIVATE;

extern void   _papplMainAddPrinterURI(ipp_t *request, const char *printer_name, char *resource,size_t rsize) _PAPPL_PRIVATE;	
extern http_t *_papplMainConnect(char *base_name, bool auto_start) _PAPPL_PRIVATE;
extern http_t *_papplMainConnectURI(char *base_name, const char *printer_uri, char  *resource, size_t rsize) _PAPPL_PRIVATE;
extern char   *_papplMainGetDefaultPrinter(http_t *http, char *buffer, size_t bufsize) _PAPPL_PRIVATE;
extern char   *_papplMainGetServerPath(char *base_name, char *buffer, size_t bufsize) _PAPPL_PRIVATE;
extern void   _papplMainAddOptions(ipp_t *request, int num_options, cups_option_t *options) _PAPPL_PRIVATE;


//
// C++ magic...
//

#  ifdef __cplusplus
}
#  endif // __cplusplus

#  endif // !_PAPPL_MAIN_PRIVATE_H
