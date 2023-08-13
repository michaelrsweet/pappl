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
    xmlNewChild(child_node, NULL, BAD_CAST "ColorMode", BAD_CAST capabilties->settingProfiles[i].ColorMode);
    xmlNewChild(child_node, NULL, BAD_CAST "DocumentFormat", BAD_CAST capabilties->settingProfiles[i].DocumentFormat);

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
    xmlNewChild(child_node, NULL, BAD_CAST "ColorMode", BAD_CAST capabilties->settingProfiles[i].ColorMode);
    xmlNewChild(child_node, NULL, BAD_CAST "DocumentFormat", BAD_CAST capabilties->settingProfiles[i].DocumentFormat);

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
