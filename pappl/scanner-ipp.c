//
// Scanner IPP operation processing for the Printer Application Framework
//
// Copyright © 2019-2026 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//
// This file provides the structural foundation for future IPP Scan
// (PWG 5100.17) support.  Currently it handles:
//   - Basic Get-Scanner-Attributes (maps to scanner capabilities)
//   - Routing of any scanner-targeted IPP requests
//
// Future additions (by another contributor) would add:
//   - Create-Job (scan)
//   - Get-Job-Attributes
//   - Cancel-Job
//   - etc.
//

#include "pappl-private.h"


//
// '_papplScannerProcessIPP()' - Process an IPP request for a scanner.
//
// Currently returns IPP_STATUS_ERROR_OPERATION_NOT_SUPPORTED for all
// operations.  This provides a clean extension point for future IPP Scan
// (PWG 5100.17) support without implementing unused functionality.
//

void
_papplScannerProcessIPP(
    pappl_client_t *client)		// I - Client connection
{
  (void)client;

  // For now, scanners do not support IPP operations.
  // All scanner interaction is via eSCL HTTP/XML endpoints.
  // This function exists as a placeholder for future IPP Scan support.
  papplClientRespond(client, HTTP_STATUS_BAD_REQUEST, NULL, NULL, 0, 0);
}
