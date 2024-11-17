#include "scanner-private.h"
#include "client-private.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>

// _papplScannerReasonString() - Convert a scanner state reason value to a string.
const char *
_papplScannerReasonString(pappl_sreason_t reason)
{
  switch (reason)
  {
    case PAPPL_SREASON_NONE:
        return "none";
    case PAPPL_SREASON_IDLE:
        return "idle";
    case PAPPL_SREASON_PROCESSING:
        return "processing";
    case PAPPL_SREASON_TESTING:
        return "testing";
    case PAPPL_SREASON_STOPPED:
        return "stopped";
    case PAPPL_SREASON_DOWN:
        return "down";
    default:
        return "unknown";
  }
}

void
_papplScannerCopyStateNoLock(
    pappl_scanner_t *scanner,   // I - Scanner
    ipp_tag_t       group_tag,  // I - Group tag
    ipp_t           *ipp,       // I - IPP message
    pappl_client_t  *client,    // I - Client connection
    cups_array_t    *ra)        // I - Requested attributes
{
  if (!ra || cupsArrayFind(ra, "scanner-is-accepting-jobs"))
    ippAddBoolean(ipp, group_tag, "scanner-is-accepting-jobs", scanner->is_accepting);

  if (!ra || cupsArrayFind(ra, "scanner-state"))
    ippAddInteger(ipp, group_tag, IPP_TAG_ENUM, "scanner-state", (int)scanner->state);

  if (!ra || cupsArrayFind(ra, "scanner-state-message"))
  {
    static const char * const messages[] = { "Idle.", "Scanning.", "Stopped." };

    ippAddString(ipp, group_tag, IPP_CONST_TAG(IPP_TAG_TEXT), "scanner-state-message", NULL, messages[scanner->state - ESCL_SSTATE_IDLE]);
  }

  if (!ra || cupsArrayFind(ra, "scanner-state-reasons"))
  {
    ipp_attribute_t *attr = NULL; // scanner-state-reasons

    if (scanner->state_reasons == PAPPL_SREASON_NONE)
    {
      if (scanner->is_stopped)
        attr = ippAddString(ipp, group_tag, IPP_CONST_TAG(IPP_TAG_KEYWORD), "scanner-state-reasons", NULL, "moving-to-paused");
      else if (scanner->state == ESCL_SSTATE_STOPPED)
        attr = ippAddString(ipp, group_tag, IPP_CONST_TAG(IPP_TAG_KEYWORD), "scanner-state-reasons", NULL, "paused");

      if (!attr)
        ippAddString(ipp, group_tag, IPP_CONST_TAG(IPP_TAG_KEYWORD), "scanner-state-reasons", NULL, "none");
    }
    else
    {
      pappl_sreason_t bit; // Reason bit

      for (bit = PAPPL_SREASON_IDLE; bit <= PAPPL_SREASON_DOWN; bit *= 2)
      {
        if (scanner->state_reasons & bit)
        {
          if (attr)
            ippSetString(ipp, &attr, ippGetCount(attr), _papplScannerReasonString(bit));
          else
            attr = ippAddString(ipp, group_tag, IPP_CONST_TAG(IPP_TAG_KEYWORD), "scanner-state-reasons", NULL, _papplScannerReasonString(bit));
        }
      }

      if (scanner->is_stopped)
        ippSetString(ipp, &attr, ippGetCount(attr), "moving-to-paused");
      else if (scanner->state == ESCL_SSTATE_STOPPED)
        ippSetString(ipp, &attr, ippGetCount(attr), "paused");
    }
  }

  ippAddInteger(ipp, group_tag, IPP_TAG_INTEGER, "scanner-state-change-time", (int)(scanner->state_time));
  ippAddInteger(ipp, group_tag, IPP_TAG_INTEGER, "scanner-up-time", (int)(time(NULL) - scanner->start_time));
}
