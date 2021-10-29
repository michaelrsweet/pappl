//
// Contact functions for the Printer Application Framework
//
// Copyright © 2019-2021 by Michael R Sweet.
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
  ipp_t		*col = ippNew();	// "xxx-contact-col" value
  char		tel_uri[256],		// Telephone URI value
		mailto_uri[256],	// Email URI value
		vcard[1024];		// VCARD value


  // Build values...
  if (contact->email[0])
    httpAssembleURI(HTTP_URI_CODING_ALL, mailto_uri, sizeof(mailto_uri), "mailto", NULL, contact->email, 0, NULL);
  else
    mailto_uri[0] = '\0';

  if (contact->telephone[0])
    httpAssembleURI(HTTP_URI_CODING_ALL, tel_uri, sizeof(tel_uri), "tel", NULL, contact->telephone, 0, NULL);
  else
    tel_uri[0] = '\0';

  snprintf(vcard, sizeof(vcard),
           "BEGIN:VCARD\r\n"
           "VERSION:4.0\r\n"
           "FN:%s\r\n"
           "TEL;VALUE=URI;TYPE=work:%s\r\n"
           "EMAIL;TYPE=work:%s\r\n"
           "END:VCARD\r\n", contact->name, tel_uri, contact->email);

  // Add values...
  ippAddString(col, IPP_TAG_ZERO, IPP_TAG_NAME, "contact-name", NULL, contact->name);

  if (mailto_uri[0])
    ippAddString(col, IPP_TAG_ZERO, IPP_TAG_URI, "contact-uri", NULL, mailto_uri);
  else if (tel_uri[0])
    ippAddString(col, IPP_TAG_ZERO, IPP_TAG_URI, "contact-uri", NULL, tel_uri);
  else
    ippAddString(col, IPP_TAG_ZERO, IPP_CONST_TAG(IPP_TAG_URI), "contact-uri", NULL, "data:,");

  ippAddString(col, IPP_TAG_ZERO, IPP_TAG_TEXT, "contact-vcard", NULL, vcard);

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
  const char	*val;			// Value


  memset(contact, 0, sizeof(pappl_contact_t));

  if ((val = ippGetString(ippFindAttribute(col, "contact-name", IPP_TAG_NAME), 0, NULL)) != NULL)
    papplCopyString(contact->name, val, sizeof(contact->name));

  if ((val = ippGetString(ippFindAttribute(col, "contact-uri", IPP_TAG_NAME), 0, NULL)) != NULL)
  {
    char	scheme[32],		// URI scheme
		userpass[32],		// User:pass from URI
		host[256],		// Host from URI
		resource[256];		// Resource from URI
    int		port;			// Port number from URI

    if (httpSeparateURI(HTTP_URI_CODING_ALL, val, scheme, sizeof(scheme), userpass, sizeof(userpass), host, sizeof(host), &port, resource, sizeof(resource)) >= HTTP_URI_STATUS_OK)
    {
      if (!strcmp(scheme, "tel"))
        papplCopyString(contact->telephone, resource, sizeof(contact->telephone));
      else if (!strcmp(scheme, "mailto"))
        papplCopyString(contact->email, resource, sizeof(contact->email));
    }
  }

  if ((val = ippGetString(ippFindAttribute(col, "contact-vcard", IPP_TAG_TEXT), 0, NULL)) != NULL)
  {
    char	vcard[1024],		// Local copy of vcard data
		*ptr,			// Pointer in data
		*next,			// Next line
		*crlf;			// CR LF at end of line

    // Note: Only VCARD data <= 1023 bytes currently supported...
    papplCopyString(vcard, val, sizeof(vcard));
    for (ptr = vcard; *ptr; ptr = next)
    {
      // Find the end of the current line...
      if ((crlf = strstr(ptr, "\r\n")) != NULL)
      {
        *crlf = '\0';
        next  = crlf + 2;
      }
      else
        next = ptr + strlen(ptr);

      if (!strncmp(ptr, "FN:", 3) && !contact->name[0])
      {
        papplCopyString(contact->name, ptr + 3, sizeof(contact->name));
      }
      else if (!strncmp(ptr, "TEL;", 4) && !contact->telephone[0])
      {
        if ((ptr = strstr(ptr, ":tel:")) != NULL)
          papplCopyString(contact->telephone, ptr + 5, sizeof(contact->telephone));
      }
      else if (!strncmp(ptr, "EMAIL;", 6) && !contact->email[0])
      {
        if ((ptr = strchr(ptr, ':')) != NULL)
          papplCopyString(contact->email, ptr + 1, sizeof(contact->email));
      }
    }
  }
}
