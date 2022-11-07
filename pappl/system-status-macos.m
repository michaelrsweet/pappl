//
// macOS system status UI for the Printer Application Framework
//
// Copyright © 2022 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

//
// Include necessary headers...
//

#include "pappl-private.h"
#import <AppKit/AppKit.h>


//
// Local class to hold system status data...
//

@interface PAPPLSystemStatusUI : NSObject <NSApplicationDelegate>
{
  @public

  pappl_system_t	*system;	// System object
  pthread_rwlock_t	rwlock;		// Reader/writer lock
  size_t		event_count;	// Event counter
  NSString		*name;		// Application name
  NSMenu		*mainMenu;	// Application menu
  NSStatusItem		*statusItem;	// Status item in menubar
  NSMenu		*statusMenu;	// Menu associated with status item
}

+ (id)newStatusUI:(pappl_system_t *)system;
- (IBAction)showPrinter:(id)sender;
- (IBAction)showWebPage:(id)sender;
- (IBAction)terminate:(id)sender;
- (void)updateMenu;
@end


//
// Local function...
//

static void	printer_cb(pappl_printer_t *printer, PAPPLSystemStatusUI *ui);
static void	status_cb(pappl_system_t *system, pappl_printer_t *printer, pappl_job_t *job, pappl_event_t event, void *data);


//
// '_papplSystemStatusUI()' - Show/run the system status UI.
//

void
_papplSystemStatusUI(
    pappl_system_t *system)		// I - System
{
  NSApplication	*app;			// Application object
  size_t	last_count = 0;		// Last count


  // Create a menu bar status item...
  app = [NSApplication sharedApplication];

  PAPPLSystemStatusUI *ui = [PAPPLSystemStatusUI newStatusUI:system];

  if (ui == nil)
  {
    NSLog(@"Unable to create system status UI.");
    return;
  }

  app.delegate = ui;
  app.mainMenu = ui->mainMenu;

  // Do a run loop that exits once the system is no longer running...
  while (papplSystemIsRunning(system))
  {
    NSEvent *event = [NSApp nextEventMatchingMask:NSEventMaskAny untilDate:[NSDate dateWithTimeIntervalSinceNow:0.5] inMode:NSDefaultRunLoopMode dequeue:YES];
    if (event)
      [NSApp sendEvent:event];

    pthread_rwlock_rdlock(&ui->rwlock);
    if (ui->event_count > last_count)
    {
//      NSLog(@"******** last_count=%u, event_count=%u", (unsigned)last_count, (unsigned)ui->event_count);
      last_count = ui->event_count;

      [ui updateMenu];
    }
    pthread_rwlock_unlock(&ui->rwlock);
  }
}


@implementation PAPPLSystemStatusUI
//
// 'newStatusUI:' - Create a new system status user interface.
//

+ (id)newStatusUI:(pappl_system_t *)system
{
  // Allocate a new UI class instance...
  PAPPLSystemStatusUI *ui = [PAPPLSystemStatusUI alloc];
					// New system status UI

  if (ui != nil)
  {
    // Assign pointers...
    ui->system             = system;
    system->systemui_data  = (void *)CFBridgingRetain(ui);
    system->systemui_cb    = status_cb;

    pthread_rwlock_init(&ui->rwlock, NULL);

    // Create the menu bar icon...
    ui->statusItem = [[NSStatusBar systemStatusBar] statusItemWithLength:
NSSquareStatusItemLength];

    // Create the menu bar menus...
    ui->name = [NSString stringWithUTF8String:system->versions[0].name];

    ui->mainMenu = [[NSMenu alloc] initWithTitle:ui->name];
    [ui->mainMenu addItemWithTitle:[NSString stringWithFormat:@"About %@",ui->name] action:@selector(orderFrontStandardAboutPanel:) keyEquivalent:@""];

    // Set the image for the item...
    NSImage *image              = [NSApp.applicationIconImage copy];
    image.size                  = ui->statusItem.button.frame.size;
    ui->statusItem.button.image = image;

    // Build the menu...
    [ui updateMenu];
  }

  return (ui);
}


//
// 'showPrinter:' - Show the selected printer's web page.
//

- (IBAction)showPrinter:(id)sender
{
  pappl_printer_t *printer;		// Printer
  char		uri[1024];		// Web page


  if ((printer = papplSystemFindPrinter(system, NULL, (int)((NSMenuItem *)sender).tag, NULL)) != NULL)
  {
    httpAssembleURI(HTTP_URI_CODING_ALL, uri, sizeof(uri), "http", NULL, "localhost", papplSystemGetHostPort(system), printer->uriname);

    NSLog(@"showWebPage: Opening '%s'.", uri);

    [[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:[NSString stringWithUTF8String:uri]]];
  }
}


//
// 'showWebPage:' - Show the system web page.
//

- (IBAction)showWebPage:(id)sender
{
  char uri[1024];			// Web page


  httpAssembleURI(HTTP_URI_CODING_ALL, uri, sizeof(uri), "http", NULL, "localhost", papplSystemGetHostPort(system), "/");

  NSLog(@"showWebPage: Opening '%s'.", uri);

  [[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:[NSString stringWithUTF8String:uri]]];
}


//
// 'terminate:' - Quit the printer application.
//

- (IBAction)terminate:(id)sender
{
  papplSystemShutdown(system);
}


//
// 'updateMenu' - Update the status icon menu.
//

