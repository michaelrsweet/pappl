//
// XML Create function from scan job data-structures for the Printer Application Framework.
//
// Copyright © 2020-2023 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

//
// Include necessary headers...
//

#include <libxml2/libxml/parser.h>
#include <libxml2/libxml/tree.h>
#include "scanner-capabilities.h"

char *copyString(const char *source)
{
  char *dest = malloc(strlen(source) + 1);
  if (dest == NULL)
  {
    exit(1); // Memory allocation error. Ideally, handle gracefully.
  }
  strcpy(dest, source);
  return dest;
}

ScannerCapabilities *populateScannerCapabilities()
{

  ScannerCapabilities *scanner = malloc(sizeof(ScannerCapabilities));
  if (scanner == NULL)
  {
    exit(1);
  }

  FILE *file = fopen("DummyDriver/ScannerCapabilities.txt", "r");
  if (!file)
  {
    perror("Unable to open the file");
    return 1;
  }

  char line[256];
  while (fgets(line, sizeof(line), file))
  {
    if (strstr(line, "Version: "))
    {
      sscanf(line, "Version: %s", scanner->Version);
    }
    if (strstr(line, "MakeAndModel: "))
    {
      sscanf(line, "MakeAndModel: %s", scanner->MakeAndModel);
    }
    if (strstr(line, "SerialNumber: "))
    {
      sscanf(line, "SerialNumber: %s", scanner->SerialNumber);
    }
    if (strstr(line, "UUID: "))
    {
      sscanf(line, "UUID: %s", scanner->UUID);
    }
    if (strstr(line, "AdminURI: "))
    {
      sscanf(line, "AdminURI: %s", scanner->AdminURI);
    }
    if (strstr(line, "IconURI: "))
    {
      sscanf(line, "IconURI: %s", scanner->IconURI);
    }

    if (strstr(line, "Profile name: p1"))
    {
      SettingProfile sp = {0};

      while (fgets(line, sizeof(line), file))
      {
        // Parse ColorModes
        if (strstr(line, "ColorModes:"))
        {
          while (fgets(line, sizeof(line), file) && strstr(line, "ColorMode: "))
          {
            char mode[100];
            sscanf(line, "ColorMode: %s", mode);
            sp.ColorModes[sp.ColorModeCount] = strdup(mode); // Assuming dynamic allocation for the ColorModes
            sp.ColorModeCount++;
          }
        }
        // Parse DocumentFormats
        else if (strstr(line, "DocumentFormats:"))
        {
          while (fgets(line, sizeof(line), file) && (strstr(line, "DocumentFormat: ") || strstr(line, "DocumentFormatExt: ")))
          {
            char format[100];
            sscanf(line, "DocumentFormat: %s", format);
            sp.DocumentFormats[sp.DocumentFormatCount] = strdup(format);
            sp.DocumentFormatCount++;
          }
        }
        // Parse SupportedResolutions
        else if (strstr(line, "SupportedResolutions:"))
        {
          while (fgets(line, sizeof(line), file) && strstr(line, "DiscreteResolution:"))
          {
            DiscreteResolution dr;
            fgets(line, sizeof(line), file); // Get XResolution
            sscanf(line, "XResolution: %d", &dr.XResolution);
            fgets(line, sizeof(line), file); // Get YResolution
            sscanf(line, "YResolution: %d", &dr.YResolution);
            sp.supportedResolutions.discreteResolutions.resolutions[sp.supportedResolutions.discreteResolutions.count] = dr;
            sp.supportedResolutions.discreteResolutions.count++;
          }
        }
        // Parse CcdChannels
        else if (strstr(line, "CcdChannels:"))
        {
          while (fgets(line, sizeof(line), file) && strstr(line, "CcdChannel"))
          {
            CcdChannel cc;
            char channel[100];
            if (sscanf(line, "CcdChannel (default=true): %s", channel) == 1)
            {
              cc.isDefault = true;
            }
            else
            {
              sscanf(line, "CcdChannel: %s", channel);
              cc.isDefault = false;
            }
            cc.CcdChannel = strdup(channel);
            sp.ccdChannels[sp.ccdChannelsCount] = cc;
            sp.ccdChannelsCount++;
          }
        }
        // Parse BinaryRenderings
        else if (strstr(line, "BinaryRenderings:"))
        {
          while (fgets(line, sizeof(line), file) && strstr(line, "BinaryRendering"))
          {
            BinaryRendering br;
            char rendering[100];
            if (sscanf(line, "BinaryRendering (default=true): %s", rendering) == 1)
            {
              br.isDefault = true;
            }
            else
            {
              sscanf(line, "BinaryRendering: %s", rendering);
              br.isDefault = false;
            }
            br.BinaryRendering = strdup(rendering);
            sp.binaryRenderings[sp.binaryRenderingsCount] = br;
            sp.binaryRenderingsCount++;
          }
        }

        // To break out of the loop when you've finished parsing the Profile p1
        if (strstr(line, "Platen:") || strstr(line, "Adf:") || strstr(line, "StoredJobRequestSupport:"))
        {
          break;
        }
      }
      scanner->settingProfiles[scanner->settingProfilesCount] = sp;
      scanner->settingProfilesCount++;
    }

    if (strstr(line, "Platen:"))
    {
      PlatenInputCaps pic = {0};

      while (fgets(line, sizeof(line), file))
      {
        // Parse PlatenInputCaps details
        if (strstr(line, "PlatenInputCaps:"))
        {
          while (fgets(line, sizeof(line), file) &&
                 (strstr(line, "MinWidth:") || strstr(line, "MaxWidth:") ||
                  strstr(line, "MinHeight:") || strstr(line, "MaxHeight:") ||
                  strstr(line, "MaxScanRegions:")))
          {

            if (strstr(line, "MinWidth:"))
            {
              sscanf(line, "MinWidth: %d", &pic.MinWidth);
            }
            else if (strstr(line, "MaxWidth:"))
            {
              sscanf(line, "MaxWidth: %d", &pic.MaxWidth);
            }
            else if (strstr(line, "MinHeight:"))
            {
              sscanf(line, "MinHeight: %d", &pic.MinHeight);
            }
            else if (strstr(line, "MaxHeight:"))
            {
              sscanf(line, "MaxHeight: %d", &pic.MaxHeight);
            }
            else if (strstr(line, "MaxScanRegions:"))
            {
              sscanf(line, "MaxScanRegions: %d", &pic.MaxScanRegions);
            }
          }
        }
        // Parse SettingProfiles for Platen
        else if (strstr(line, "SettingProfiles:"))
        {
          pic.settingProfiles[pic.settingProfilesCount] = scanner->settingProfiles[scanner->settingProfilesCount];
          pic.settingProfilesCount++;
        }
        else if (strstr(line, "MaxOpticalXResolution:"))
        {
          // Assume that these belong to the main platen structure for simplicity.
          // If they belong to some specific profile, adjust accordingly.
          sscanf(line, "MaxOpticalXResolution: %d", &pic.MaxOpticalXResolution);
        }
        else if (strstr(line, "MaxOpticalYResolution:"))
        {
          sscanf(line, "MaxOpticalYResolution: %d", &pic.MaxOpticalYResolution);
        }
        else if (strstr(line, "RiskyLeftMargin:"))
        {
          sscanf(line, "RiskyLeftMargin: %d", &pic.RiskyLeftMargin);
        }
        else if (strstr(line, "RiskyRightMargin:"))
        {
          sscanf(line, "RiskyRightMargin: %d", &pic.RiskyRightMargin);
        }
        else if (strstr(line, "RiskyTopMargin:"))
        {
          sscanf(line, "RiskyTopMargin: %d", &pic.RiskyTopMargin);
        }
        else if (strstr(line, "RiskyBottomMargin:"))
        {
          sscanf(line, "RiskyBottomMargin: %d", &pic.RiskyBottomMargin);
        }

        // To break out of the loop when you've finished parsing the Platen
        if (strstr(line, "Adf:") || strstr(line, "StoredJobRequestSupport:"))
        {
          break;
        }
      }
      scanner->platenInputCaps = pic;
    }

    if (strstr(line, "Adf:"))
    {
      Adf adfStruct = {0};

      while (fgets(line, sizeof(line), file))
      {
        // Parse AdfSimplexInputCaps details
        if (strstr(line, "AdfSimplexInputCaps:"))
        {
          AdfSimplexInputCaps asic = {0};

          while (fgets(line, sizeof(line), file) &&
                 (strstr(line, "MinWidth:") || strstr(line, "MaxWidth:") ||
                  strstr(line, "MinHeight:") || strstr(line, "MaxHeight:")))
          {

            if (strstr(line, "MinWidth:"))
            {
              sscanf(line, "MinWidth: %d", &asic.MinWidth);
            }
            else if (strstr(line, "MaxWidth:"))
            {
              sscanf(line, "MaxWidth: %d", &asic.MaxWidth);
            }
            else if (strstr(line, "MinHeight:"))
            {
              sscanf(line, "MinHeight: %d", &asic.MinHeight);
            }
            else if (strstr(line, "MaxHeight:"))
            {
              sscanf(line, "MaxHeight: %d", &asic.MaxHeight);
            }
          }

          adfStruct.adfSimplexInputCaps = asic;
        }
        // Parse SettingProfile reference for Adf
        else if (strstr(line, "SettingProfile ref:"))
        {
          adfStruct.adfSimplexInputCaps.settingProfile[adfStruct.adfSimplexInputCaps.settingProfileCount] = scanner->settingProfiles[scanner->settingProfilesCount];
          adfStruct.adfSimplexInputCaps.settingProfileCount++;

          // Assuming we have a function to retrieve profile based on name
          // Here it's simply mentioned for understanding and would require actual implementation.
          // char profileName[256];
          // sscanf(line, "SettingProfile ref: %s", profileName);
          // adfStruct.adfSimplexInputCaps.settingProfile = getProfileByName(profileName);
        }
        else if (strstr(line, "EdgeAutoDetection:"))
        {
          // Parse SupportedEdge details
          fgets(line, sizeof(line), file); // get the next line
          sscanf(line, "SupportedEdge: %s", &adfStruct.adfSimplexInputCaps.SupportedEdge);
        }
        else if (strstr(line, "MaxOpticalXResolution:"))
        {
          sscanf(line, "MaxOpticalXResolution: %d", &adfStruct.adfSimplexInputCaps.MaxOpticalXResolution);
        }
        else if (strstr(line, "MaxOpticalYResolution:"))
        {
          sscanf(line, "MaxOpticalYResolution: %d", &adfStruct.adfSimplexInputCaps.MaxOpticalYResolution);
        }
        else if (strstr(line, "RiskyLeftMargin:"))
        {
          sscanf(line, "RiskyLeftMargin: %d", &adfStruct.adfSimplexInputCaps.RiskyLeftMargin);
        }
        else if (strstr(line, "RiskyRightMargin:"))
        {
          sscanf(line, "RiskyRightMargin: %d", &adfStruct.adfSimplexInputCaps.RiskyRightMargin);
        }
        else if (strstr(line, "RiskyTopMargin:"))
        {
          sscanf(line, "RiskyTopMargin: %d", &adfStruct.adfSimplexInputCaps.RiskyTopMargin);
        }
        else if (strstr(line, "RiskyBottomMargin:"))
        {
          sscanf(line, "RiskyBottomMargin: %d", &adfStruct.adfSimplexInputCaps.RiskyBottomMargin);
        }
        else if (strstr(line, "FeederCapacity:"))
        {
          sscanf(line, "FeederCapacity: %d", &adfStruct.FeederCapacity);
        }
        else if (strstr(line, "AdfOptions:"))
        {
          while (fgets(line, sizeof(line), file) && strstr(line, "AdfOption:"))
          {
            char option[256];
            sscanf(line, "AdfOption: %s", option);
            // Store the option. This assumes that AdfOptions is a pre-allocated array
            adfStruct.AdfOptions[adfStruct.adfOptionsCount] = strdup(option);
            adfStruct.adfOptionsCount++;
          }
        }

        // To break out of the loop when you've finished parsing the Adf
        if (strstr(line, "StoredJobRequestSupport:") || strstr(line, "BlankPageDetection:") || strstr(line, "BlankPageDetectionAndRemoval:"))
        {
          break;
        }
      }

      scanner->adf = adfStruct;
    }
    if (strstr(line, "StoredJobRequestSupport:"))
    {
      StoredJobRequestSupport sjrs = {0};

      while (fgets(line, sizeof(line), file))
      {
        if (strstr(line, "MaxStoredJobRequests:"))
        {
          sscanf(line, "MaxStoredJobRequests: %d", &sjrs.MaxStoredJobRequests);
        }
        else if (strstr(line, "TimeoutInSeconds:"))
        {
          sscanf(line, "TimeoutInSeconds: %d", &sjrs.TimeoutInSeconds);
        }

        // To break out of the loop when you've finished parsing the StoredJobRequestSupport
        if (strstr(line, "BlankPageDetection:") || strstr(line, "BlankPageDetectionAndRemoval:"))
        {
          break;
        }
      }

      scanner->storedJobRequestSupport = sjrs;
    }
    if (strstr(line, "BlankPageDetection:"))
    {
      char boolVal[6]; // Maximum length to hold "false" plus null terminator
      sscanf(line, "BlankPageDetection: %s", boolVal);
      scanner->BlankPageDetection = strcmp(boolVal, "true") == 0;
    }
    if (strstr(line, "BlankPageDetectionAndRemoval:"))
    {
      char boolVal[6];
      sscanf(line, "BlankPageDetectionAndRemoval: %s", boolVal);
      scanner->BlankPageDetectionAndRemoval = strcmp(boolVal, "true") == 0;
    }
  }

  return scanner;
}

