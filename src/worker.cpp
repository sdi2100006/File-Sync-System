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

using namespace std;

#define SIZE 30
#define PERM 0644
#define BUFFSIZE 4096

//copy file name1 to file name2
int copy_file(const char* name1, const char* name2) {
    int infile, outfile;
    ssize_t nread;
    char buffer[BUFFSIZE];

    //checks
    if ( (infile=open(name1, O_RDONLY)) == -1) {
        perror("open name1");
        return -1;
    }
    if ( (outfile=open(name2, O_WRONLY|O_CREAT|O_TRUNC, PERM)) == -1) {
        perror("open name2");
        close(infile);
        return -2;
    }
    //now we have both files open

    while ( (nread=read(infile, buffer, BUFFSIZE)) > 0) {
        if ( write(outfile, buffer, nread) < nread) {
            close(infile);
            close(outfile);
            return -3;
        }
    }
    
    close(infile);
    close(outfile);

    if (nread == -1) {
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
        perror("opendir");
    } else {
        while ( (direntp=readdir(dir_ptr)) != NULL ) {

            if ( strcmp(direntp->d_name, ".") == 0 || strcmp(direntp->d_name, "..") == 0 )
                continue;

            //cout << "inode: " << direntp->d_ino << " name: " << direntp->d_name << endl; 
            files.push_back(direntp->d_name);
        }
        closedir(dir_ptr);
    }
    return files;
}


int main(int argc, char *argv[]) {
    //process args MAYBE MAKE THEM STRINGS - NO KEEP EVERYTHING CRITICAL AS A CHAR* OR A TABLE ?
    cout << "i started" << endl; 
    char* source = argv[1];
    char* destination = argv[2];
    char* filename = argv[3]; //if we want a full sync  it should be ALL
    char* operation = argv[4]; //FULL, ADDED, MODIFIED, DELETED

    if ( strcmp(operation, "FULL") == 0 ) {
        //parse the directory and call copy for every file.
        vector<char*> file_names = parse_directory(source); //returns the file names of the directory in the vector
        for (int i=0 ; i<file_names.size() ; i++) {
            cout << file_names[i] << " ";
        }
        cout << endl;

        for (int i=0 ; i<file_names.size() ; i++) {
            //char *temp_str;
            char source_temp_str[50];
            char dest_temp_str[50];

            //cool solution to concat 2 strings because segfault with strcat I HAVE FUCKED UP WITH STRINGS AND CHAR*
            snprintf(source_temp_str, sizeof(source_temp_str), "%s/%s", source, file_names[i]);
            snprintf(dest_temp_str, sizeof(dest_temp_str), "%s/%s", destination, file_names[i]);
            
            cout << "temp_str " << source_temp_str <<endl;
            cout << "dest_str " << dest_temp_str <<endl;

            if( copy_file(source_temp_str, dest_temp_str) != 0 ) { 
                perror("FULL: copy_file");
                exit(1);
            }
        }
 
    } else if ( strcmp(operation, "ADDED") == 0 ) {
        //copy the requested file to the source directory (if a file with the same name exists ????)
        if( copy_file(filename, destination) != 0 ) { 
            perror("ADD: copy_file");
            exit(1);
        }
            
    } else if ( strcmp(operation, "MODIFIED") == 0) {
        //do modified
    } else if ( strcmp(operation, "DELETED") == 0) {
        //do deleted
    }



}