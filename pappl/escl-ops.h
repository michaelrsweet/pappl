//
// Scan eSCL functions for the Printer Application Framework
//
// Copyright © 2019-2022 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#ifndef ESCL_OPS_H
#define ESCL_OPS_H

#include "pappl-private.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>
#include <assert.h>
#include <math.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

  //
  // Types...
  //

  typedef struct ScanSettingsXml
  {
    char *xml;
    char *version;
    char *intent;
    char *height;
    char *contentRegionUnits;
    double width;
    double xOffset;
    double yOffset;
    char *inputSource;
    char *colorMode;
    char *blankPageDetection;
  } ScanSettingsXml;

  //
  // Functions...
  //

  char *readXmlContent(const char *filePath);

  void initScanSettingsXml(ScanSettingsXml *settings, const char *s);

  char *getString(const ScanSettingsXml *settings, const char *name, const char *pattern);

  double getNumber(const ScanSettingsXml *settings, const char *name, const char *pattern);

  bool ClientAlreadyAirScan(pappl_client_t *client);

  ScanSettingsXml *ScanSettingsFromXML(const char *xmlString, pappl_client_t *client);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // ESCL_OPS_H
