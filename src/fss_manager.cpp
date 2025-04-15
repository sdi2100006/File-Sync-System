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
#include <time.h>
#include <poll.h>


#define READ 0
#define WRITE 1

using namespace std;

volatile sig_atomic_t terminated_workers;

char* get_current_time();
int get_random_index(int);

void termination_signal_handler (int signum) {

    int status;
    pid_t pid;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        terminated_workers++;
    }

}

int main(int argc, char* argv[]) {

    int fd_fss_in, fd_fss_out, opt, worker_limit=5;
    string manager_logfile, config_file;
    
    unordered_map<string, sync_info_struct*> sync_info_mem_store;
    queue< pair<string, string> > jobs_queue;

    signal(SIGCHLD, termination_signal_handler);
    srand(time(NULL));

    //read from command line 
    while ((opt = getopt(argc, argv, "l:c:n:")) != -1) {
        switch (opt) {
            case 'l':
                manager_logfile = optarg;
                break;
            case 'c':
                config_file = optarg;
                break;
            case 'n': 
                worker_limit = atoi(optarg);
                break;
            default:
                cout << "wrong call" << endl;
                exit(1);
        }
    }

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
    ifstream configFile(config_file);
    if (!configFile) {
        perror("Failed to open config.txt");
        exit(1);
    }
    string line, source, destination;
    while (getline(configFile, line)) {
        istringstream iss(line);
        iss >> source >> destination;
        //cout << "Source: " << source << ", Destination: " << destination << endl;

        /*Making a new struct to keep info about this directory*/
        sync_info_struct* sync_info_struct_ptr = new sync_info_struct;
        sync_info_struct_ptr->source = source;
        sync_info_struct_ptr->destination = destination;
        sync_info_struct_ptr->active = true;
        //sync_info_struct_ptr->last_sync_time=NULL;
            
        /*Put the sruct pointer containing info about the directory in the hash map*/
        sync_info_mem_store[source] = sync_info_struct_ptr;
        jobs_queue.push({sync_info_struct_ptr->source, sync_info_struct_ptr->destination});

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

    //start the initial sync
    int poll_ready, workers_count=0 , pipe_descriptor_table[worker_limit];
    pollfd poll_fds[worker_limit];  
    for (int i=0 ; i<worker_limit ; i++) {  //initialize
        poll_fds[i].fd = -1;
        poll_fds[i].events = -1;
        poll_fds[i].revents = -1;
    }

    while (!jobs_queue.empty() || workers_count > 0) {  //THIS WILL CHANGE 

        while (!jobs_queue.empty() && workers_count < worker_limit) {   //worker spawning loop
            //cout << "workers count: " << workers_count << endl;
            pair <string, string> source_dest = jobs_queue.front();
            jobs_queue.pop();
            //cout << "poped from queue, source: " << source_dest.first << " dest: " << source_dest.second << endl;

            /*Fork the initial ΜΑΧ_WORKERS*/
            int pipe_fd[2]; //pipe for worker - manager communication
            if (pipe(pipe_fd)==-1){ perror("pipe"); exit(1);}
            //pipe_descriptor_table[workers_count] = pipe_fd[READ]; //keep the read end of the active pipes

            pid_t workerpid;
            workerpid = fork();
            if (workerpid == -1) {
                perror("Failed to fork");
                exit(1);
            }
            if (workerpid == 0) {   //child
                close(pipe_fd[READ]);   //child is only writing
                dup2(pipe_fd[WRITE], 1);    //redirect stdout to point at the pipe. Now everything that the child prints goes to the pipe 
                //close(pipe_fd[WRITE]); //aparenetly its not needed because of dup2
                int retval = execl("./worker", "./worker", source_dest.first.c_str(), source_dest.second.c_str(), "ALL", "FULL", NULL);
                if(retval == -1) {
                    perror("execl");
                    exit(1);
                }
            } else {    //parent
                cout << "started worker with pid: " << workerpid << endl;
                close(pipe_fd[WRITE]);  //parent only reads from child
                int index;
                for (int j=0 ; j<worker_limit ; j++) {  //find the first available slot in the table 
                    if (poll_fds[j].fd == 0) {
                        index = j;
                        break;
                    }
                }
                poll_fds[index].fd = pipe_fd[READ]; //add the pipe fd in poll for monitoring 
                poll_fds[index].events = POLLIN;    //monitor only if a fd is ready for reading 
                workers_count++;
            }   
        }

        int poll_ready = poll(poll_fds, worker_limit, 100); //non blocking poll
        if (poll_ready == -1) {
            if (errno == EINTR) {   //this is for the error "poll interupted by signal SIGCHLD "
                continue;
            } else {
                perror("poll");
                exit(1);
            }
        }
        for (int i=0 ; i<worker_limit; i++) {   //loop through all the fds and read if someone is ready
            if (poll_fds[i].revents & POLLIN) {
                char buff[256];
                //cout << "reading ..." << endl;
                ssize_t s = read(poll_fds[i].fd, buff, sizeof(buff));
                buff[s] = '\0';
                cout << buff << endl;
                close(poll_fds[i].fd);  //not needed any more, initialize again so another descriptor can be placed in its position
                poll_fds[i].fd=-1;
                poll_fds[i].events=-1;
                poll_fds[i].revents=-1;
                //cout << "emptied the space" << endl;
            }
        }
        if (terminated_workers > 0) {   //from the signal handler
            workers_count -= terminated_workers;
            terminated_workers = 0;     //reset the flag
        }
    }
    //cout << workers_count << endl;
    return 0;
}


char* get_current_time () {
    time_t raw_time;
    struct tm *time_info;
    static char formatted_time[20];

    time(&raw_time);
    time_info = localtime(&raw_time);
    strftime(formatted_time, sizeof(formatted_time), "%F %T", time_info);

    return formatted_time;
}

int get_random_index(int worker_limit) {
    return rand() % worker_limit;
}