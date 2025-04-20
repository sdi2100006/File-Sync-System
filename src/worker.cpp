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

#define SIZE 30
#define PERM 0644
#define BUFFSIZE 4096
#define ERROR_SIZE 256

//copy file name1 to file name2
int copy_file(const char* name1, const char* name2, char* error_buffer) {
    int infile, outfile;
    size_t nread;
    char buffer[BUFFSIZE];

    //checks
    if ( (infile=open(name1, O_RDONLY)) == -1) {
        snprintf(error_buffer, ERROR_SIZE, " - Can't open source file '%s' %s", name1, strerror(errno));
        //cout << "error_buffer: " << error_buffer << endl;
        return -5;
        exit(-2);
    }
    if ( (outfile=open(name2, O_WRONLY|O_CREAT|O_TRUNC, PERM)) == -1) {
        snprintf(error_buffer, ERROR_SIZE, " - Can't open dest file '%s', %s", name2, strerror(errno));
        //cout << "error_buffer: " << error_buffer << endl;
        return -5;
        exit(-2);
    }

    while ( (nread=read(infile, buffer, BUFFSIZE)) > 0) {
        if ( write(outfile, buffer, nread) < nread) {
            close(infile);
            close(outfile);
            snprintf(error_buffer, ERROR_SIZE, " - error writing to file '%s', %s", name2, strerror(errno));
            //cout << "error_buffer: " << error_buffer << endl;
            return -3;
        }
        ///cout << "shit" << endl;
    }
    
    close(infile);
    close(outfile);

    if (nread == -1) {
        snprintf(error_buffer, ERROR_SIZE, " - error reading file '%s'. %s", name1, strerror(errno));
        //cout << "error_buffer: " << error_buffer << endl;
        return -4;
    } else {
        return 0;
    }
}

