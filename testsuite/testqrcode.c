//
// QR code unit tests for the Printer Application Framework
//
// Copyright © 2026 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#include <pappl/qrcode-private.h>
#include "test.h"


//
// Local functions...
//

static void	test_qrcode(FILE *fp, const char *s);


//
// 'main()' - Test the QR code functions.
//

int					// O - Exit status
main(void)
{
  FILE	*fp;				// Output file


  if ((fp = fopen("testqrcode.html", "w")) == NULL)
  {
    perror("Unable to create 'testqrcode.html'");
    return (1);
  }

  fputs("<!DOCTYPE html>\n", fp);
  fputs("<html>\n", fp);
  fputs("  <head>\n", fp);
  fputs("    <title>QR Code Test Page</title>\n", fp);
  fputs("  </head>\n", fp);
  fputs("  <body>\n", fp);
  fputs("    <h1>QR Code Test Page</h1>\n", fp);

  test_qrcode(fp, "https://www.msweet.org/pappl/");
  test_qrcode(fp, "https://github.com/michaelrsweet/pappl");
  test_qrcode(fp, "ipps://printer.example.com/ipp/print/example");
  test_qrcode(fp, "BEGIN:VCARD\nN:Sweet;Michael\nFN:Michael Sweet\nADR:;;42 Any St;Any Town;ON;H0H0H0;Canada\nTEL;WORK;VOICE:705 555-1212\nEMAIL;WORK;INTERNET:msweet@example.com\nURL:https://www.msweet.org/\nEND:VCARD");
  test_qrcode(fp, "WIFI:S:MySSID;T:WPA;P:MyPassW0rd;;");

  fputs("  </body>\n", fp);
  fputs("</html>\n", fp);

  fclose(fp);

  return (testsPassed ? 0 : 1);
}


//
// 'test_qrcode()' - Test writing a QR code.
//

static void
test_qrcode(FILE       *fp,		// I - File
            const char *s)		// I - String for QR code
{
  _pappl_bb_t	*qrcode;		// QR code
  char		*dataurl;		// "data:" URL


  // Create the QR code...
  testBegin("_papplMakeQRCode(%s)", s);
  qrcode = _papplMakeQRCode(s, _PAPPL_QRVERSION_AUTO, _PAPPL_QRECC_LOW);
  testEnd(qrcode != NULL);

  // Create the data URL...
  testBegin("_papplMakeDataURL()");
  if ((dataurl = _papplMakeDataURL(qrcode)) != NULL)
  {
    testEndMessage(true, "%u bytes", (unsigned)strlen(dataurl));

    fprintf(fp, "    <p>%s<br>\n    &nbsp;<br>\n    <img src=\"%s\" border=\"4\"><br>&nbsp;</p>\n", s, dataurl);

    free(dataurl);
  }
  else
  {
    testEnd(false);
  }

  _papplBBDelete(qrcode);
}
