//
// Contact functions for the Printer Application Framework
//
// Copyright © 2019-2020 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

//
// Include necessary headers...
//

#include "base-private.h"


//
// '_papplContactExport()' - Export contact information to an "xxx-contact-col" value.
//

ipp_t *					// O - "xxx-contact-col" value
_papplContactExport(
    pappl_contact_t *contact)		// I - Contact information
{
  ipp_t	*col = ippNew();		// "xxx-contact-col" value
  char	uri[1024];			// "contact-uri" value


  ippAddString(col, IPP_TAG_ZERO, IPP_TAG_NAME, "contact-name", NULL, contact->name);

  if (contact->email[0])
    httpAssembleURI(HTTP_URI_CODING_ALL, uri, sizeof(uri), "mailto", NULL, contact->email, 0, NULL);
  else
    httpAssembleURI(HTTP_URI_CODING_ALL, uri, sizeof(uri), "tel", NULL, contact->telephone, 0, NULL);
  ippAddString(col, IPP_TAG_ZERO, IPP_TAG_URI, "contact-uri", NULL, uri);

  // TODO: Build contact-vcard

  return (col);
}


//
// '_papplContactImport()' - Import contact information from an "xxx-contact-col" value.
//

void
_papplContactImport(
    ipp_t           *col,		// I - "xxx-contact-col" value
    pappl_contact_t *contact)		// O - Contact information
{
}
