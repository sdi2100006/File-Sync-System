#include "../include/worker_helpers.hpp"
#include <string>
#include <fcntl.h>
#include <dirent.h>
#include <cstring>
#include <iostream>
#include <unistd.h>


int copy_file(const char* name1, const char* name2, char* error_buffer) {
    int infile, outfile;
    size_t nread;
    char buffer[BUFFSIZE];

    //checks
    if ( (infile=open(name1, O_RDONLY)) == -1) {
        snprintf(error_buffer, SIZE, "Can't open source file '%s' during copy. %s", name1, strerror(errno));
        //cout << "error_buffer: " << error_buffer << endl;
        return -5;
        //exit(-2);
    }
    if ( (outfile=open(name2, O_WRONLY|O_CREAT|O_TRUNC, PERM)) == -1) {
        snprintf(error_buffer, SIZE, "Can't open dest file '%s' during copy. %s", name2, strerror(errno));
        //cout << "error_buffer: " << error_buffer << endl;
        return -5;
        //exit(-2);
    }

    while ( (nread=read(infile, buffer, BUFFSIZE)) > 0) {
        if ( write(outfile, buffer, nread) < nread) {
            close(infile);
            close(outfile);
            snprintf(error_buffer, SIZE, "error writing to file '%s' during copy. %s", name2, strerror(errno));
            //cout << "error_buffer: " << error_buffer << endl;
            return -3;
        }
    }
    if (nread == -1) {
        close(infile);
        close(outfile);
        snprintf(error_buffer, SIZE, "error reading from file '%s' during copy. %s", name1, strerror(errno));
        //cout << "error_buffer: " << error_buffer << endl;
        return -4;
    }
    if (close(infile) == -1) {
        snprintf(error_buffer, SIZE, "error closing source file '%s' during copy. %s", name1, strerror(errno));
        //cout << "error_buffer: " << error_buffer << endl;
        return -4;
    }
    if (close(outfile) == -1) {
        snprintf(error_buffer, SIZE, "error closing dest file '%s' during copy. %s", name2, strerror(errno));
        //cout << "error_buffer: " << error_buffer << endl;
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

            if ( strcmp(direntp->d_name, ".") == 0 || strcmp(direntp->d_name, "..") == 0 ) {
                continue;
            }

            char* file;

            files.push_back(strdup(direntp->d_name));   //insert a duplacate of d_name i need to free them after SOS!!!
        }
        closedir(dir_ptr);
    }
    return files;
}