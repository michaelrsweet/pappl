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
// '_papplSystemStatusCallback()' - Handle system events...
//

void
_papplSystemStatusCallback(
    pappl_system_t  *system,		// I - System
    pappl_printer_t *printer,		// I - Printer, if any
    pappl_job_t     *job,		// I - Job, if any
    pappl_event_t   event,		// I - Event
    void            *data)		// I - System UI data
{
  (void)system;
  (void)printer;
  (void)job;
  (void)event;
  (void)data;
}


//
// '_papplSystemStatusUI()' - Show/run the system status UI.
//

void
_papplSystemStatusUI(
    pappl_system_t *system)		// I - System
{
  // Create a menu bar status item...
  [NSApplication sharedApplication];

  NSStatusItem *item = [[NSStatusBar systemStatusBar] statusItemWithLength:
NSSquareStatusItemLength];

  system->systemui_data = (void *)CFBridgingRetain(item);

  // Set the image for the item...
  // TODO: Do something about providing an application icon...
  //NSImage *image = [[NSImage alloc] initWithContentsOfFile:@"pappl/icon-sm.png"];
  NSImage *image = [NSImage imageNamed:NSImageNameApplicationIcon];
  image.size        = item.button.frame.size;
  item.button.image = image;

  // Build the menu...
  char name[256];			// System name
  item.menu = [[NSMenu alloc] initWithTitle:@"PAPPL"];
  [item.menu addItemWithTitle:[NSString stringWithUTF8String:papplSystemGetName(system, name, sizeof(name))] action:nil keyEquivalent:@""];

  // Do a run loop that exits once the system is no longer running...
  while (papplSystemIsRunning(system))
  {
    NSEvent *event = [NSApp nextEventMatchingMask:NSEventMaskAny untilDate:[NSDate dateWithTimeIntervalSinceNow:1.0] inMode:NSDefaultRunLoopMode dequeue:YES];
    if (event)
      [NSApp sendEvent:event];
  }
}
