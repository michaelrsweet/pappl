//
// Common client eSCL processing for the Printer Application Framework
//
// Copyright © 2019-2020 by Michael R Sweet.
// Copyright © 2010-2019 by Apple Inc.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

//
// Include necessary headers...
//

#include "pappl-private.h"
#include "scanner-xml.h"

//
// Protocol Constants - If HTTP 503 reply is received, how many retry attempts to perform before giving up
//

// ESCL_RETRY_ATTEMPTS_LOAD - for NextDocument request
// ESCL_RETRY_ATTEMPTS      - for other requests

#define ESCL_RETRY_ATTEMPTS_LOAD 30
#define ESCL_RETRY_ATTEMPTS 10

// ESCL_RETRY_PAUSE  - Pause between retries, in milliseconds

#define ESCL_RETRY_PAUSE 1000

// ESCL_NEXT_LOAD_DELAY     - delay between LOAD requests, milliseconds
// ESCL_NEXT_LOAD_DELAY_MAX - upper limit of this delay, as a fraction of a previous LOAD time

#define ESCL_NEXT_LOAD_DELAY 1000
#define ESCL_NEXT_LOAD_DELAY_MAX 0.5

//
// '_papplClientFlushDocumentData()' - Safely flush remaining document data.
//

void _papplClientFlushDocumentData(
    pappl_client_t *client) // I - Client
{
  char buffer[8192]; // Read buffer

  if (httpGetState(client->http) == HTTP_STATE_POST_RECV)
  {
    while (httpRead(client->http, buffer, sizeof(buffer)) > 0)
      ; // Read all data
  }
}

//
// '_papplClientHaveDocumentData()' - Determine whether we have more document data.
//

bool // O - `true` if data is present, `false` otherwise
_papplClientHaveDocumentData(
    pappl_client_t *client) // I - Client
{
  char temp; // Data

  if (httpGetState(client->http) != HTTP_STATE_POST_RECV)
    return (false);
  else
    return (httpPeek(client->http, &temp, 1) > 0);
}

//
// proto_handler_escl represents eSCL protocol handler
//

typedef struct
{
  proto_handler proto; /* Base class */

  /* Miscellaneous flags */
  bool quirk_localhost;          /* Set Host: localhost in ScanJobs rq */
  bool quirk_canon_mf410_series; /* Canon MF410 Series */
  bool quirk_port_in_host;       /* Always set port in Host: header */
} proto_handler_escl;

//
// XML namespace for XML writer
//

static const xml_ns escl_xml_wr_ns[] = {
    {"pwg", "http://www.pwg.org/schemas/2010/12/sm"},
    {"scan", "http://schemas.hp.com/imaging/escl/2011/05/03"},
    {NULL, NULL}};

//
// escl_scanner_status represents decoded ScannerStatus response
//

typedef struct
{
  SANE_Status device_status; /* <pwg:State>XXX</pwg:State> */
  SANE_Status adf_status;    /* <scan:AdfState>YYY</scan:AdfState> */
} escl_scanner_status;

//
// Forward Declaration
//

static error
escl_parse_scanner_status(const proto_ctx *ctx, const char *xml_text, size_t xml_len, escl_scanner_status *out);

//
// Functions for parsing different scanner capabilities
//

//
// escl_parse_color_modes - Parse color modes
//

static error
escl_parse_color_modes(xml_rd *xml, devcaps_source *src)
{
  src->colormodes = 0;

  xml_rd_enter(xml);
  for (; !xml_rd_end(xml); xml_rd_next(xml))
  {
    if (xml_rd_node_name_match(xml, "scan:ColorMode"))
    {
      const char *v = xml_rd_node_value(xml);
      if (!strcmp(v, "BlackAndWhite1"))
      {
        src->colormodes |= 1 << ID_COLORMODE_BW1;
      }
      else if (!strcmp(v, "Grayscale8"))
      {
        src->colormodes |= 1 << ID_COLORMODE_GRAYSCALE;
      }
      else if (!strcmp(v, "RGB24"))
      {
        src->colormodes |= 1 << ID_COLORMODE_COLOR;
      }
    }
  }
  xml_rd_leave(xml);

  return NULL;
}

//
// escl_parse_document_formats - Parse document formats
//