// Function to create an XML representation of ScannerCapabilities
void createXML(
    ScannerCapabilities *capabilties, // I - Data Structure which stores job attributes
    xmlChar **xmlBufferPtr)           // I - XML buffer created from the job
{
  xmlDocPtr doc = NULL;
  xmlNodePtr root_node = NULL, child_node = NULL, grandchild_node = NULL;
  int bufferSize;
  char buffer[256];

  // Create a new XML document with "UTF-8" encoding and the root node of the document and set it as the root element
  doc = xmlNewDoc(BAD_CAST "UTF-8");
  root_node = xmlDocSetRootElement(doc, xmlNewNode(NULL, BAD_CAST "ScannerCapabilities"));

  // Add Version, MakeAndModel, and SerialNumber as child elements of the root node
  xmlNewChild(root_node, NULL, BAD_CAST "Version", BAD_CAST capabilties->Version);
  xmlNewChild(root_node, NULL, BAD_CAST "MakeAndModel", BAD_CAST capabilties->MakeAndModel);
  xmlNewChild(root_node, NULL, BAD_CAST "SerialNumber", BAD_CAST capabilties->SerialNumber);

  // Add PlatenInputCaps as a child node of the root node and set its attributes
  child_node = xmlNewChild(root_node, NULL, BAD_CAST "PlatenInputCaps", NULL);
  sprintf(buffer, "%d", capabilties->platenInputCaps.MinWidth);
  xmlNewChild(child_node, NULL, BAD_CAST "MinWidth", BAD_CAST buffer);
  sprintf(buffer, "%d", capabilties->platenInputCaps.MaxWidth);
  xmlNewChild(child_node, NULL, BAD_CAST "MaxWidth", BAD_CAST buffer);
  sprintf(buffer, "%d", capabilties->platenInputCaps.MinHeight);
  xmlNewChild(child_node, NULL, BAD_CAST "MinHeight", BAD_CAST buffer);
  sprintf(buffer, "%d", capabilties->platenInputCaps.MaxHeight);
  xmlNewChild(child_node, NULL, BAD_CAST "MaxHeight", BAD_CAST buffer);
  sprintf(buffer, "%d", capabilties->platenInputCaps.MaxScanRegions);
  xmlNewChild(child_node, NULL, BAD_CAST "MaxScanRegions", BAD_CAST buffer);

  for (int i = 0; i < capabilties->settingProfilesCount; i++)
  {
    child_node = xmlNewChild(root_node, NULL, BAD_CAST "SettingProfile", NULL);
    xmlNewChild(child_node, NULL, BAD_CAST "ColorMode", BAD_CAST capabilties->settingProfiles[i].ColorModes);
    xmlNewChild(child_node, NULL, BAD_CAST "DocumentFormat", BAD_CAST capabilties->settingProfiles[i].DocumentFormats);

    grandchild_node = xmlNewChild(child_node, NULL, BAD_CAST "SupportedResolutions", NULL);
    sprintf(buffer, "%d", capabilties->settingProfiles[i].supportedResolutions.isDiscrete);
    xmlNewChild(grandchild_node, NULL, BAD_CAST "isDiscrete", BAD_CAST buffer);

    if (capabilties->settingProfiles[i].supportedResolutions.isDiscrete)
    {
      DiscreteResolutions resolutions = capabilties->settingProfiles[i].supportedResolutions.discreteResolutions;
      for (int j = 0; j < resolutions.count; j++)
      {
        grandchild_node = xmlNewChild(child_node, NULL, BAD_CAST "DiscreteResolution", NULL);
        sprintf(buffer, "%d", resolutions.resolutions[j].XResolution);
        xmlNewChild(grandchild_node, NULL, BAD_CAST "XResolution", BAD_CAST buffer);
        sprintf(buffer, "%d", resolutions.resolutions[j].YResolution);
        xmlNewChild(grandchild_node, NULL, BAD_CAST "YResolution", BAD_CAST buffer);
      }
    }
    else
    {
      ResolutionRanges ranges = capabilties->settingProfiles[i].supportedResolutions.resolutionRanges;

      grandchild_node = xmlNewChild(child_node, NULL, BAD_CAST "ResolutionRanges", NULL);

      sprintf(buffer, "%d", ranges.XResolutionRange.Min);
      xmlNewChild(grandchild_node, NULL, BAD_CAST "XMin", BAD_CAST buffer);
      sprintf(buffer, "%d", ranges.XResolutionRange.Max);
      xmlNewChild(grandchild_node, NULL, BAD_CAST "XMax", BAD_CAST buffer);
      sprintf(buffer, "%d", ranges.XResolutionRange.Normal);
      xmlNewChild(grandchild_node, NULL, BAD_CAST "XNormal", BAD_CAST buffer);
      sprintf(buffer, "%d", ranges.XResolutionRange.Step);
      xmlNewChild(grandchild_node, NULL, BAD_CAST "XStep", BAD_CAST buffer);

      sprintf(buffer, "%d", ranges.YResolutionRange.Min);
      xmlNewChild(grandchild_node, NULL, BAD_CAST "YMin", BAD_CAST buffer);
      sprintf(buffer, "%d", ranges.YResolutionRange.Max);
      xmlNewChild(grandchild_node, NULL, BAD_CAST "YMax", BAD_CAST buffer);
      sprintf(buffer, "%d", ranges.YResolutionRange.Normal);
      xmlNewChild(grandchild_node, NULL, BAD_CAST "YNormal", BAD_CAST buffer);
      sprintf(buffer, "%d", ranges.YResolutionRange.Step);
      xmlNewChild(grandchild_node, NULL, BAD_CAST "YStep", BAD_CAST buffer);
    }

    sprintf(buffer, "%d", capabilties->settingProfiles[i].ccdChannelsCount);
    xmlNewChild(child_node, NULL, BAD_CAST "CcdChannelsCount", BAD_CAST buffer);

    // Fill CCD Channels
    for (int j = 0; j < capabilties->settingProfiles[i].ccdChannelsCount; j++)
    {
      grandchild_node = xmlNewChild(child_node, NULL, BAD_CAST "CcdChannel", NULL);
      xmlNewChild(grandchild_node, NULL, BAD_CAST "CcdChannel", BAD_CAST capabilties->settingProfiles[i].ccdChannels[j].CcdChannel);
      sprintf(buffer, "%d", capabilties->settingProfiles[i].ccdChannels[j].isDefault);
      xmlNewChild(grandchild_node, NULL, BAD_CAST "isDefault", BAD_CAST buffer);
    }

    sprintf(buffer, "%d", capabilties->settingProfiles[i].binaryRenderingsCount);
    xmlNewChild(child_node, NULL, BAD_CAST "BinaryRenderingsCount", BAD_CAST buffer);

    // Fill Binary Renderings
    for (int j = 0; j < capabilties->settingProfiles[i].binaryRenderingsCount; j++)
    {
      grandchild_node = xmlNewChild(child_node, NULL, BAD_CAST "BinaryRendering", NULL);
      xmlNewChild(grandchild_node, NULL, BAD_CAST "BinaryRendering", BAD_CAST capabilties->settingProfiles[i].binaryRenderings[j].BinaryRendering);
      sprintf(buffer, "%d", capabilties->settingProfiles[i].binaryRenderings[j].isDefault);
      xmlNewChild(grandchild_node, NULL, BAD_CAST "isDefault", BAD_CAST buffer);
    }
  }
  sprintf(buffer, "%d", capabilties->platenInputCaps.colorSpacesCount);
  xmlNewChild(child_node, NULL, BAD_CAST "ColorSpacesCount", BAD_CAST buffer);

  // Fill Color Spaces
  for (int i = 0; i < capabilties->platenInputCaps.colorSpacesCount; i++)
  {
    grandchild_node = xmlNewChild(child_node, NULL, BAD_CAST "ColorSpace", NULL);
    xmlNewChild(grandchild_node, NULL, BAD_CAST "ColorSpace", BAD_CAST capabilties->platenInputCaps.colorSpaces[i].ColorSpace);
    sprintf(buffer, "%d", capabilties->platenInputCaps.colorSpaces[i].isDefault);
    xmlNewChild(grandchild_node, NULL, BAD_CAST "isDefault", BAD_CAST buffer);
  }

  // Add ADF as a child node of the root node and set its attributes
  child_node = xmlNewChild(root_node, NULL, BAD_CAST "Adf", NULL);
  grandchild_node = xmlNewChild(child_node, NULL, BAD_CAST "AdfSimplexInputCaps", NULL);

  sprintf(buffer, "%d", capabilties->adf.adfSimplexInputCaps.MinWidth);
  xmlNewChild(grandchild_node, NULL, BAD_CAST "MinWidth", BAD_CAST buffer);
  sprintf(buffer, "%d", capabilties->adf.adfSimplexInputCaps.MaxWidth);
  xmlNewChild(grandchild_node, NULL, BAD_CAST "MaxWidth", BAD_CAST buffer);
  sprintf(buffer, "%d", capabilties->adf.adfSimplexInputCaps.MinHeight);
  xmlNewChild(grandchild_node, NULL, BAD_CAST "MinHeight", BAD_CAST buffer);
  sprintf(buffer, "%d", capabilties->adf.adfSimplexInputCaps.MaxHeight);
  xmlNewChild(grandchild_node, NULL, BAD_CAST "MaxHeight", BAD_CAST buffer);

  for (int i = 0; i < capabilties->settingProfilesCount; i++)
  {
    child_node = xmlNewChild(root_node, NULL, BAD_CAST "SettingProfile", NULL);
    xmlNewChild(child_node, NULL, BAD_CAST "ColorMode", BAD_CAST capabilties->settingProfiles[i].ColorModes);
    xmlNewChild(child_node, NULL, BAD_CAST "DocumentFormat", BAD_CAST capabilties->settingProfiles[i].DocumentFormats);

    grandchild_node = xmlNewChild(child_node, NULL, BAD_CAST "SupportedResolutions", NULL);
    sprintf(buffer, "%d", capabilties->settingProfiles[i].supportedResolutions.isDiscrete);
    xmlNewChild(grandchild_node, NULL, BAD_CAST "isDiscrete", BAD_CAST buffer);
    if (capabilties->settingProfiles[i].supportedResolutions.isDiscrete)
    {
      DiscreteResolutions resolutions = capabilties->settingProfiles[i].supportedResolutions.discreteResolutions;
      for (int j = 0; j < resolutions.count; j++)
      {
        grandchild_node = xmlNewChild(child_node, NULL, BAD_CAST "DiscreteResolution", NULL);
        sprintf(buffer, "%d", resolutions.resolutions[j].XResolution);
        xmlNewChild(grandchild_node, NULL, BAD_CAST "XResolution", BAD_CAST buffer);
        sprintf(buffer, "%d", resolutions.resolutions[j].YResolution);
        xmlNewChild(grandchild_node, NULL, BAD_CAST "YResolution", BAD_CAST buffer);
      }
    }
    else
    {
      ResolutionRanges ranges = capabilties->settingProfiles[i].supportedResolutions.resolutionRanges;

      grandchild_node = xmlNewChild(child_node, NULL, BAD_CAST "ResolutionRanges", NULL);

      sprintf(buffer, "%d", ranges.XResolutionRange.Min);
      xmlNewChild(grandchild_node, NULL, BAD_CAST "XMin", BAD_CAST buffer);
      sprintf(buffer, "%d", ranges.XResolutionRange.Max);
      xmlNewChild(grandchild_node, NULL, BAD_CAST "XMax", BAD_CAST buffer);
      sprintf(buffer, "%d", ranges.XResolutionRange.Normal);
      xmlNewChild(grandchild_node, NULL, BAD_CAST "XNormal", BAD_CAST buffer);
      sprintf(buffer, "%d", ranges.XResolutionRange.Step);
      xmlNewChild(grandchild_node, NULL, BAD_CAST "XStep", BAD_CAST buffer);

      sprintf(buffer, "%d", ranges.YResolutionRange.Min);
      xmlNewChild(grandchild_node, NULL, BAD_CAST "YMin", BAD_CAST buffer);
      sprintf(buffer, "%d", ranges.YResolutionRange.Max);
      xmlNewChild(grandchild_node, NULL, BAD_CAST "YMax", BAD_CAST buffer);
      sprintf(buffer, "%d", ranges.YResolutionRange.Normal);
      xmlNewChild(grandchild_node, NULL, BAD_CAST "YNormal", BAD_CAST buffer);
      sprintf(buffer, "%d", ranges.YResolutionRange.Step);
      xmlNewChild(grandchild_node, NULL, BAD_CAST "YStep", BAD_CAST buffer);
    }
    sprintf(buffer, "%d", capabilties->settingProfiles[i].ccdChannelsCount);
    xmlNewChild(child_node, NULL, BAD_CAST "CcdChannelsCount", BAD_CAST buffer);
    // Fill CCD Channels
    for (int j = 0; j < capabilties->settingProfiles[i].ccdChannelsCount; j++)
    {
      grandchild_node = xmlNewChild(child_node, NULL, BAD_CAST "CcdChannel", NULL);
      xmlNewChild(grandchild_node, NULL, BAD_CAST "CcdChannel", BAD_CAST capabilties->settingProfiles[i].ccdChannels[j].CcdChannel);
      sprintf(buffer, "%d", capabilties->settingProfiles[i].ccdChannels[j].isDefault);
      xmlNewChild(grandchild_node, NULL, BAD_CAST "isDefault", BAD_CAST buffer);
    }
    sprintf(buffer, "%d", capabilties->settingProfiles[i].binaryRenderingsCount);
    xmlNewChild(child_node, NULL, BAD_CAST "BinaryRenderingsCount", BAD_CAST buffer);
    // Fill Binary Renderings
    for (int j = 0; j < capabilties->settingProfiles[i].binaryRenderingsCount; j++)
    {
      grandchild_node = xmlNewChild(child_node, NULL, BAD_CAST "BinaryRendering", NULL);
      xmlNewChild(grandchild_node, NULL, BAD_CAST "BinaryRendering", BAD_CAST capabilties->settingProfiles[i].binaryRenderings[j].BinaryRendering);
      sprintf(buffer, "%d", capabilties->settingProfiles[i].binaryRenderings[j].isDefault);
      xmlNewChild(grandchild_node, NULL, BAD_CAST "isDefault", BAD_CAST buffer);
    }
  }
  xmlNewChild(grandchild_node, NULL, BAD_CAST "SupportedEdge", BAD_CAST capabilties->adf.adfSimplexInputCaps.SupportedEdge);
  sprintf(buffer, "%d", capabilties->adf.adfSimplexInputCaps.MaxOpticalXResolution);
  xmlNewChild(grandchild_node, NULL, BAD_CAST "MaxOpticalXResolution", BAD_CAST buffer);
  sprintf(buffer, "%d", capabilties->adf.adfSimplexInputCaps.MaxOpticalYResolution);
  xmlNewChild(grandchild_node, NULL, BAD_CAST "MaxOpticalYResolution", BAD_CAST buffer);
  sprintf(buffer, "%d", capabilties->adf.adfSimplexInputCaps.RiskyLeftMargin);
  xmlNewChild(grandchild_node, NULL, BAD_CAST "RiskyLeftMargin", BAD_CAST buffer);
  sprintf(buffer, "%d", capabilties->adf.adfSimplexInputCaps.RiskyRightMargin);
  xmlNewChild(grandchild_node, NULL, BAD_CAST "RiskyRightMargin", BAD_CAST buffer);
  sprintf(buffer, "%d", capabilties->adf.adfSimplexInputCaps.RiskyTopMargin);
  xmlNewChild(grandchild_node, NULL, BAD_CAST "RiskyTopMargin", BAD_CAST buffer);
  sprintf(buffer, "%d", capabilties->adf.adfSimplexInputCaps.RiskyBottomMargin);
  xmlNewChild(grandchild_node, NULL, BAD_CAST "RiskyBottomMargin", BAD_CAST buffer);
  sprintf(buffer, "%d", capabilties->adf.FeederCapacity);
  xmlNewChild(child_node, NULL, BAD_CAST "FeederCapacity", BAD_CAST buffer);
  sprintf(buffer, "%d", capabilties->adf.adfOptionsCount);
  xmlNewChild(child_node, NULL, BAD_CAST "AdfOptionsCount", BAD_CAST buffer);

  for (int i = 0; i < capabilties->adf.adfOptionsCount; i++)
  {
    xmlNewChild(child_node, NULL, BAD_CAST "AdfOption", BAD_CAST capabilties->adf.AdfOptions[i]);
  }

  child_node = xmlNewChild(root_node, NULL, BAD_CAST "StoredJobRequestSupport", NULL);
  sprintf(buffer, "%d", capabilties->storedJobRequestSupport.MaxStoredJobRequests);
  xmlNewChild(child_node, NULL, BAD_CAST "MaxStoredJobRequests", BAD_CAST buffer);
  sprintf(buffer, "%d", capabilties->storedJobRequestSupport.TimeoutInSeconds);
  xmlNewChild(child_node, NULL, BAD_CAST "TimeoutInSeconds", BAD_CAST buffer);
  xmlNewChild(root_node, NULL, BAD_CAST "UUID", BAD_CAST capabilties->UUID);
  xmlNewChild(root_node, NULL, BAD_CAST "AdminURI", BAD_CAST capabilties->AdminURI);
  xmlNewChild(root_node, NULL, BAD_CAST "IconURI", BAD_CAST capabilties->IconURI);
  sprintf(buffer, "%d", capabilties->BlankPageDetection);
  xmlNewChild(root_node, NULL, BAD_CAST "BlankPageDetection", BAD_CAST buffer);
  sprintf(buffer, "%d", capabilties->BlankPageDetectionAndRemoval);
  xmlNewChild(root_node, NULL, BAD_CAST "BlankPageDetectionAndRemoval", BAD_CAST buffer);

  xmlChar *xmlbuff;
  int buffersize;
  xmlDocDumpFormatMemory(doc, &xmlbuff, &buffersize, 1);

  // Generating file
  FILE *file = fopen("/DummyDriver/ScannerCapabilities.xml", "w");
  if (file)
  {
    fprintf(file, "%s", (char *)xmlbuff);
    fclose(file);
  }

  // Freeing allocated memory
  xmlFree(xmlbuff);
}
