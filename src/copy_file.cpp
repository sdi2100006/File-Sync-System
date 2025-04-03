#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <iostream>

#define SIZE 30
#define PERM 0644

using namespace std;

int copy_file(const char* name1, const char* name2, int BUFFSIZE) {
    int infile, outfile;
    ssize_t nread;
    char buffer[BUFFSIZE];

    //checks
    if ( (infile=open(name1, O_RDONLY)) == -1) {
        perror("copy file: open");
        return -1;
    }
    if ( (outfile=open(name2, O_WRONLY|O_CREAT|O_TRUNC, PERM)) == -1) {
        perror("copy file: open");
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

int main() {
    int status=0;

    status = copy_file("config.txt", "config_copy.txt", 4096);
    exit(status);

}