static error
escl_parse_document_formats(xml_rd *xml, devcaps_source *src)
{
  xml_rd_enter(xml);
  for (; !xml_rd_end(xml); xml_rd_next(xml))
  {
    unsigned int flags = 0;

    if (xml_rd_node_name_match(xml, "pwg:DocumentFormat"))
    {
      flags |= DEVCAPS_SOURCE_PWG_DOCFMT;
    }

    if (xml_rd_node_name_match(xml, "scan:DocumentFormatExt"))
    {
      flags |= DEVCAPS_SOURCE_SCAN_DOCFMT_EXT;
    }

    if (flags != 0)
    {
      const char *v = xml_rd_node_value(xml);
      ID_FORMAT fmt = id_format_by_mime_name(v);

      if (fmt != ID_FORMAT_UNKNOWN)
      {
        src->formats |= 1 << fmt;
        src->flags |= flags;
      }
    }
  }
  xml_rd_leave(xml);

  return NULL;
}

//
// escl_parse_discrete_resolutions - Parse discrete resolutions.
//

static error
escl_parse_discrete_resolutions(xml_rd *xml, devcaps_source *src)
{
  error err = NULL;

  sane_word_array_reset(&src->resolutions);

  xml_rd_enter(xml);
  for (; err == NULL && !xml_rd_end(xml); xml_rd_next(xml))
  {
    if (xml_rd_node_name_match(xml, "scan:DiscreteResolution"))
    {
      SANE_Word x = 0, y = 0;
      xml_rd_enter(xml);
      for (; err == NULL && !xml_rd_end(xml); xml_rd_next(xml))
      {
        if (xml_rd_node_name_match(xml, "scan:XResolution"))
        {
          err = xml_rd_node_value_uint(xml, &x);
        }
        else if (xml_rd_node_name_match(xml,
                                        "scan:YResolution"))
        {
          err = xml_rd_node_value_uint(xml, &y);
        }
      }
      xml_rd_leave(xml);

      if (x && y && x == y)
      {
        src->resolutions = sane_word_array_append(src->resolutions, x);
      }
    }
  }
  xml_rd_leave(xml);

  if (sane_word_array_len(src->resolutions) > 0)
  {
    src->flags |= DEVCAPS_SOURCE_RES_DISCRETE;
    sane_word_array_sort(src->resolutions);
  }

  return err;
}

//
// escl_parse_resolutions_range - Parse resolutions range
//

static error
escl_parse_resolutions_range(xml_rd *xml, devcaps_source *src)
{
  error err = NULL;
  SANE_Range range_x = {0, 0, 0}, range_y = {0, 0, 0};

  xml_rd_enter(xml);
  for (; err == NULL && !xml_rd_end(xml); xml_rd_next(xml))
  {
    SANE_Range *range = NULL;
    if (xml_rd_node_name_match(xml, "scan:XResolution"))
    {
      range = &range_x;
    }
    else if (xml_rd_node_name_match(xml, "scan:XResolution"))
    {
      range = &range_y;
    }

    if (range != NULL)
    {
      xml_rd_enter(xml);
      for (; err == NULL && !xml_rd_end(xml); xml_rd_next(xml))
      {
        if (xml_rd_node_name_match(xml, "scan:Min"))
        {
          err = xml_rd_node_value_uint(xml, &range->min);
        }
        else if (xml_rd_node_name_match(xml, "scan:Max"))
        {
          err = xml_rd_node_value_uint(xml, &range->max);
        }
        else if (xml_rd_node_name_match(xml, "scan:Step"))
        {
          err = xml_rd_node_value_uint(xml, &range->quant);
        }
      }
      xml_rd_leave(xml);
    }
  }
  xml_rd_leave(xml);

  if (range_x.min > range_x.max)
  {
    err = ERROR("Invalid scan:XResolution range");
    goto DONE;
  }

  if (range_y.min > range_y.max)
  {
    err = ERROR("Invalid scan:YResolution range");
    goto DONE;
  }

  /* If no quantization value, SANE uses 0, not 1
   */
  if (range_x.quant == 1)
  {
    range_x.quant = 0;
  }

  if (range_y.quant == 1)
  {
    range_y.quant = 0;
  }

  /* Try to merge x/y ranges */
  if (!math_range_merge(&src->res_range, &range_x, &range_y))
  {
    err = ERROR("Incompatible scan:XResolution and "
                "scan:YResolution ranges");
    goto DONE;
  }

  src->flags |= DEVCAPS_SOURCE_RES_RANGE;

DONE:
  return err;
}

