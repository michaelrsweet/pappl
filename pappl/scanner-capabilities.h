//
// Scanner attribute data structures for the Printer Application Framework
//
// Copyright © 2019-2022 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#ifndef SCANNER_CAPABILITIES_H
#define SCANNER_CAPABILITIES_H

#include <stdio.h>
#include <stdbool.h>

//
// Types...
//

typedef struct DiscreteResolution
{
  int XResolution;
  int YResolution;
} DiscreteResolution;

typedef struct DiscreteResolutions
{
  int count;
  DiscreteResolution *resolutions;
} DiscreteResolutions;

typedef struct ResolutionRange
{
  int Min;
  int Max;
  int Normal;
  int Step;
} ResolutionRange;

typedef struct ResolutionRanges
{
  ResolutionRange XResolutionRange;
  ResolutionRange YResolutionRange;
} ResolutionRanges;

typedef struct SupportedResolutions
{
  bool isDiscrete;
  union
  {
    DiscreteResolutions discreteResolutions;
    ResolutionRanges resolutionRanges;
  };
} SupportedResolutions;

typedef struct CcdChannel
{
  char *CcdChannel;
  bool isDefault;
} CcdChannel;

typedef struct BinaryRendering
{
  char *BinaryRendering;
  bool isDefault;
} BinaryRendering;

typedef struct SettingProfile
{
  char *ColorMode;
  char *DocumentFormat;
  SupportedResolutions supportedResolutions;
  int ccdChannelsCount;
  CcdChannel *ccdChannels;
  int binaryRenderingsCount;
  BinaryRendering *binaryRenderings;
} SettingProfile;

typedef struct ColorSpace
{
  char *ColorSpace;
  bool isDefault;
} ColorSpace;

typedef struct PlatenInputCaps
{
  int MinWidth;
  int MaxWidth;
  int MinHeight;
  int MaxHeight;
  int MaxScanRegions;
  int settingProfilesCount;
  SettingProfile *settingProfiles;
  int colorSpacesCount;
  ColorSpace *colorSpaces;
} PlatenInputCaps;

typedef struct AdfSimplexInputCaps
{
  int MinWidth;
  int MaxWidth;
  int MinHeight;
  int MaxHeight;
  SettingProfile settingProfile;
  char *SupportedEdge;
  int MaxOpticalXResolution;
  int MaxOpticalYResolution;
  int RiskyLeftMargin;
  int RiskyRightMargin;
  int RiskyTopMargin;
  int RiskyBottomMargin;
} AdfSimplexInputCaps;

typedef struct Adf
{
  AdfSimplexInputCaps adfSimplexInputCaps;
  int FeederCapacity;
  int adfOptionsCount;
  char **AdfOptions;
} Adf;

typedef struct StoredJobRequestSupport
{
  int MaxStoredJobRequests;
  int TimeoutInSeconds;
} StoredJobRequestSupport;

typedef struct ScannerCapabilities
{
  char *Version;
  char *MakeAndModel;
  char *SerialNumber;
  char *UUID;
  char *AdminURI;
  char *IconURI;
  int settingProfilesCount;
  SettingProfile *settingProfiles;
  PlatenInputCaps platenInputCaps;
  Adf adf;
  StoredJobRequestSupport storedJobRequestSupport;
  bool BlankPageDetection;
  bool BlankPageDetectionAndRemoval;
} ScannerCapabilities;

// Functions
void createXML(ScannerCapabilities *capabilties, xmlChar **xmlBufferPtr);

#endif // SCANNER_CAPABILITIES_H