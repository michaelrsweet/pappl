EPX Prototyping Development Plan
=====================================

### References
[1]: https://openprinting.github.io/documentation/02-designing-printer-drivers/
[2]: file:///Users/smitty/source/GitHub/wifiprintguy/pappl-epx/doc/pappl.html

Xcode project Setup
-------------------------------------

Download static tarballs from the folowing dependencies into the \_DEPENDENCIES\_ directory, unpack, build, and update the references to the static libraries
- openssl
- libpng
- libjpeg
- libusb

Header search paths (keep /usr/local/ at the end so that older SDK versions don't get selected first):
. .. ../libcups /opt/homebrew/Cellar/openssl@3/3.0.7/include /opt/homebrew/Cellar/libpng/1.6.38/include /opt/homebrew/Cellar/jpeg/9e/include /opt/homebrew/Cellar/libusb/1.0.26/include/** /usr/local/include

Library search paths (keep /usr/local/ at the end so that older SDK versions don't get selected first):
../libcups /opt/homebrew/Cellar/openssl@3/3.0.7/lib /opt/homebrew/Cellar/libpng/1.6.38/lib /opt/homebrew/Cellar/jpeg/9e/lib /opt/homebrew/Cellar/libusb/1.0.26/lib /usr/local/lib

Libraries and Frameworks:
- Remove the TBD reference to libcups.dylib and libcupsimage.dylib, and add a reference to /Users/smitty/source/GitHub/openprinting/libcups/cups/libcups.3.3.dylib

testepx target
- Add the testepx folder
- Copy the papplMainloop target to make a testepx target
- Add the files from the folder to the new target
- Copy the papplMainLoop scheme to make a testepx scheme
- Edit the new scheme to provide the following arguments: 


Plan
-------------------------------------
* Read several sources to see if pappl is really the best choice for this task and if so how to use it
** In particular, I don't know if pappl runs till I kill it like ippserver is, so if not I need to think about how printer state is maintained across executions
** Because there can be non-trivial time spans between segments of Proof Print, Job Release, and Job Storage, if it isn't always a running process I will have to manage that job statefulness across runs. (Thoughts?)
* If pappl is the right choice, then I guess I will be planning to use it to store received documents in one place and write the print ready files to a different place (maybe separate directories for output bins). I need to research how pappl does this and see to what extent I need to manage that myself
* Need to extend the Printer to support the required Printer Description attributes, either by modifying the app that provides them to pappl, or by modifying pappl. I think this should be doable via a callback but I still don't know my way around pappl well enough to be certain.
* Need to extend the Printer to support the Get-User-Printer-Attributes operation and figure out how to have different users and do attribute filtering.
** Need to research how pappl deals with authentication and manages usernames / passwords.
** The _papplPrinterProcessIPP() function already has "ipp_get_printer_attributes()" handle the  IPP_OP_GET_PRINTER_ATTRIBUTES, IPP_OP_GET_PRINTER_SUPPORTED_VALUES and IPP_OP_CUPS_GET_DEFAULT cases, so it should be able to also handle an IPP_OP_GET_USER_PRINTER_ATTRIBUTES case. But this will require extending pappl - can't be extended by the app using pappl (is this true?)
* Need to decide / figure out how to handle the "console interactions" for Job Release / Job Storage / Proof Print. Maybe this can be done via the command line by using the "Sub-command callback"...
* Need to figure out the driver callback and the file printing callback and the raster printing callback and see how all that works together


Operations
-------------------------------------
* Get-User-Printer-Attributes


Operation Attributes
-------------------------------------
* job-password
* job-password-encryption
* job-release-action
* job-storage


Job Template Attributes
-------------------------------------
* job-cancel-after
* proof-copies
* proof-print


Printer Description Attributes
-------------------------------------
* job-cancel-after-default
* job-cancel-after-supported
* job-password-encryption-supported
* job-password-length-supported
* job-password-repertoire-supported
* job-password-repertoire-configured
* job-password-supported
* job-release-action-default
* job-release-action-supported
* job-storage-access-supported
* job-storage-disposition-supported
* job-storage-group-supported
* job-storage-supported
* printer-detailed-status-messages 
* proof-copies-supported
* proof-print-copies-supported
* proof-print-default
* proof-print-supported


Job Status Attributes
-------------------------------------
job-release-action
job-storage
parent-job-id
parent-job-uuid


Additional Values
-------------------------------------
* ipp-features-supported:
    * 'job-release'
    * 'job-storage'
    * 'print-policy'
    * 'proof-and-suspend'
    * 'proof-print'
* job-state-reasons:
    * 'conflicting-attributes'
    * 'job-canceled-after-timeout'
    * 'job-held-for-authorization'
    * 'job-held-for-button-press'
    * 'job-held-for-release'
    * 'job-password-wait'
    * 'job-printed-successfully'
    * 'job-printed-with-errors'
    * 'job-printed-with-warnings'
    * 'job-resuming'
    * 'job-stored'
    * 'job-stored-with-errors'
    * 'job-stored-with-warnings'
    * 'job-storing'
    * 'job-suspended-by-operator'
    * 'job-suspended-by-system'
    * 'job-suspended-by-user'
    * 'job-suspended-for-approval'
    * 'job-suspending'
    * 'unsupported-attributes-or-values', 
* which-jobs: 'proof-print'
    * 'proof-and-suspend'
    * 'stored-group'
    * 'stored-owner'
    * 'stored-public'


Obsolete Attributes
-------------------------------------
* feed-orientation
* feed-orientation-default
* feed-orientation-supported
* job-save-disposition
* pdl-init-file
* pdl-init-file-default
* pdl-init-file-entry
* pdl-init-file-entry-supported
* pdl-init-file-location
* pdl-init-file-location-supported
* pdl-init-file-name
* pdl-init-file-name-supported
* pdl-init-file-name-subdirectory-supported
* save-disposition
* save-disposition-supported
* save-document-format
* save-document-format-default
* save-document-format-supported
* save-info
* save-info-supported
* save-location
* save-location-default
* save-location-supported
* save-name
* save-name-supported
* save-name-subdirectory-supported


Obsolete Attribute Values
-------------------------------------
* job-password-encryption:
    * ‘md2’
    * ‘md4’
    * ‘md5’
    * ‘sha'
* job-state-reasons:
    * 'job-saved-successfully'
    * 'job-saved-with-errors'
    * 'job-saved-with-warnings'
    * 'job-saving'
* which-jobs:
    * 'saved'


Other Feedback
-------------------------------------

### Questions and answers

1. How does a "file device" work? Not that clear in the documentation, and there is no way to set a file URL in the web interface (that I can discern).

    Right, providing unlimited remote file access seems like a bad idea to me, so the web interface will only report "discovered" paths.

    In lieu of official documentation, here is what the current code supports:

    1. Character device files: Character device files are opened write-only (no back-channel support), and file:///dev/null is mapped to "NUL:" on Windows.
    2. Directories: The job name passed to papplDeviceOpen is used to name a file under the directory. If the URI contains the "ext" parameter (file:///some/directory?ext=foo) then the value is used as the filename extension, otherwise "prn" is used.
    3. Regular/new files: Regular files are created in append mode, so "file:///some/file.prn" will continue to grow as new jobs are printed.

    Any other kind of file is not supported and will yield an open error.

    The testpappl PWG raster driver doesn't use the file device interface, opting instead to manage its file output separately so that the printer and job ID information is preserved.

2. How / who manages the spool file before it gets sent to processing? Is it the Printer object? Need to understand that before I can start doing things like Job Release and Proof Print and Job Storage.

    The job object manages the file, but uses the printer object's spool directory.

3. Need to figure out how all the authentication plumbing works and figure out how to write a per-user policy document so that I can test Get-User-Printer-Attributes

    The easiest thing for prototyping is to use the built-in PAM support ("-A cups" on macOS will work for any local user).