//
// escl_parse_resolutions - Parse supported resolutions.
//

static error
escl_parse_resolutions(xml_rd *xml, devcaps_source *src)
{
  error err = NULL;

  xml_rd_enter(xml);
  for (; err == NULL && !xml_rd_end(xml); xml_rd_next(xml))
  {
    if (xml_rd_node_name_match(xml, "scan:DiscreteResolutions"))
    {
      err = escl_devcaps_source_parse_discrete_resolutions(xml, src);
    }
    else if (xml_rd_node_name_match(xml, "scan:ResolutionRange"))
    {
      err = escl_devcaps_source_parse_resolutions_range(xml, src);
    }
  }
  xml_rd_leave(xml);

  /* Prefer discrete resolution, if both are provided */
  if (src->flags & DEVCAPS_SOURCE_RES_DISCRETE)
  {
    src->flags &= ~DEVCAPS_SOURCE_RES_RANGE;
  }

  return err;
}

//
// escl_parse_setting_profiles - Parse setting profiles (color modes, document formats etc).
//

static error
escl_parse_setting_profiles(xml_rd *xml, devcaps_source *src)
{
  error err = NULL;

  /* Parse setting profiles */
  xml_rd_enter(xml);
  for (; err == NULL && !xml_rd_end(xml); xml_rd_next(xml))
  {
    if (xml_rd_node_name_match(xml, "scan:SettingProfile"))
    {
      xml_rd_enter(xml);
      for (; err == NULL && !xml_rd_end(xml); xml_rd_next(xml))
      {
        if (xml_rd_node_name_match(xml, "scan:ColorModes"))
        {
          err = escl_parse_color_modes(xml, src);
        }
        else if (xml_rd_node_name_match(xml,
                                        "scan:DocumentFormats"))
        {
          err = escl_parse_document_formats(xml, src);
        }
        else if (xml_rd_node_name_match(xml,
                                        "scan:SupportedResolutions"))
        {
          err = escl_parse_resolutions(xml, src);
        }
      }
      xml_rd_leave(xml);
    }
  }
  xml_rd_leave(xml);

  /* Validate results */
  if (err == NULL)
  {
    src->colormodes &= DEVCAPS_COLORMODES_SUPPORTED;
    if (src->colormodes == 0)
    {
      return ERROR("no color modes detected");
    }

    src->formats &= DEVCAPS_FORMATS_SUPPORTED;
    if (src->formats == 0)
    {
      return ERROR("no image formats detected");
    }

    if (!(src->flags & (DEVCAPS_SOURCE_RES_DISCRETE |
                        DEVCAPS_SOURCE_RES_RANGE)))
    {
      return ERROR("scan resolutions are not defined");
    }
  }

  return err;
}

//
// escl_parse_justification  - Parse ADF justification
//

static void
escl_parse_justification(xml_rd *xml,
                         ID_JUSTIFICATION *x, ID_JUSTIFICATION *y)
{
  xml_rd_enter(xml);

  *x = *y = ID_JUSTIFICATION_UNKNOWN;

  for (; !xml_rd_end(xml); xml_rd_next(xml))
  {
    if (xml_rd_node_name_match(xml, "pwg:XImagePosition"))
    {
      const char *v = xml_rd_node_value(xml);
      if (!strcmp(v, "Right"))
      {
        *x = ID_JUSTIFICATION_RIGHT;
      }
      else if (!strcmp(v, "Center"))
      {
        *x = ID_JUSTIFICATION_CENTER;
      }
      else if (!strcmp(v, "Left"))
      {
        *x = ID_JUSTIFICATION_LEFT;
      }
    }
    else if (xml_rd_node_name_match(xml, "pwg:YImagePosition"))
    {
      const char *v = xml_rd_node_value(xml);
      if (!strcmp(v, "Top"))
      {
        *y = ID_JUSTIFICATION_TOP;
      }
      else if (!strcmp(v, "Center"))
      {
        *y = ID_JUSTIFICATION_CENTER;
      }
      else if (!strcmp(v, "Bottom"))
      {
        *y = ID_JUSTIFICATION_BOTTOM;
      }
    }
  }
  xml_rd_leave(xml);
}

