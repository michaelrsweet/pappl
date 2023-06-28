#ifndef ESCL_OPS_H
#define ESCL_OPS_H

#include "pappl-private.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>
#include <assert.h>

#ifdef __cplusplus
extern "C" {
#endif

// Function to read XML content from a file
char* readXmlContent(const char* filePath);

typedef struct ScanSettingsXml {
    char* xml;
} ScanSettingsXml;

void initScanSettingsXml(ScanSettingsXml* settings, const char* s);

char* getString(const ScanSettingsXml* settings, const char* name, const char* pattern);

double getNumber(const ScanSettingsXml* settings, const char* name, const char* pattern);

bool ClientAlreadyAirScan(pappl_client_t* client);

void ScanSettingsFromXML(const char* xmlString, pappl_client_t* client);

#ifdef __cplusplus
}
#endif

#endif /* ESCL_OPS_H */
