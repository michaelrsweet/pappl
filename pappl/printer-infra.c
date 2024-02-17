//
// Infrastructure printer functions for the Printer Application Framework
//
// Copyright © 2024 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#include "printer-private.h"
#include "system-private.h"


//
// Local functions...
//

static void	delete_infra(pappl_printer_t *printer, pappl_pr_driver_data_t *data);
static char	*get_string(cups_array_t *strings, const char *s);


//
// '_papplPrinterUpdateInfra()' - Update the capabilities of an infrastructure printer based on its output devices.
//

void
_papplPrinterUpdateInfra(
    pappl_printer_t *printer)		// I - Printer
{
  pappl_pr_driver_data_t data;		// Printer driver data
  cups_array_t		*strings;	// Strings array
  _pappl_odevice_t	*od;		// Current output device
  size_t		odidx,		// Current output device index
			odcount;	// Number of output devices
  ipp_attribute_t	*attr;		// Current attribute
  size_t		i, j,		// Looping vars
			count;		// Number of values
  const char		*svalue;	// String value
  int			ivalue;		// Integer/enum value
  int			ppm = 0,	// Pages per minute, monochrome
			ppm_color = 0;	// Pages per minute, color


  // Get the driver information...
  papplPrinterGetDriverData(printer, &data);

  // Update callbacks/data
  data.delete_cb = (pappl_pr_delete_cb_t)delete_infra;

  if ((strings = (cups_array_t *)data.extension) == NULL)
    data.extension = strings = cupsArrayNewStrings(NULL, '\0');

  // Reset capabilities...
  data.kind                     = 0;
  data.color_supported          = 0;
  data.raster_types             = 0;
  data.duplex                   = PAPPL_DUPLEX_NONE;
  data.sides_supported          = PAPPL_SIDES_ONE_SIDED;
  data.finishings_supported     = PAPPL_FINISHINGS_NONE;
  data.num_resolution           = 0;
  data.borderless               = true;
  data.left_right               = 1;
  data.bottom_top               = 1;
  data.num_media                = 0;
  data.num_source               = 0;
  data.left_offset_supported[0] = 0;
  data.left_offset_supported[1] = 0;
  data.top_offset_supported[0]  = 0;
  data.top_offset_supported[1]  = 0;
  data.tracking_supported       = 0;
  data.num_type                 = 0;
  data.num_bin                  = 0;
  data.mode_supported           = 0;
  data.tear_offset_supported[0] = 0;
  data.tear_offset_supported[1] = 0;
  data.speed_supported[0]       = 0;
  data.speed_supported[1]       = 0;
  data.darkness_supported       = 0;
  data.identify_supported       = 0;
  data.num_features             = 0;

  // Scan each of the output devices...
  cupsRWLockRead(&printer->output_rwlock);
  for (odidx = 0, odcount = cupsArrayGetCount(printer->output_devices); odidx < odcount; odidx ++)
  {
    // Get the output device attributes...
    od = (_pappl_odevice_t *)cupsArrayGetElement(printer->output_devices, odidx);

    if (!od->device_attrs)
      continue;

    // Merge capabilities
    if ((attr = ippFindAttribute(od->device_attrs, "finishings-supported", IPP_TAG_ENUM)) != NULL)
    {
      // finishings
      for (i = 0, count = ippGetCount(attr); i < count; i ++)
      {
        switch (ippGetInteger(attr, i))
        {
          case IPP_FINISHINGS_PUNCH :
              data.finishings_supported |= PAPPL_FINISHINGS_PUNCH;
              break;
          case IPP_FINISHINGS_STAPLE :
              data.finishings_supported |= PAPPL_FINISHINGS_STAPLE;
              break;
          case IPP_FINISHINGS_TRIM :
              data.finishings_supported |= PAPPL_FINISHINGS_TRIM;
              break;
          case IPP_FINISHINGS_BOOKLET_MAKER :
              data.finishings_supported |= PAPPL_FINISHINGS_BOOKLET_MAKER;
              break;
          case IPP_FINISHINGS_FOLD_DOUBLE_GATE :
              data.finishings_supported |= PAPPL_FINISHINGS_FOLD_DOUBLE_GATE;
              break;
          case IPP_FINISHINGS_FOLD_HALF :
              data.finishings_supported |= PAPPL_FINISHINGS_FOLD_HALF;
              break;
          case IPP_FINISHINGS_FOLD_LETTER :
              data.finishings_supported |= PAPPL_FINISHINGS_FOLD_LETTER;
              break;
          case IPP_FINISHINGS_FOLD_PARALLEL :
              data.finishings_supported |= PAPPL_FINISHINGS_FOLD_PARALLEL;
              break;
          case IPP_FINISHINGS_FOLD_Z :
              data.finishings_supported |= PAPPL_FINISHINGS_FOLD_Z;
              break;
          case IPP_FINISHINGS_PUNCH_DUAL_LEFT :
              data.finishings_supported |= PAPPL_FINISHINGS_PUNCH_DUAL_LEFT;
              break;
          case IPP_FINISHINGS_PUNCH_DUAL_TOP :
              data.finishings_supported |= PAPPL_FINISHINGS_PUNCH_DUAL_TOP;
              break;
          case IPP_FINISHINGS_PUNCH_TRIPLE_LEFT :
              data.finishings_supported |= PAPPL_FINISHINGS_PUNCH_TRIPLE_LEFT;
              break;
          case IPP_FINISHINGS_PUNCH_TRIPLE_TOP :
              data.finishings_supported |= PAPPL_FINISHINGS_PUNCH_TRIPLE_TOP;
              break;
          case IPP_FINISHINGS_PUNCH_MULTIPLE_LEFT :
              data.finishings_supported |= PAPPL_FINISHINGS_PUNCH_MULTIPLE_LEFT;
              break;
          case IPP_FINISHINGS_PUNCH_MULTIPLE_TOP :
              data.finishings_supported |= PAPPL_FINISHINGS_PUNCH_MULTIPLE_TOP;
              break;
          case IPP_FINISHINGS_SADDLE_STITCH :
              data.finishings_supported |= PAPPL_FINISHINGS_SADDLE_STITCH;
              break;
          case IPP_FINISHINGS_STAPLE_TOP_LEFT :
              data.finishings_supported |= PAPPL_FINISHINGS_STAPLE_TOP_LEFT;
              break;
          case IPP_FINISHINGS_STAPLE_BOTTOM_LEFT :
              data.finishings_supported |= PAPPL_FINISHINGS_STAPLE_BOTTOM_LEFT;
              break;
          case IPP_FINISHINGS_STAPLE_TOP_RIGHT :
              data.finishings_supported |= PAPPL_FINISHINGS_STAPLE_TOP_RIGHT;
              break;
          case IPP_FINISHINGS_STAPLE_BOTTOM_RIGHT :
              data.finishings_supported |= PAPPL_FINISHINGS_STAPLE_BOTTOM_RIGHT;
              break;
          case IPP_FINISHINGS_STAPLE_DUAL_LEFT :
              data.finishings_supported |= PAPPL_FINISHINGS_STAPLE_DUAL_LEFT;
              break;
          case IPP_FINISHINGS_STAPLE_DUAL_TOP :
              data.finishings_supported |= PAPPL_FINISHINGS_STAPLE_DUAL_TOP;
              break;
	}
      }
    }

    if ((attr = ippFindAttribute(od->device_attrs, "identify-actions-supported", IPP_TAG_KEYWORD)) != NULL)
    {
      // identify-actions
      for (i = 0, count = ippGetCount(attr); i < count; i ++)
      {
        if ((svalue = ippGetString(attr, i, NULL)) == NULL)
          continue;

        data.identify_supported |= _papplIdentifyActionsValue(svalue);
      }
    }

    if ((attr = ippFindAttribute(od->device_attrs, "ipp-features-supported", IPP_TAG_KEYWORD)) != NULL)
    {
      // ipp-features-supported
      for (i = 0, count = ippGetCount(attr); i < count; i ++)
      {
        if ((svalue = ippGetString(attr, i, NULL)) == NULL)
          continue;

	for (j = 0; j < data.num_features; j ++)
	{
	  if (!strcmp(svalue, data.features[j]))
	    break;
	}

	if (j >= data.num_features && data.num_features < PAPPL_MAX_VENDOR)
	{
	  data.features[data.num_features] = get_string(strings, svalue);
	  data.num_features ++;
	}
      }
    }

    if ((attr = ippFindAttribute(od->device_attrs, "label-mode-supported", IPP_TAG_KEYWORD)) != NULL)
    {
      // label-mode
      for (i = 0, count = ippGetCount(attr); i < count; i ++)
      {
        if ((svalue = ippGetString(attr, i, NULL)) == NULL)
          continue;

        data.mode_supported |= _papplLabelModeValue(svalue);
      }
    }

    if ((attr = ippFindAttribute(od->device_attrs, "label-tear-offset-supported", IPP_TAG_RANGE)))
    {
      // label-tear-offset
      int	lower, upper;		// Range values

      lower = ippGetRange(attr, 0, &upper);

      if (lower < data.tear_offset_supported[0])
	data.tear_offset_supported[0] = lower;
      if (upper > data.tear_offset_supported[1])
	data.tear_offset_supported[1] = upper;
    }

    if ((attr = ippFindAttribute(od->device_attrs, "media-supported", IPP_TAG_ZERO)) != NULL)
    {
      // media
      for (i = 0, count = ippGetCount(attr); i < count; i ++)
      {
        if ((svalue = ippGetString(attr, i, NULL)) == NULL)
          continue;

        for (j = 0; j < data.num_media; j ++)
        {
          if (!strcmp(svalue, data.media[j]))
            break;
	}

	if (j >= data.num_media && data.num_media < PAPPL_MAX_MEDIA)
	{
	  data.media[data.num_media] = get_string(strings, svalue);
          data.num_media ++;
	}
      }
    }

    if ((attr = ippFindAttribute(od->device_attrs, "media-bottom-margin-supported", IPP_TAG_INTEGER)) != NULL)
    {
      // media-bottom-margin
      data.borderless &= ippContainsInteger(attr, 0);

      for (i = 0, count = ippGetCount(attr); i < count; i ++)
      {
        if ((ivalue = ippGetInteger(attr, i)) > data.bottom_top)
          data.bottom_top = ivalue;
      }
    }

    if ((attr = ippFindAttribute(od->device_attrs, "media-left-margin-supported", IPP_TAG_INTEGER)) != NULL)
    {
      // media-left-margin
      data.borderless &= ippContainsInteger(attr, 0);

      for (i = 0, count = ippGetCount(attr); i < count; i ++)
      {
        if ((ivalue = ippGetInteger(attr, i)) > data.left_right)
          data.left_right = ivalue;
      }
    }

    if ((attr = ippFindAttribute(od->device_attrs, "media-right-margin-supported", IPP_TAG_INTEGER)) != NULL)
    {
      // media-right-margin
      data.borderless &= ippContainsInteger(attr, 0);

      for (i = 0, count = ippGetCount(attr); i < count; i ++)
      {
        if ((ivalue = ippGetInteger(attr, i)) > data.left_right)
          data.left_right = ivalue;
      }
    }

    if ((attr = ippFindAttribute(od->device_attrs, "media-source-supported", IPP_TAG_ZERO)) != NULL)
    {
      // media-source
      for (i = 0, count = ippGetCount(attr); i < count; i ++)
      {
        if ((svalue = ippGetString(attr, i, NULL)) == NULL)
          continue;

        for (j = 0; j < data.num_source; j ++)
        {
          if (!strcmp(svalue, data.source[j]))
            break;
	}

	if (j >= data.num_source && data.num_source < PAPPL_MAX_SOURCE)
	{
	  data.source[data.num_source] = get_string(strings, svalue);
          data.num_source ++;
	}
      }
    }

    if ((attr = ippFindAttribute(od->device_attrs, "media-top-margin-supported", IPP_TAG_INTEGER)) != NULL)
    {
      // media-top-margin
      data.borderless &= ippContainsInteger(attr, 0);

      for (i = 0, count = ippGetCount(attr); i < count; i ++)
      {
        if ((ivalue = ippGetInteger(attr, i)) > data.bottom_top)
          data.bottom_top = ivalue;
      }
    }

    if ((attr = ippFindAttribute(od->device_attrs, "media-tracking-supported", IPP_TAG_KEYWORD)) != NULL)
    {
      // media-tracking
      for (i = 0, count = ippGetCount(attr); i < count; i ++)
      {
        if ((svalue = ippGetString(attr, i, NULL)) == NULL)
          continue;
	data.tracking_supported |= _papplMediaTrackingValue(svalue);
      }
    }

    if ((attr = ippFindAttribute(od->device_attrs, "media-type-supported", IPP_TAG_ZERO)) != NULL)
    {
      // media-typs
      for (i = 0, count = ippGetCount(attr); i < count; i ++)
      {
        if ((svalue = ippGetString(attr, i, NULL)) == NULL)
          continue;

        for (j = 0; j < data.num_type; j ++)
        {
          if (!strcmp(svalue, data.type[j]))
            break;
	}

	if (j >= data.num_type && data.num_type < PAPPL_MAX_TYPE)
	{
	  data.type[data.num_type] = get_string(strings, svalue);
          data.num_type ++;
	}
      }
    }

    if ((attr = ippFindAttribute(od->device_attrs, "output-bin-supported", IPP_TAG_ZERO)) != NULL)
    {
      // output-bin
      for (i = 0, count = ippGetCount(attr); i < count; i ++)
      {
        if ((svalue = ippGetString(attr, i, NULL)) == NULL)
          continue;

        for (j = 0; j < data.num_bin; j ++)
        {
          if (!strcmp(svalue, data.bin[j]))
            break;
	}

	if (j >= data.num_bin && data.num_media < PAPPL_MAX_BIN)
	{
	  data.bin[data.num_bin] = get_string(strings, svalue);
          data.num_bin ++;
	}
      }
    }

    if ((attr = ippFindAttribute(od->device_attrs, "pages-per-minute", IPP_TAG_INTEGER)) != NULL)
    {
      // pages-per-minute
      ppm += ippGetInteger(attr, 0);
    }

    if ((attr = ippFindAttribute(od->device_attrs, "pages-per-minute-color", IPP_TAG_INTEGER)) != NULL)
    {
      // pages-per-minute-color
      ppm_color += ippGetInteger(attr, 0);
    }

    if ((attr = ippFindAttribute(od->device_attrs, "print-color-mode-supported", IPP_TAG_KEYWORD)) != NULL)
    {
      // print-color-mode
      for (i = 0, count = ippGetCount(attr); i < count; i ++)
      {
        if ((svalue = ippGetString(attr, i, NULL)) == NULL)
          continue;

        data.color_supported |= _papplColorModeValue(svalue);
      }
    }

    if ((attr = ippFindAttribute(od->device_attrs, "print-darkness-supported", IPP_TAG_INTEGER)))
    {
      if ((ivalue = ippGetInteger(attr, 0)) > data.darkness_supported)
        data.darkness_supported = ivalue;
    }

    if ((attr = ippFindAttribute(od->device_attrs, "print-speed-supported", IPP_TAG_RANGE)))
    {
      int	lower, upper;		// Range values

      for (i = 0, count = ippGetCount(attr); i < count; i ++)
      {
        lower = ippGetRange(attr, i, &upper);

        if (lower < data.speed_supported[0])
          data.speed_supported[0] = lower;
	if (upper > data.speed_supported[1])
	  data.speed_supported[1] = upper;
      }
    }
    else if ((attr = ippFindAttribute(od->device_attrs, "print-speed-supported", IPP_TAG_INTEGER)))
    {
      for (i = 0, count = ippGetCount(attr); i < count; i ++)
      {
        ivalue = ippGetInteger(attr, i);

        if (ivalue < data.speed_supported[0])
          data.speed_supported[0] = ivalue;
	if (ivalue > data.speed_supported[1])
	  data.speed_supported[1] = ivalue;
      }
    }

    if ((attr = ippFindAttribute(od->device_attrs, "pwg-raster-document-resolution-supported", IPP_TAG_RESOLUTION)) != NULL)
    {
      // pwg-raster-document-resolution
      int	xres, yres;		// X and Y resolution
      ipp_res_t	units;			// Resolution units

      for (i = 0, count = ippGetCount(attr); i < count; i ++)
      {
        xres = ippGetResolution(attr, i, &yres, &units);

        if (units == IPP_RES_PER_CM)
        {
          xres = (int)(2.54 * xres);
          yres = (int)(2.54 * yres);
        }

        for (j = 0; j < data.num_resolution; j ++)
        {
          if (xres == data.x_resolution[j] && yres == data.y_resolution[i])
            break;
        }

        if (j >= data.num_resolution && data.num_resolution < PAPPL_MAX_RESOLUTION)
        {
          data.x_resolution[data.num_resolution] = xres;
          data.y_resolution[data.num_resolution] = yres;
          data.num_resolution ++;
        }
      }
    }

    if ((attr = ippFindAttribute(od->device_attrs, "pwg-raster-document-sheet-back", IPP_TAG_KEYWORD)) != NULL)
    {
      // pwg-raster-document-sheet-back
      if ((svalue = ippGetString(attr, 0, NULL)) != NULL)
      {
        if (!strcmp(svalue, "normal"))
          data.duplex = PAPPL_DUPLEX_NORMAL;
	else if (!strcmp(svalue, "flipped"))
          data.duplex = PAPPL_DUPLEX_FLIPPED;
	else if (!strcmp(svalue, "rotated"))
          data.duplex = PAPPL_DUPLEX_ROTATED;
	else if (!strcmp(svalue, "manual-tumble"))
          data.duplex = PAPPL_DUPLEX_MANUAL_TUMBLE;
      }
    }

    if ((attr = ippFindAttribute(od->device_attrs, "pwg-raster-document-type-supported", IPP_TAG_KEYWORD)) != NULL)
    {
      // pwg-raster-document-type
      for (i = 0, count = ippGetCount(attr); i < count; i ++)
      {
        if ((svalue = ippGetString(attr, i, NULL)) == NULL)
          continue;

        data.raster_types |= _papplRasterTypeValue(svalue);
      }
    }

    if ((attr = ippFindAttribute(od->device_attrs, "sides-supported", IPP_TAG_KEYWORD)) != NULL)
    {
      // sides
      if (ippContainsString(attr, "two-sided-long-edge"))
        data.sides_supported |= PAPPL_SIDES_TWO_SIDED_LONG_EDGE;
      if (ippContainsString(attr, "two-sided-short-edge"))
        data.sides_supported |= PAPPL_SIDES_TWO_SIDED_SHORT_EDGE;
    }

    if ((attr = ippFindAttribute(od->device_attrs, "urf-supported", IPP_TAG_KEYWORD)) != NULL)
    {
      // urf-supported
      for (i = 0, count = ippGetCount(attr); i < count; i ++)
      {
        if ((svalue = ippGetString(attr, i, NULL)) == NULL)
          continue;

        if (!strcmp(svalue, "W8"))
        {
          data.raster_types |= PAPPL_RASTER_TYPE_SGRAY_8;
	}
	else if (!strcmp(svalue, "SRGB24"))
        {
          data.raster_types |= PAPPL_RASTER_TYPE_SRGB_8;
	}
	else if (!strcmp(svalue, "ADOBERGB24"))
        {
          data.raster_types |= PAPPL_RASTER_TYPE_ADOBE_RGB_8;
	}
	else if (!strcmp(svalue, "DM1"))
        {
          data.duplex = PAPPL_DUPLEX_NORMAL;
	}
	else if (!strcmp(svalue, "DM2"))
        {
          data.duplex = PAPPL_DUPLEX_FLIPPED;
	}
	else if (!strcmp(svalue, "DM3"))
        {
          data.duplex = PAPPL_DUPLEX_ROTATED;
	}
	else if (!strcmp(svalue, "DM4"))
	{
          data.duplex = PAPPL_DUPLEX_MANUAL_TUMBLE;
	}
	else if (!strncmp(svalue, "RS", 2))
	{
	  // Resolutions
	  char	*ptr;		// Pointer into value

          for (ptr = (char *)svalue + 2; ptr && *ptr;)
          {
            ivalue = (int)strtol(ptr, &ptr, 10);
            if (ptr && *ptr == '-')
              ptr ++;

	    for (j = 0; j < data.num_resolution; j ++)
	    {
	      if (ivalue == data.x_resolution[j] && ivalue == data.y_resolution[i])
		break;
	    }

	    if (j >= data.num_resolution && data.num_resolution < PAPPL_MAX_RESOLUTION)
	    {
	      data.x_resolution[data.num_resolution] = ivalue;
	      data.y_resolution[data.num_resolution] = ivalue;
	      data.num_resolution ++;
	    }
          }
	}
      }
    }
  }
  cupsRWUnlock(&printer->output_rwlock);

  if (odcount > 0)
  {
    ppm       = (ppm + (int)odcount - 1) / (int)odcount;
    ppm_color = (ppm_color + (int)odcount - 1) / (int)odcount;
  }

  if (ppm > 0)
    data.ppm = ppm;
  else
    data.ppm = 1;

  data.ppm_color = ppm_color;

  // Normalize the defaults and capabilities...
  if (data.num_media == 0)
  {
    data.num_media = 2;
    data.media[0]  = get_string(strings, "na_letter_8.5x11in");
    data.media[1]  = get_string(strings, "iso_a4_210x297mm");
  }

  if (data.num_source == 0)
  {
    data.num_source = 1;
    data.source[0]  = get_string(strings, "auto");
  }

  if (data.num_type == 0)
  {
    data.num_type = 1;
    data.type[0]  = get_string(strings, "stationery");
  }

  if (data.num_resolution == 0)
  {
    data.num_resolution  = 1;
    data.x_resolution[0] = 300;
    data.y_resolution[0] = 300;
  }

  if (!data.raster_types)
    data.raster_types = PAPPL_RASTER_TYPE_SGRAY_8;

  for (i = 0; i < data.num_source; i ++)
  {
    if (!data.media_ready[i].size_name[0])
    {
      pwg_media_t *pwg = pwgMediaForPWG(data.media[0]);
					// PWG media information

      cupsCopyString(data.media_ready[i].size_name, data.media[0], sizeof(data.media_ready[i].size_name));
      cupsCopyString(data.media_ready[i].source, data.source[0], sizeof(data.media_ready[i].source));
      cupsCopyString(data.media_ready[i].type, data.type[0], sizeof(data.media_ready[i].type));

      data.media_ready[i].size_width    = pwg ? pwg->width : 21590;
      data.media_ready[i].size_length   = pwg ? pwg->length : 27940;
      data.media_ready[i].left_margin   = data.left_right;
      data.media_ready[i].right_margin  = data.left_right;
      data.media_ready[i].bottom_margin = data.bottom_top;
      data.media_ready[i].top_margin    = data.bottom_top;
    }
  }

  if (!data.media_default.size_name[0])
    data.media_default = data.media_ready[0];

  // Save the new values...
  papplPrinterSetDriverData(printer, &data, /*attrs*/NULL);
}


//
// 'delete_infra()' - Delete an infrastructure printer.
//

static void
delete_infra(
    pappl_printer_t        *printer,	// I - Printer
    pappl_pr_driver_data_t *data)	// I - Printer driver data
{
  (void)printer;

  cupsArrayDelete((cups_array_t *)data->extension);
}


//
// 'get_string()' - Get a string from the strings array.
//

static char *				// O - String from array
get_string(cups_array_t *strings,	// I - Strings array
           const char   *s)		// I - String
{
  char	*news;				// New string


  // See if the string is already in the array...
  if ((news = (char *)cupsArrayFind(strings, (void *)s)) == NULL)
  {
    // No, add it...
    cupsArrayAdd(strings, (void *)s);
    news = (char *)cupsArrayFind(strings, (void *)s);
  }

  return (news);
}