//
// escl_source_parse - Parse source capabilities. Returns NULL on success, error string otherwise
//

static error
escl_source_parse(xml_rd *xml, devcaps_source **out)
{
  devcaps_source *src = devcaps_source_new();
  error err = NULL;

  xml_rd_enter(xml);
  for (; err == NULL && !xml_rd_end(xml); xml_rd_next(xml))
  {
    if (xml_rd_node_name_match(xml, "scan:MinWidth"))
    {
      err = xml_rd_node_value_uint(xml, &src->min_wid_px);
    }
    else if (xml_rd_node_name_match(xml, "scan:MaxWidth"))
    {
      err = xml_rd_node_value_uint(xml, &src->max_wid_px);
    }
    else if (xml_rd_node_name_match(xml, "scan:MinHeight"))
    {
      err = xml_rd_node_value_uint(xml, &src->min_hei_px);
    }
    else if (xml_rd_node_name_match(xml, "scan:MaxHeight"))
    {
      err = xml_rd_node_value_uint(xml, &src->max_hei_px);
    }
    else if (xml_rd_node_name_match(xml, "scan:SettingProfiles"))
    {
      err = escl_parse_setting_profiles(xml, src);
    }
  }
  xml_rd_leave(xml);

  if (err != NULL)
  {
    goto DONE;
  }

  if (src->max_wid_px != 0 && src->max_hei_px != 0)
  {
    /* Validate window size */
    if (src->min_wid_px > src->max_wid_px)
    {
      err = ERROR("Invalid scan:MinWidth or scan:MaxWidth");
      goto DONE;
    }

    if (src->min_hei_px > src->max_hei_px)
    {
      err = ERROR("Invalid scan:MinHeight or scan:MaxHeight");
      goto DONE;
    }

    src->flags |= DEVCAPS_SOURCE_HAS_SIZE;

    /* Set window ranges */
    src->win_x_range_mm.min = src->win_y_range_mm.min = 0;
    src->win_x_range_mm.max = math_px2mm_res(src->max_wid_px, 300);
    src->win_y_range_mm.max = math_px2mm_res(src->max_hei_px, 300);
  }

DONE:
  if (err != NULL)
  {
    devcaps_source_free(src);
  }
  else
  {
    if (*out == NULL)
    {
      *out = src;
    }
    else
    {
      /* Duplicate detected. Ignored for now */
      devcaps_source_free(src);
    }
  }

  return err;
}

//
// escl_compression_parse - Parse compression factor parameters
//

static error
escl_compression_parse(xml_rd *xml, devcaps *caps)
{
  for (; !xml_rd_end(xml); xml_rd_next(xml))
  {
    error err = NULL;

    if (xml_rd_node_name_match(xml, "scan:Min"))
    {
      err = xml_rd_node_value_uint(xml, &caps->compression_range.min);
    }
    else if (xml_rd_node_name_match(xml, "scan:Max"))
    {
      err = xml_rd_node_value_uint(xml, &caps->compression_range.max);
    }
    else if (xml_rd_node_name_match(xml, "scan:Step"))
    {
      err = xml_rd_node_value_uint(xml, &caps->compression_range.quant);
    }
    else if (xml_rd_node_name_match(xml, "scan:Normal"))
    {
      err = xml_rd_node_value_uint(xml, &caps->compression_norm);
    }

    if (err != NULL)
    {
      return err;
    }
  }

  // Validate obtained parameters.
  if (caps->compression_range.min > caps->compression_range.max)
  {
    return NULL;
  }

  if (caps->compression_norm < caps->compression_range.min ||
      caps->compression_norm > caps->compression_range.max)
  {
    return NULL;
  }

  caps->compression_ok = true;

  return NULL;
}

//
// escl_device_parse - Parse device capabilities. devcaps structure must be initialized before calling this function.
//

