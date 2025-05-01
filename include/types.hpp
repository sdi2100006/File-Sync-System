#pragma once
#include <string>

using namespace std;

typedef struct sync_info {
    string source;
    string destination;
    string status ;
    char last_sync_time[20];
    bool active;
    bool monitored;
    int error_count;
    int wd;
} sync_info_struct;

typedef struct job {
    string source;
    string dest;
    string operation;
    string filename;
    bool fromconsole;
    bool sync;
} job_struct;

typedef struct report_info{
    string timestamp;
    string source;
    string dest;
    pid_t pid;
    string operation;
    string status;
    string errors;
    int num;
} report_info_struct;