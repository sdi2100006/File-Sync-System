#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <bits/stdc++.h>
#include <unordered_map>
#include "../include/sync_info_mem_store.hpp"
#include <queue>
#include <sys/inotify.h>
#include <dirent.h>
#include "../include/utilities.hpp"

using namespace std;

#define SIZE 128
#define PERM 0644
#define BUFFSIZE 4096
#define ERROR_SIZE 512
#define REPORT_SIZE 1024

typedef struct report_info{
    char timestamp[20];
    string source;
    string dest;
    string filename;
    pid_t pid;
    string operation;
    string status;
    char errors[ERROR_SIZE];
    int num;
} report_info_struct;

//copy file name1 to file name2
int copy_file(const char* name1, const char* name2, char* error_buffer) {
    int infile, outfile;
    size_t nread;
    char buffer[BUFFSIZE];

    //checks
    if ( (infile=open(name1, O_RDONLY)) == -1) {
        snprintf(error_buffer, SIZE, "Can't open source file '%s' during copy. %s", name1, strerror(errno));
        cout << "error_buffer: " << error_buffer << endl;
        return -5;
        //exit(-2);
    }
    if ( (outfile=open(name2, O_WRONLY|O_CREAT|O_TRUNC, PERM)) == -1) {
        snprintf(error_buffer, SIZE, "Can't open dest file '%s' during copy. %s", name2, strerror(errno));
        cout << "error_buffer: " << error_buffer << endl;
        return -5;
        //exit(-2);
    }

    while ( (nread=read(infile, buffer, BUFFSIZE)) > 0) {
        if ( write(outfile, buffer, nread) < nread) {
            close(infile);
            close(outfile);
            snprintf(error_buffer, SIZE, "error writing to file '%s' during copy. %s", name2, strerror(errno));
            cout << "error_buffer: " << error_buffer << endl;
            return -3;
        }
        ///cout << "shit" << endl;
    }
    if (nread == -1) {
        close(infile);
        close(outfile);
        snprintf(error_buffer, SIZE, "error reading from file '%s' during copy. %s", name1, strerror(errno));
        cout << "error_buffer: " << error_buffer << endl;
        return -4;
    }
    if (close(infile) == -1) {
        snprintf(error_buffer, SIZE, "error closing source file '%s' during copy. %s", name1, strerror(errno));
        cout << "error_buffer: " << error_buffer << endl;
        return -4;
    }
    if (close(outfile) == -1) {
        snprintf(error_buffer, SIZE, "error closing dest file '%s' during copy. %s", name2, strerror(errno));
        cout << "error_buffer: " << error_buffer << endl;
        return -4;
    }

    return 0;
}

//parses the given directory and retruns a vector containing the file names
vector<char*> parse_directory(const char dirname[]) {
    DIR *dir_ptr;
    struct dirent *direntp;
    vector<char*> files;

    if( (dir_ptr = opendir(dirname) ) == NULL ) {
        perror("opendir");
    } else {
        while ( (direntp=readdir(dir_ptr)) != NULL ) {

            if ( strcmp(direntp->d_name, ".") == 0 || strcmp(direntp->d_name, "..") == 0 )
                continue;

            char* file;

            files.push_back(strdup(direntp->d_name));   //insert a duplacate of d_name i need to free them after SOS!!!
        }
        closedir(dir_ptr);
    }
    return files;
}