static error
escl_device_parse(proto_handler_escl *escl,
                  devcaps *caps, const char *xml_text, size_t xml_len)
{
  error err = NULL;
  xml_rd *xml;
  bool quirk_canon_iR2625_2630 = false;
  ID_SOURCE id_src;
  bool src_ok = false;

  /* Parse capabilities XML */
  err = xml_rd_begin(&xml, xml_text, xml_len, NULL);
  if (err != NULL)
  {
    goto DONE;
  }

  if (!xml_rd_node_name_match(xml, "scan:ScannerCapabilities"))
  {
    err = ERROR("XML: missed scan:ScannerCapabilities");
    goto DONE;
  }

  xml_rd_enter(xml);
  for (; !xml_rd_end(xml); xml_rd_next(xml))
  {
    if (xml_rd_node_name_match(xml, "pwg:MakeAndModel"))
    {
      const char *m = xml_rd_node_value(xml);

      if (!strcmp(m, "Canon iR2625/2630"))
      {
        quirk_canon_iR2625_2630 = true;
      }
      else if (!strcmp(m, "HP LaserJet MFP M630"))
      {
        escl->quirk_localhost = true;
      }
      else if (!strcmp(m, "HP Color LaserJet FlowMFP M578"))
      {
        escl->quirk_localhost = true;
      }
      else if (!strcmp(m, "MF410 Series"))
      {
        escl->quirk_canon_mf410_series = true;
      }
      else if (!strncasecmp(m, "EPSON ", 6))
      {
        escl->quirk_port_in_host = true;
      }
    }
    else if (xml_rd_node_name_match(xml, "scan:Manufacturer"))
    {
      const char *m = xml_rd_node_value(xml);

      if (!strcasecmp(m, "EPSON"))
      {
        escl->quirk_port_in_host = true;
      }
    }
    else if (xml_rd_node_name_match(xml, "scan:Platen"))
    {
      xml_rd_enter(xml);
      if (xml_rd_node_name_match(xml, "scan:PlatenInputCaps"))
      {
        err = escl_source_parse(xml,
                                &caps->src[ID_SOURCE_PLATEN]);
      }
      xml_rd_leave(xml);
    }
    else if (xml_rd_node_name_match(xml, "scan:Adf"))
    {
      xml_rd_enter(xml);
      while (!xml_rd_end(xml))
      {
        if (xml_rd_node_name_match(xml, "scan:AdfSimplexInputCaps"))
        {
          err = escl_source_parse(xml,
                                  &caps->src[ID_SOURCE_ADF_SIMPLEX]);
        }
        else if (xml_rd_node_name_match(xml,
                                        "scan:AdfDuplexInputCaps"))
        {
          err = escl_source_parse(xml,
                                  &caps->src[ID_SOURCE_ADF_DUPLEX]);
        }
        else if (xml_rd_node_name_match(xml, "scan:Justification"))
        {
          escl_parse_justification(xml,
                                   &caps->justification_x, &caps->justification_y);
        }
        xml_rd_next(xml);
      }
      xml_rd_leave(xml);
    }
    else if (xml_rd_node_name_match(xml, "scan:CompressionFactorSupport"))
    {
      xml_rd_enter(xml);
      err = escl_compression_parse(xml, caps);
      xml_rd_leave(xml);
    }

    if (err != NULL)
    {
      goto DONE;
    }
  }

  /* Check that we have at least one source */
  for (id_src = (ID_SOURCE)0; id_src < NUM_ID_SOURCE; id_src++)
  {
    if (caps->src[id_src] != NULL)
    {
      src_ok = true;
    }
  }

  if (!src_ok)
  {
    err = ERROR("XML: neither platen nor ADF sources detected");
    goto DONE;
  }

  /* Apply quirks, if any */
  if (quirk_canon_iR2625_2630)
  {

    for (id_src = (ID_SOURCE)0; id_src < NUM_ID_SOURCE; id_src++)
    {
      devcaps_source *src = caps->src[id_src];
      if (src != NULL &&
          /* paranoia: array won't be empty after quirk applied */
          sane_word_array_len(src->resolutions) > 0 &&
          src->resolutions[1] <= 300)
      {
        sane_word_array_bound(src->resolutions, 0, 300);
      }
    }
  }

DONE:
  if (err != NULL)
  {
    devcaps_reset(caps);
  }

  xml_rd_finish(&xml);

  return err;
}