//parses the given directory and retruns a vector containing the file names
vector<char*> parse_directory(const char dirname[]) {
    DIR *dir_ptr;
    struct dirent *direntp;
    vector<char*> files;

    if( (dir_ptr = opendir(dirname) ) == NULL ) {
        //write error messsage in the report buffer while keeping count of how many chars you have already written
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

    int copied_count = 0;
    int skipped_count = 0;

    //start the exec report
    char* timestamp = get_current_time();
    pid_t  pid = getpid();
    int report_buffer_size = snprintf(NULL, 0, "TIMESTAMP=%s\nSOURCE=%s\nDEST=%s\nPID=%d\nOP=%s\n", timestamp, source, destination, pid, operation);
    char* report_buffer = (char*)malloc(report_buffer_size);
    snprintf(report_buffer, report_buffer_size, "TIMESTAMP=%s\nSOURCE=%s\nDEST=%s\nPID=%d\nOP=%s\n", timestamp, source, destination, pid, operation);

    char* additional_info;
    int additional_info_size;
    char* error_buffer = (char*)malloc(ERROR_SIZE *sizeof(char));
    //char error_buffer[50];
    
    if ( strcmp(operation, "FULL") == 0 ) {
        //parse the directory and call copy for every file.
        vector<char*> file_names = parse_directory(source); //returns the file names of the directory in the vector

        for (int i=0 ; i<file_names.size() ; i++) {

            char* source_temp_str = (char*)malloc( (strlen(source) + strlen(file_names[i]) + 2) * sizeof(char));
            char* dest_temp_str = (char*)malloc( (strlen(destination) + strlen(file_names[i]) + 2) * sizeof(char));

            //cool solution to concat 2 strings because segfault with strcat I HAVE FUCKED UP WITH STRINGS AND CHAR*
            snprintf(source_temp_str, strlen(source) + strlen(file_names[i]) + 2, "%s/%s", source, file_names[i]);
            snprintf(dest_temp_str, strlen(destination) + strlen(file_names[i]) + 2, "%s/%s", destination, file_names[i]);

            if( copy_file(source_temp_str, dest_temp_str, report_buffer) != 0 ) { 
                skipped_count++;
                perror("copy error: ");
                //strerror();
            } else {
                copied_count++;
            }
            free(source_temp_str);
            free(dest_temp_str);
        }
 
        if (copied_count == file_names.size() && skipped_count == 0) {  //STATUS: succes
            additional_info_size = snprintf(NULL, 0, "STATUS=SUCCESS\nERRORS=%d files copied, %d skipped\n", copied_count, skipped_count);
            additional_info = (char*)malloc(additional_info_size * sizeof(char));
            snprintf(additional_info, additional_info_size, "STATUS=SUCCESS\nERRORS=%d files copied, %d skipped\n", copied_count, skipped_count);
        } else if (copied_count == 0 && skipped_count == file_names.size()) {   //STATUS: error
            additional_info_size = snprintf(NULL, 0, "STATUS=ERROR\nERRORS=%d files copied, %d skipped\n", copied_count, skipped_count);
            additional_info = (char*)malloc(additional_info_size * sizeof(char));  
            snprintf(additional_info, additional_info_size, "STATUS=ERROR\nERRORS=%d files copied, %d skipped\n", copied_count, skipped_count);
        } else if (copied_count+skipped_count == file_names.size()){    //STATUS: partial
            additional_info_size = snprintf(NULL, 0, "STATUS=PARTIAL\nERRORS=%d files copied, %d skipped\n", copied_count, skipped_count);
            additional_info = (char*)malloc(additional_info_size * sizeof(char));
            snprintf(additional_info, additional_info_size, "STATUS=PARTIAL\nERRORS=%d files copied, %d skipped\n", copied_count, skipped_count);
        } else {    //STATUS: unknown 
            additional_info_size = snprintf(NULL, 0, "STATUS=UNKNOWN\nERRORS=%d files copied, %d skipped\n", copied_count, skipped_count);
            additional_info = (char*)malloc(additional_info_size * sizeof(char));
            snprintf(additional_info, additional_info_size, "STATUS=UNKNOWN\nERRORS=%d files copied, %d skipped\n", copied_count, skipped_count);
        }

        for (int i=0 ; i<file_names.size() ; i++) {
            free(file_names[i]);
        }
 
    } else if ( strcmp(operation, "ADDED") == 0 ) {
        //copy the requested file to the source directory (if a file with the same name exists ????)
        char* temp_source = (char*)malloc( (strlen(source) + 1 + strlen(filename) + 1) * sizeof(char) );
        snprintf(temp_source, strlen(source) + 1 + strlen(filename) + 1, "%s/%s", source, filename);
        char* temp_dest = (char*)malloc( (strlen(destination) + 1 + strlen(filename) + 1) * sizeof(char) );
        snprintf(temp_dest, strlen(destination) + 1 + strlen(filename) + 1, "%s/%s", destination, filename);

        int code = copy_file(temp_source, temp_dest, error_buffer);
        free(temp_source);
        free(temp_dest);
        if( code < 0 ) { 
            skipped_count++;
            perror("ADD: copy_file");
        }
        if (code < 0) {
            additional_info_size = snprintf(NULL, 0, "STATUS=ERROR\nERRORS=FILE: %s%s\n",filename, error_buffer);
            additional_info = (char*)malloc(additional_info_size * sizeof(char));
            snprintf(additional_info, additional_info_size, "STATUS=ERROR\nERRORS=FILE: %s%s\n", filename, error_buffer);
        } else {
            additional_info_size = snprintf(NULL, 0, "STATUS=SUCCESS\nERRORS=FILE: %s\n", filename);
            additional_info = (char*)malloc(additional_info_size * sizeof(char));
            snprintf(additional_info, additional_info_size, "STATUS=SUCCESS\nERRORS=FILE: %s\n", filename);
        }
        
            
    } else if ( strcmp(operation, "MODIFIED") == 0) {
        char* temp_source = (char*)malloc( (strlen(source) + 1 + strlen(filename) + 1) * sizeof(char) );
        snprintf(temp_source, strlen(source) + 1 + strlen(filename) + 1, "%s/%s", source, filename);
        char* temp_dest = (char*)malloc( (strlen(destination) + 1 + strlen(filename) + 1) * sizeof(char) );
        snprintf(temp_dest, strlen(destination) + 1 + strlen(filename) + 1, "%s/%s", destination, filename);

        int code = copy_file(temp_source, temp_dest, error_buffer);
        free(temp_source);
        free(temp_dest);
        if( code < 0 ) { 
            skipped_count++;
            perror("ADD: copy_file");
        }
        if (code < 0) {
            additional_info_size = snprintf(NULL, 0, "STATUS=ERROR\nERRORS=FILE: %s%s\n",filename, error_buffer);
            additional_info = (char*)malloc(additional_info_size * sizeof(char));
            snprintf(additional_info, additional_info_size, "STATUS=ERROR\nERRORS=FILE: %s%s\n", filename, error_buffer);
        } else {
            additional_info_size = snprintf(NULL, 0, "STATUS=SUCCESS\nERRORS=FILE: %s\n", filename);
            additional_info = (char*)malloc(additional_info_size * sizeof(char));
            snprintf(additional_info, additional_info_size, "STATUS=SUCCESS\nERRORS=FILE: %s\n", filename);
        }


    } else if ( strcmp(operation, "DELETED") == 0) {
        //do deleted
        char* temp_dest = (char*)malloc( (strlen(destination) + 1 + strlen(filename) +1) * sizeof(char) );
        snprintf(temp_dest, strlen(destination) + 1 + strlen(filename) +1, "%s/%s\n", destination, filename);

        if (unlink(temp_dest) == -1) {
            //log error 
            additional_info_size = snprintf(NULL, 0, "STATUS=ERROR\nERRORS=FILE: %s - error unlinking file, %s", filename, strerror(errno));
            additional_info = (char*)malloc(additional_info_size * sizeof(char));
            snprintf(additional_info, additional_info_size, "STATUS=ERROR\nERRORS-FILE: %s - error unlinking file, %s", filename, strerror(errno));
        } else {
            //log success
            additional_info_size = snprintf(NULL, 0, "STATUS=SUCCESS\nERRORS=FILE: %s - error unlinking file, %s", filename, strerror(errno));
            additional_info = (char*)malloc(additional_info_size * sizeof(char));
            snprintf(additional_info, additional_info_size, "STATUS=SUCCESS\nERRORS=FILE: %s", filename);
        }
        free(temp_dest);

    }
    report_buffer = (char*)realloc(report_buffer, report_buffer_size + 1 + additional_info_size);
    //cout << "strlen: " << strlen(report_buffer) << endl;
    strcat(report_buffer,"\n");
    strcat(report_buffer, additional_info);
   
   
    cout << report_buffer << endl; //send to pipe
    cout.flush();   //maybe uselles
    
    free(additional_info);
    free(report_buffer);
    free(error_buffer);
    exit(0);

}