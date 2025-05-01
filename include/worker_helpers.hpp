#pragma once
#include <vector>
#include <string>

#define SIZE 128
#define PERM 0644
#define BUFFSIZE 4096
#define ERROR_SIZE 512
#define REPORT_SIZE 1024

using namespace std;

typedef struct report_info_worker{
    char timestamp[20];
    string source;
    string dest;
    string filename;
    pid_t pid;
    string operation;
    string status;
    char errors[ERROR_SIZE];
    int num;
} report_info_worker_struct;

using namespace std;

int copy_file(const char* name1, const char* name2, char* error_buffer);

vector<char*> parse_directory(const char dirname[]);