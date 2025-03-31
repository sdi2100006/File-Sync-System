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

#define MAX 5
#define READ 0
#define WRITE 1

using namespace std;

int main() {
    int fd_fss_in, fd_fss_out;

    //create 2 named pipes (fss_in and fss_out) to communincate with fss_console
    //create fss_in for reading 
    if (mkfifo("fss_in", 0666) == -1) {
        perror("Failed to create fss_in");
        exit(1);
    }
    //create fss_out for writing 
    if (mkfifo("fss_out", 0666) == -1) {
        perror("Failed to create fss_out");
        exit(1);
    }

    //open fss_in for reading *NOT YET*
    /*if ( (fd_fss_in = open("fss_in", O_RDONLY | O_NONBLOCK) ) < 0) {
        perror("Failed to open fss_in");
        exit(1);
    }*/

    //open fss_out for writing  **ΝΟΤ ΥΕΤ BECAUSE IT BLOCKS**
    /*if ( (fd_fss_out = open("fss_out", O_WRONLY | O_NONBLOCK) ) < 0) {
        perror("Failed to open fss_out");
        exit(1);
    }*/

    //read config file and fork processes or store them in queue if more than MAX
    ifstream configFile("config.txt");
    if (!configFile) {
        perror("Failed to open config.txt");
        exit(1);
    }
    string line, source, destination;
    int workers_count = 0;
    while (getline(configFile, line) && workers_count < MAX) {
        istringstream iss(line);
        iss >> source >> destination;
        cout << "Source: " << source << ", Destination: " << destination << endl;

        pid_t childpid;
        childpid = fork();
        workers_count++;
        if (childpid == -1) {
            perror("Failed to fork");
            exit(1);
        }

        if(childpid == 0) { // Child process
            printf("I am the child process with PID: %lu\n", (long)getpid());
        } else {    // Parent process
            printf("I am the parent process with PID: %lu\n", (long)getpid());
        }
    }
    return 0;
}