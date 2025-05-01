#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <unordered_map>
#include <queue>
#include <sys/inotify.h>
#include <time.h>
#include <poll.h>
#include "../include/utilities.hpp"
#include <deque>
#include "../include/manager_helpers.hpp"

using namespace std;

volatile sig_atomic_t terminated_workers;

void termination_signal_handler (int signum) {
    int status;
    pid_t pid;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {     //kill zombie porcesses
        terminated_workers++;   //keep track of how many you killed so that i can do workers_cout - terminated workers in the loop
    }
}

int main(int argc, char* argv[]) {

    int fd_fss_in, fd_fss_out, opt, worker_limit=5, sync_fd=-1;
    string manager_logfile, config_file;
    bool shutdown = false;
    
    unordered_map<string, sync_info_struct*> sync_info_mem_store;
    deque <job_struct> jobs_queue;

    srand(time(NULL));
    signal(SIGCHLD, termination_signal_handler);    //initialize signal handler 

    parse_commandline(argc, argv, manager_logfile, config_file, worker_limit);
    if (worker_limit <= 0) {
        worker_limit = 5;   //default
    }

    cleanup(manager_logfile);   //deletes managerlogfile, and pipes

    create_named_pipes();

    //open fss_in for reading 
    if ( (fd_fss_in = open("fss_in", O_RDONLY | O_NONBLOCK) ) < 0) {
        perror("Failed to open fss_in");
        exit(1);
    }

    //read config file and store the info into sync_info_mem_store 
    read_config_and_init_struct(config_file, sync_info_mem_store, jobs_queue);

    //create and open  logfile 
    ofstream logfile(manager_logfile);

    //init inotify 
    int inotify_fd = inotify_init();
    if (inotify_fd==-1) {
        perror("inotify_init problem");
        exit(1);
    }

    int poll_ready, workers_count=0;
    pollfd poll_fds[worker_limit]={0};
    pollfd poll_inotify_fds;
    poll_inotify_fds.fd = inotify_fd;
    poll_inotify_fds.events = POLLIN; 
    char sync_response[MESSAGE_SIZE];

    while (true) {  //main server loop

        if (!jobs_queue.empty() && workers_count < worker_limit) {  //worker spawning condition
            
            job_struct job = jobs_queue.front();
            jobs_queue.pop_front();

            /*Log and print messages and make some changes to the sync info mem data struct*/
            handle_and_log_job(job, sync_response, shutdown, logfile, sync_info_mem_store, fd_fss_out, inotify_fd);

            /*Forking logic*/
            spawn_workers(job, sync_fd, sync_info_mem_store, worker_limit, poll_fds, workers_count);
        }
    
        /*Rreading exec reports from workers if they exist and handling them appropriatly */
        int code = handle_exec_report_events(poll_fds, worker_limit, sync_fd, sync_response, logfile, fd_fss_out, sync_info_mem_store);
        if (code == 1) {
            continue;
        }

        /*Reading events from inotify */
        code = handle_inotify_events(shutdown, poll_inotify_fds, inotify_fd, sync_info_mem_store, jobs_queue);
        if (code == 1) {
            continue;
        }

        /*Reading commands from console*/
        code = handle_command(fd_fss_in, fd_fss_out, inotify_fd, sync_info_mem_store, jobs_queue, shutdown);
        if (code == 1) {
            continue;
        }

        /*Reniew how many workers we have active at the moment*/
        if (terminated_workers > 0) {   //from the signal handler
            workers_count -= terminated_workers;
            terminated_workers = 0;     //reset the flag
        }

        if (jobs_queue.empty() && workers_count == 0 && shutdown == true) {     //loop exiting condiiton for termiantion
            break;
        }
    }

    //free memmory from he sync info mem store
    for(auto it = sync_info_mem_store.begin() ; it != sync_info_mem_store.end() ; ++it) {
        delete it->second;
    }

    if ( (fd_fss_out = open("fss_out", O_WRONLY | O_NONBLOCK) ) < 0) {
        perror("Failed to open fss_in");
        exit(1);
    }
    char message[MESSAGE_SIZE];
    snprintf(message, MESSAGE_SIZE, "[%s] Manager shutdown complete", get_current_time());
    cout << message << endl;
    ssize_t n = write(fd_fss_out, message, strlen(message));
    if (n<=0){
        perror("error writing to fss_out");
    }
    logfile.close();
    close(fd_fss_out);

    return 0;
}