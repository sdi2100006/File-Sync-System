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


#define MAX 5
#define READ 0
#define WRITE 1

using namespace std;

int main() {
    int fd_fss_in, fd_fss_out;
    
    unordered_map<string, sync_info_struct*> sync_info_mem_store;
    queue<string> jobs_queue;



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

    //read config file and store the info into sync_info_mem_store
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

        /*Making a new struct to keep info about this directory*/
        sync_info_struct* sync_info_struct_ptr = new sync_info_struct;
        sync_info_struct_ptr->source = source;
        sync_info_struct_ptr->destination = destination;
        sync_info_struct_ptr->status = "starting";
        sync_info_struct_ptr->active = true;
        sync_info_struct_ptr->last_sync_time=0;
        /*Put the sruct pointer containing info about the directory in the hash map*/
        sync_info_mem_store[source] = sync_info_struct_ptr;
    }

    //start the inotify thing 
    int inotify_fd = inotify_init();
    if (inotify_fd==-1) {
        perror("inotify_init problem");
        exit(1);
    }
    //start watching all the stored directories by storing the watch descriptor in the sync_info_mem_store 
    for (auto it=sync_info_mem_store.begin() ; it != sync_info_mem_store.end() ; ++it) {
        it->second->wd = inotify_add_watch(inotify_fd, it->second->source.c_str(), IN_MODIFY | IN_CREATE | IN_DELETE);
        if (it->second->wd == -1) {
            perror("inotify_add_wach problem");
            exit(1);
        }
    }



    /*
    for (auto it=sync_info_mem_store.begin() ; it !=sync_info_mem_store.end() ; ++it) {
        cout << it->second->source << " " << it->second->destination << endl;
    }*/

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
    return 0;
}