int main(int argc, char *argv[]) {
    //process args MAYBE MAKE THEM STRINGS - NO KEEP EVERYTHING CRITICAL AS A CHAR* OR A TABLE ?
    char* source = argv[1];
    char* destination = argv[2];
    char* filename = argv[3]; //if we want a full sync  it should be ALL
    char* operation = argv[4]; //FULL, ADDED, MODIFIED, DELETED

    report_info_struct report_info;
    strcpy(report_info.timestamp, get_current_time());
    report_info.source = source;
    report_info.dest = destination;
    report_info.filename = filename;
    report_info.pid = getpid();
    report_info.num = 0;

    char exec_report[1024]; //where the final report will be stored
    char error_buffer[SIZE]; //where the error messages will be stored

    int copied_count = 0;
    int skipped_count = 0;
    
    if ( strcmp(operation, "FULL") == 0 ) {
        //parse the directory and call copy for every file.
        vector<char*> file_names = parse_directory(source); //returns the file names of the directory in the vector
        for (int i=0 ; i<file_names.size() ; i++) {
            string full_name_source = report_info.source + "/" + file_names[i];
            string full_name_dest = report_info.dest + "/" + file_names[i];
            copy_file(full_name_source.c_str(), full_name_dest.c_str(), error_buffer);
            if (copy_file(full_name_source.c_str(), full_name_dest.c_str(), error_buffer) == 0) {
                copied_count++;
            } else {
                skipped_count++;
                report_info.num++;
                //report_info.errors += error_buffer;
            }
        }

        report_info.operation = "FULL";
        if (skipped_count == 0 && copied_count > 0) {
            report_info.status = "SUCCESS";
            snprintf(report_info.errors, ERROR_SIZE, "%d files copied", copied_count);
        } else if (skipped_count > 0 && copied_count > 0) {
            report_info.status = "PARTIAL";
            snprintf(report_info.errors, ERROR_SIZE, "%d files copied, %d skipped", copied_count, skipped_count);
        } 

 
    } else if ( strcmp(operation, "ADDED") == 0 ) {
        //copy the requested file to the source directory
        string full_name_source = report_info.source + "/" + report_info.filename;
        string full_name_dest = report_info.dest + "/" + report_info.filename;

        if (copy_file(full_name_source.c_str(), full_name_dest.c_str(), error_buffer) == 0) {
            copied_count++;
        } else {
            skipped_count++;
            report_info.num++;
        }

        report_info.operation = "ADDED";
        if (skipped_count > 0) {
            report_info.status = "ERROR";
            snprintf(report_info.errors, ERROR_SIZE + report_info.filename.size() , "File: %s - %s", report_info.filename.c_str(), error_buffer);
        } else {
            report_info.status = "SUCCESS";
            snprintf(report_info.errors, ERROR_SIZE, "File: %s", report_info.filename.c_str());
        }
            
    } else if ( strcmp(operation, "MODIFIED") == 0) {

        string full_name_source = report_info.source + "/" + report_info.filename;
        string full_name_dest = report_info.dest + "/" + report_info.filename;

        if (copy_file(full_name_source.c_str(), full_name_dest.c_str(), error_buffer) == 0) {
            copied_count++;
        } else {
            skipped_count++;
            report_info.num++;
        }

        report_info.operation = "MODIFIED";
        if (skipped_count > 0) {
            report_info.status = "ERROR";
            snprintf(report_info.errors, ERROR_SIZE, "File: %s - %s", report_info.filename.c_str(), error_buffer);
        } else {
            report_info.status = "SUCCESS";
            snprintf(report_info.errors, ERROR_SIZE, "File: %s", report_info.filename.c_str());
        }

    } else if ( strcmp(operation, "DELETED") == 0) {

        string full_name_dest = report_info.dest + "/" + report_info.filename;

        if(unlink(full_name_dest.c_str()) == -1) {
            snprintf(error_buffer, ERROR_SIZE, " - Can't delete file '%s'. %s", full_name_dest.c_str(), strerror(errno));
            skipped_count++;
            report_info.num++;
        }

        report_info.operation = "DELETED";
        if (skipped_count > 0) {
            report_info.status = "ERROR";
            snprintf(report_info.errors, ERROR_SIZE, "File: %s - %s", report_info.filename.c_str(), error_buffer);
        } else {
            report_info.status = "SUCCESS";
            snprintf(report_info.errors, ERROR_SIZE, "File: %s", report_info.filename.c_str());
        }
    }


    snprintf(exec_report, REPORT_SIZE, "TIMESTAMP=%s\nSOURCE=%s\nDEST=%s\nPID=%d\nOP=%s\nSTATUS=%s\nERRORS=%s\nNUM=%d", report_info.timestamp, report_info.source.c_str(), report_info.dest.c_str(), report_info.pid, report_info.operation.c_str(), report_info.status.c_str(), report_info.errors, report_info.num);
    sleep(10);
    cout << exec_report << endl; //send to pipe
    cout.flush();   //maybe uselles

    /*cout << "TIMESTAMP=" << report_info.timestamp << endl
     << "SOURCE=" << report_info.source << endl
     << "DEST=" << report_info.dest << endl
     << "PID=" << report_info.pid << endl
     << "OP=" << report_info.operation << endl
     << "STATUS=" << report_info.status << endl
     << "ERRORS=" << report_info.errors << endl
     << "NUM=" << report_info.num;*/
    //cout.flush();   //maybe uselles
    
    /*free(additional_info);
    free(report_buffer);
    free(error_buffer);*/
    exit(0);

}