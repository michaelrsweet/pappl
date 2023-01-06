//---------------------------------------------------------------------------------------------------
//
// EPX Test Printer
//
// A virtual IPP Printer used to prototype IPP Enterprise Printing Extensions v2.0 (EPX)
//
// Copyright © 2022 Printer Working Group.
// Written by Smith Kennedy (HP Inc.)
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//
//---------------------------------------------------------------------------------------------------


#ifndef _TESTEPX_H_
#  define _TESTEPX_H_

#include <pappl/pappl.h>

#define EPX_DRIVER_COUNT 1
extern pappl_pr_driver_t epx_drivers[EPX_DRIVER_COUNT];

#ifdef EPX_DRIVER
pappl_pr_driver_t epx_drivers[EPX_DRIVER_COUNT] =
{     /* name */        /* description */       /* device ID */                                         /* extension */
    { "epx-driver",     "EPX Driver",           "MFG:PWG;MDL:EPX Test Model;CMD:PWGRaster,PDF;",        NULL },
};
#endif // EPX_DRIVER


extern const char *epx_pappl_autoadd_cb(const char *device_info, const char *device_uri, const char *device_id, void *data);
extern bool	epx_pappl_driver_cb(pappl_system_t *system, const char *driver_name, const char *device_uri, const char *device_id, pappl_pr_driver_data_t *driver_data, ipp_t **driver_attrs, void *data);


#endif // !_TESTEPX_H_
