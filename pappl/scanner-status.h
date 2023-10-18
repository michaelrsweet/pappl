#ifndef SCANNER_STATUS_H
#define SCANNER_STATUS_H

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define MAX_URI_LENGTH 2048
#define MAX_UUID_LENGTH 128
typedef enum
{
    Canceled,  // Job was canceled
    Aborted,   // End state due to error
    Completed, // Job is finished
    Pending,   // Job was initiated
    Processing // Scanner is processing the job
} JobState;

typedef enum
{
    Idle,              // Scanner is idle
    ScannerProcessing, // Scanner is busy with some job/activity
    Testing,           // Scanner is calibrating or preparing
    Stopped,           // Error condition occurred
    Down               // Unit is unavailable
} ScannerState;

typedef enum
{
    ScannerAdfProcessing, // OK state
    ScannerAdfEmpty,
    ScannerAdfJam,
    ScannerAdfLoaded,
    ScannerAdfMispick,
    ScannerAdfHatchOpen,
    ScannerAdfDuplexPageTooShort,
    ScannerAdfDuplexPageTooLong,
    ScannerAdfMultipickDetected,
    ScannerAdfInputTrayFailed,
    ScannerAdfInputTrayOverloaded
} AdfState;

typedef struct
{
    char JobUuid[MAX_UUID_LENGTH];
    char JobUri[MAX_URI_LENGTH];
    uint32_t Age;
    uint32_t ImagesCompleted;
    ScannerState scannerState;
    AdfState adfState;
    JobState jobState;
} JobInfo;

JobInfo *parseFile(const char *filename);
JobInfo *scanner_status(const char *filename);
#endif // SCANNER_STATUS_H