- (void)updateMenu
{
  char	sysname[256];			// System name


//  NSLog(@"******** updateMenu");

  // Start with the system name...
  statusMenu = [[NSMenu alloc] initWithTitle:name];
  NSMenuItem *item = [[NSMenuItem alloc] initWithTitle:[NSString stringWithUTF8String:papplSystemGetName(system, sysname, sizeof(sysname))] action:@selector(showWebPage:) keyEquivalent:@""];
  item.target = self;

  [statusMenu addItem:item];

  // Version number...
  [statusMenu addItemWithTitle:[NSString stringWithFormat:@"    %s %s",system->versions[0].name,system->versions[0].sversion] action:nil keyEquivalent:@""];

  // Separator...
  [statusMenu addItem:[NSMenuItem separatorItem]];

  // Then show all of the printers...
  papplSystemIteratePrinters(system, (pappl_printer_cb_t)printer_cb, (__bridge void *)self);

  // Another separator...
  [statusMenu addItem:[NSMenuItem separatorItem]];

  // Quit...
  item = [[NSMenuItem alloc] initWithTitle:@"Quit" action:@selector(terminate:) keyEquivalent:@""];
  item.target = self;

  [statusMenu addItem:item];

  // Replace the current menu...
  self->statusItem.menu = self->statusMenu;
}
@end


//
// 'printer_cb()' - Callback for adding a printer to the status menu.
//

static void
printer_cb(
    pappl_printer_t     *printer,	// I - Printer
    PAPPLSystemStatusUI *ui)		// I - Status UI
{
  char		name_status[256];	// Printer name
  pappl_preason_t reasons;		// State reasons...


  snprintf(name_status, sizeof(name_status), "%s (%s)", printer->name, ippEnumString("printer-state", (int)printer->state));
  reasons = printer->state_reasons;

  NSMenuItem *item = [[NSMenuItem alloc] initWithTitle:[NSString stringWithUTF8String:name_status] action:@selector(showPrinter:) keyEquivalent:@""];
  item.target = ui;
  item.tag    = printer->printer_id;
  [ui->statusMenu addItem:item];

  if (reasons & (PAPPL_PREASON_MEDIA_NEEDED | PAPPL_PREASON_MEDIA_EMPTY))
    [ui->statusMenu addItemWithTitle:@"    Out of paper." action:nil keyEquivalent:@""];
  else if (reasons & PAPPL_PREASON_MEDIA_LOW)
    [ui->statusMenu addItemWithTitle:@"    Low paper." action:nil keyEquivalent:@""];

  if (reasons & PAPPL_PREASON_MARKER_SUPPLY_EMPTY)
    [ui->statusMenu addItemWithTitle:@"    Out of ink." action:nil keyEquivalent:@""];
  else if (reasons & PAPPL_PREASON_MARKER_SUPPLY_LOW)
    [ui->statusMenu addItemWithTitle:@"    Low ink." action:nil keyEquivalent:@""];

  if (reasons & PAPPL_PREASON_TONER_EMPTY)
    [ui->statusMenu addItemWithTitle:@"    Out of toner." action:nil keyEquivalent:@""];
  else if (reasons & PAPPL_PREASON_TONER_LOW)
    [ui->statusMenu addItemWithTitle:@"    Low toner." action:nil keyEquivalent:@""];
}


//
// 'status_cb()' - Handle system events...
//

static void
status_cb(
    pappl_system_t  *system,		// I - System
    pappl_printer_t *printer,		// I - Printer, if any
    pappl_job_t     *job,		// I - Job, if any
    pappl_event_t   event,		// I - Event
    void            *data)		// I - System UI data
{
  (void)system;
  (void)printer;
  (void)job;
  (void)data;

//  NSLog(@"******** status_cb: event=%08x", event);

  if (event & (PAPPL_EVENT_PRINTER_ALL | PAPPL_EVENT_SYSTEM_CONFIG_CHANGED))
  {
    // Printer or system change event, update the menu...
    PAPPLSystemStatusUI *ui = (__bridge PAPPLSystemStatusUI *)system->systemui_data;
    pthread_rwlock_wrlock(&ui->rwlock);
    ui->event_count ++;
    pthread_rwlock_unlock(&ui->rwlock);

//    NSLog(@"******* status_cb: event_count=%u", (unsigned)ui->event_count);
  }

#if 0 // TODO: Migrate to new UNUserNotification API
  if (event & PAPPL_EVENT_PRINTER_STATE_CHANGED)
  {
    pappl_preason_t reasons;		// State reasons...
    NSString	*message = nil;		// Message for notification

    reasons = printer->state_reasons;
    if (reasons & (PAPPL_PREASON_MEDIA_NEEDED | PAPPL_PREASON_MEDIA_EMPTY))
      message = @"Out of paper.";
    else if (reasons & PAPPL_PREASON_MARKER_SUPPLY_EMPTY)
      message = @"Out of ink.";
    else if (reasons & PAPPL_PREASON_TONER_EMPTY)
      message = @"Out of toner.";

    if (message)
    {
      NSUserNotification *n = [[NSUserNotification alloc] init];
      n.title           = [NSString stringWithUTF8String:papplPrinterGetName(printer)];
      n.informativeText = message;
      [[NSUserNotificationCenter defaultUserNotificationCenter] deliverNotification:n];
    }
  }
#endif // 0
}
