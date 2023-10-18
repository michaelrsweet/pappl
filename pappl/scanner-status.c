#include "scanner-status.h"

JobInfo *scanner_status(const char *filename)
{
    FILE *file = fopen(filename, "r");
    if (!file)
        return NULL; // File open error

    size_t maxJobs = 10; // Initial allocation for 10 jobs
    size_t *count = 0;

    JobInfo *jobs = (JobInfo *)malloc(maxJobs * sizeof(JobInfo));
    if (!jobs)
        return NULL; // Memory allocation error

    char line[256];

    while (fgets(line, sizeof(line), file))
    {
        if (strstr(line, "Job UUID"))
        {
            sscanf(line, "- Job UUID: %s", jobs[*count].JobUuid);
        }
        else if (strstr(line, "Job URI"))
        {
            sscanf(line, "- Job URI: %s", jobs[*count].JobUri);
        }
        else if (strstr(line, "Age"))
        {
            sscanf(line, "- Age: %u", &jobs[*count].Age);
        }
        else if (strstr(line, "Images Completed"))
        {
            sscanf(line, "- Images Completed: %u", &jobs[*count].ImagesCompleted);
        }
        else if (strstr(line, "Job State Reasons")) // end marker for each job info
        {
            if (strstr(line, "Cancelled"))
            {
                jobs[*count].jobState = Canceled;
            }
            if (strstr(line, "Aborted"))
            {
                jobs[*count].jobState = Aborted;
            }
            if (strstr(line, "Completed"))
            {
                jobs[*count].jobState = Completed;
            }
            if (strstr(line, "Pending"))
            {
                jobs[*count].jobState = Pending;
            }
            if (strstr(line, "Processing"))
            {
                jobs[*count].jobState = Processing;
            }

            jobs[*count].scannerState = Idle;             // Default
            jobs[*count].adfState = ScannerAdfProcessing; // Default

            (*count)++;

            // If we've reached the max capacity, reallocate
            if (*count >= maxJobs)
            {
                maxJobs *= 2;
                jobs = (JobInfo *)realloc(jobs, maxJobs * sizeof(JobInfo));
                if (!jobs)
                {
                    fclose(file);
                    return NULL; // Memory reallocation error
                }
            }
        }
    }

    fclose(file);
    return jobs;
}

// int main()
// {
//     // Example usage
//     JobInfo job = {
//         .JobUuid = "sample-uuid-1234567890",
//         .JobUri = "http://example.com/job/123456",
//         .Age = 10,
//         .ImagesCompleted = 5};

//     JobState currentJobState = Processing;
//     ScannerState currentScannerState = ScannerProcessing;
//     AdfState currentAdfState = ScannerAdfProcessing;

//     // Print states
//     printf("Job UUID: %s\n", job.JobUuid);
//     printf("Job URI: %s\n", job.JobUri);
//     printf("Job Age: %u\n", job.Age);
//     printf("Images Completed: %u\n", job.ImagesCompleted);
//     printf("Current Job State: %d\n", currentJobState);
//     printf("Current Scanner State: %d\n", currentScannerState);
//     printf("Current Adf State: %d\n", currentAdfState);

//     return 0;
// }
