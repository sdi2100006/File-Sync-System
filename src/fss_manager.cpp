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
#include "../include/utilities.hpp"

#define READ 0
#define WRITE 1
#define REPORT_SIZE 256

using namespace std;

volatile sig_atomic_t terminated_workers;

typedef struct job {
    string source;
    string dest;
    string operation;
    string filename;
    bool fromconsole;
    bool sync;
} job_struct;

typedef struct report_info{
    string timestamp;
    string source;
    string dest;
    pid_t pid;
    string operation;
    string status;
    string errors;
    int num;
} report_info_struct;

int parse_report(string, report_info_struct&);

void termination_signal_handler (int signum) {

    int status;
    pid_t pid;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        terminated_workers++;
    }
}

int main(int argc, char* argv[]) {

    int fd_fss_in, fd_fss_out, opt, worker_limit=5, total_jobs=0;
    string manager_logfile, config_file;
    int sync_fd=-1;
    char special_message[250];
    
    
    unordered_map<string, sync_info_struct*> sync_info_mem_store;
    //queue< pair<string, string> > jobs_queue;
    queue< job_struct > jobs_queue;

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
    if ( (fd_fss_in = open("fss_in", O_RDONLY | O_NONBLOCK) ) < 0) {
        perror("Failed to open fss_in");
        exit(1);
    }

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
        sync_info_struct_ptr->active = false;
        sync_info_struct_ptr->monitored = false;
        sync_info_struct_ptr->status = "Inactive";
        //sync_info_struct_ptr->last_sync_time=NULL;
            
        /*Put the sruct pointer containing info about the directory in the hash map*/
        sync_info_mem_store[source] = sync_info_struct_ptr;
        job_struct job;
        job.source = sync_info_struct_ptr->source;
        job.dest = sync_info_struct_ptr->destination;
        job.operation = "FULL";
        job.filename = "ALL";
        job.fromconsole = false;
        job.sync = false;
        jobs_queue.push(job);
    }

    //create and open  logfile 
    ofstream logfile(manager_logfile);

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
    int poll_ready, workers_count=0;
    pollfd poll_fds[worker_limit]={0};
    pollfd poll_inotify_fds;
    poll_inotify_fds.fd = inotify_fd;
    poll_inotify_fds.events = POLLIN; 

    while (true) {
    
        while (!jobs_queue.empty() && workers_count < worker_limit) {   //worker spawning loop

            job_struct job = jobs_queue.front();
            jobs_queue.pop();
            total_jobs++;
            //cout << "op " << job.operation << "sync" << job.sync <<"fromson " << job.fromconsole << " " << job.source<< " " << job.dest<< " " << job.filename <<endl;
            //if (job.operation == "FULL" && job.sync == false) {
            if (job.operation == "FULL" && job.sync == false && (sync_info_mem_store.find(job.source) == sync_info_mem_store.end() || sync_info_mem_store[job.source]->monitored == false)) {
                cout << "[" << get_current_time() << "]" << " Added directory: " << job.source << " -> " << job.dest << endl;
                logfile << "[" << get_current_time() << "]" << " Added directory: " << job.source << " -> " << job.dest << endl;
        
                cout << "[" << get_current_time() << "]" << " Monitoring started for " << job.source << endl;
                logfile << "[" << get_current_time() << "]" << " Monitoring started for " << job.source << endl;

                sync_info_mem_store[job.source]->monitored = true;
                sync_info_mem_store[job.source]->status = "Active";

                if (job.fromconsole == true) {
                    char message[250];
                    snprintf(message, 250, "[%s] Added directory: %s -> %s\nMonitoring started for %s\n", get_current_time(), job.source.c_str(), job.dest.c_str(), job.source.c_str());
                    if ( (fd_fss_out = open("fss_out", O_WRONLY | O_NONBLOCK) ) < 0) {
                        perror("Failed to open fss_out");
                        exit(1);
                    }
                    write(fd_fss_out, message, sizeof(message));
                    close(fd_fss_out);
                }
            } else if (job.operation == "FULL" && job.sync == true) {
                cout << "[" << get_current_time() << "]" << " Syncing directory: " << job.source << " -> " << job.dest << endl;
                logfile << "[" << get_current_time() << "]" << " Syncing directory: " << job.source << " -> " << job.dest << endl;
                char message[250];
                snprintf(message, 250, "[%s] Syncing directory: %s -> %s\n", get_current_time(), job.source.c_str(), job.dest.c_str());

                if ( (fd_fss_out = open("fss_out", O_WRONLY | O_NONBLOCK) ) < 0) {
                    perror("Failed to open fss_out");
                    exit(1);
                }
                write(fd_fss_out, message, sizeof(message));
                close(fd_fss_out);
            }
            /*
            auto it = sync_info_mem_store.find(job.source);
            if (job.fromconsole == true && job.operation == "FULL" && job.sync == false && it == sync_info_mem_store.end() ) {
                sync_info_struct* sync_info_struct_ptr = new sync_info_struct;
                sync_info_struct_ptr->source = job.source;
                sync_info_struct_ptr->destination = job.dest;
                sync_info_struct_ptr->active = true;
                sync_info_struct_ptr->monitored = true;
                sync_info_struct_ptr->wd = inotify_add_watch(inotify_fd, job.source.c_str(), IN_MODIFY | IN_CREATE | IN_DELETE);
                if (sync_info_struct_ptr->wd == -1) {
                    perror("inotify_add_wach problem");
                    exit(1);
                }
                    
                sync_info_mem_store[job.source] = sync_info_struct_ptr;
            }*/

            /*Fork the initial ΜΑΧ_WORKERS*/
            int pipe_fd[2]; //pipe for worker - manager communication
            if (pipe(pipe_fd)==-1){ perror("pipe"); exit(1);}
            if (job.sync == true) {
                sync_fd = pipe_fd[READ];
            }
            //pipe_descriptor_table[workers_count] = pipe_fd[READ]; //keep the read end of the active pipes
            //cout << job.source << " " << job.dest << " " << job.filename << " " << job.operation << endl;

            pid_t workerpid;
            workerpid = fork();
            if (workerpid == -1) {
                perror("Failed to fork");
                exit(1);
            }
            if (workerpid == 0) {   //child
                close(pipe_fd[READ]);   //child is only writing
                dup2(pipe_fd[WRITE], 1);    //redirect stdout to point at the pipe. Now everything that the child prints goes to the pipe 
                close(pipe_fd[WRITE]); //aparenetly its not needed because of dup2
                //cout << job.source << " " << job.dest << " " << job.filename << " " << job.operation << endl;
                int retval = execl("./worker", "./worker", job.source.c_str(), job.dest.c_str(), job.filename.c_str(), job.operation.c_str(), NULL);
                if(retval == -1) {
                    perror("execl");
                    exit(1);
                } else {
                    //cout << "oook" << endl;
                }
            } else {    //parent
                //cout << "started worker with pid: " << workerpid << endl;
                close(pipe_fd[WRITE]);  //parent only reads from child
                sync_info_mem_store[job.source]->active = true;
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
                char exec_report[REPORT_SIZE];
                ssize_t s = read(poll_fds[i].fd, exec_report, REPORT_SIZE);
                //cout << "report: " << exec_report << endl;
                //cout << "report end" << endl;
                //parse the exec report and store values into variables
                report_info_struct report_info;
                if (parse_report(exec_report, report_info) < 0) {
                    perror("parse_report");
                    exit(-1);
                }
                //sync_info_mem_store[report_info.source]->active = false;
                //this case is only for when we have a sync command
                //cout << "fd: " << poll_fds[i].fd << " sync fd: " << sync_fd<< endl;
                if (poll_fds[i].fd == sync_fd) {
                    //cout << "yeeeesss" << endl;
                    sync_fd = -1;
                    cout << "[" << get_current_time() << "] " <<"Sync completed " << report_info.source << " -> " << report_info.dest << " Errors:" << sync_info_mem_store[report_info.source]->error_count << endl;
                    logfile << "[" << get_current_time() << "] " <<"Sync completed " << report_info.source << " -> " << report_info.dest << " Errors:" << sync_info_mem_store[report_info.source]->error_count << endl;
                    char message[250];
                    snprintf(message, 250, "[%s] Sync completed: %s -> %s Errors:%d\n", get_current_time(), report_info.source.c_str(), report_info.dest.c_str(), sync_info_mem_store[report_info.source]->error_count);
                    if ( (fd_fss_out = open("fss_out", O_WRONLY | O_NONBLOCK) ) < 0) {
                        perror("Failed to open fss_out");
                        exit(1);
                    }
                    write(fd_fss_out, message, sizeof(message));
                    close(fd_fss_out);
                } else {
                    logfile << "[" << report_info.timestamp << "] " << "[" << report_info.source << "] " << "[" << report_info.dest << "] " << "[" << report_info.pid << "] " << "[" << report_info.operation << "] " << "[" << report_info.status << "] " << "[" << report_info.errors << "] " << endl;
                }
                strcpy(sync_info_mem_store[report_info.source]->last_sync_time, report_info.timestamp.c_str());
                sync_info_mem_store[report_info.source]->active = false;
                sync_info_mem_store[report_info.source]->error_count = report_info.num;
                
                close(poll_fds[i].fd);  //not needed any more, initialize again so another descriptor can be placed in its position
                poll_fds[i].fd=0;
                poll_fds[i].events=0;
                poll_fds[i].revents=0;
            }
        }

        //poll for the inotify 
        int poll_inotify = poll(&poll_inotify_fds, 1, 100);
        if (poll_inotify == -1) {
            if (errno == EINTR) {   //this is for the error "poll interupted by signal SIGCHLD "
                continue;
            } else {
                perror("poll");
                exit(1);
            }
        }
        char inotify_buffer[1024];
        if (poll_inotify_fds.revents & POLLIN) {    //there is data to read from inotify
            const struct inotify_event *event;
            int length = read(inotify_fd, inotify_buffer, 1024);
            int i=0;
            while (i < length) {
                struct inotify_event* event = (struct inotify_event*)&inotify_buffer[i];
                job_struct new_job;
                //search sync_info_mem_store to find the wd
                for (auto it=sync_info_mem_store.begin() ; it != sync_info_mem_store.end() ; ++it) {
                    if (it->second->wd == event->wd) {
                        new_job.source = it->first;
                        new_job.dest = it->second->destination;
                        new_job.filename = event->name;
                    }
                }
                if (event->mask & IN_CREATE) {
                    new_job.operation = "ADDED";
                    //cout << "File created: " << event->name << "\n";
                } else if (event->mask & IN_MODIFY) {
                    new_job.operation = "MODIFIED";
                    //cout << "File modified: " << event->name << "\n";
                } else if (event->mask & IN_DELETE) {
                    new_job.operation = "DELETED";
                    //cout << "File deleted: " << event->name << "\n";
                } else {
                    i += sizeof(struct inotify_event) + event->len; //SKIP
                    continue;
                }
                new_job.fromconsole = false;
                new_job.sync = false;
                //add the new job to the queue
                jobs_queue.push(new_job);            
                i += sizeof(struct inotify_event) + event->len;
            }
        }

        if (terminated_workers > 0) {   //from the signal handler
            workers_count -= terminated_workers;
            terminated_workers = 0;     //reset the flag
        }

        //check for command from console
        char console_buffer[256] = {0};
        ssize_t s = read(fd_fss_in, console_buffer, 256);
        if (s > 0) {
            console_buffer[s] = '\0';
            vector<string> command_parts;
            string command(console_buffer);
            string token;
            istringstream iss(command);
            while (iss >> token) {
                command_parts.push_back(token);
            }

            //handle which command
            if (command_parts[0] == "add") {
                job_struct new_job;
                new_job.source = command_parts[1];
                new_job.dest = command_parts[2];
                new_job.filename = "ALL";
                new_job.operation = "FULL";
                new_job.fromconsole = true;
                new_job.sync = false;


                auto it = sync_info_mem_store.find(new_job.source);
                if (it != sync_info_mem_store.end() && it->second->monitored == true /*|| it->second->monitored == false sync_info_mem_store[new_job.source]->monitored == false*/) {
                    char message[256];
                    snprintf(message, 256, "[%s] Already in queue: %s\n", get_current_time(), new_job.source.c_str());
                    cout << message << endl;
                    if ( (fd_fss_out = open("fss_out", O_WRONLY | O_NONBLOCK) ) < 0) {
                        perror("Failed to open fss_in");
                        exit(1);
                    }
                   ssize_t n = write(fd_fss_out, message, sizeof(message));
                   close(fd_fss_out);
                   if (n<=0){
                    perror("error writing to fss_out");
                   }
                    continue;
                }

                if (it == sync_info_mem_store.end()) {  //if you didnt find it 
                    /*Making a new struct to keep info about this directory*/
                    sync_info_struct* sync_info_struct_ptr = new sync_info_struct;
                    sync_info_struct_ptr->source = new_job.source;
                    sync_info_struct_ptr->destination = new_job.dest;
                    sync_info_struct_ptr->active = false;
                    sync_info_struct_ptr->monitored = false;
                    sync_info_struct_ptr->status = "Active";
                    sync_info_struct_ptr->wd = -1;
                    //add it to the sync info mem store 
                    sync_info_mem_store[new_job.source] = sync_info_struct_ptr;
                } else { //found it but monitored == false 
                   // sync_info_mem_store[new_job.source]->status = "Active";
                //ync_info_mem_store[new_job.source]->monitored = true;
                }
                //restart the monitoring 
                
                sync_info_mem_store[new_job.source]->wd = inotify_add_watch(inotify_fd, sync_info_mem_store[new_job.source]->source.c_str(), IN_MODIFY | IN_CREATE | IN_DELETE);
                //cout << "ok" << endl;
                jobs_queue.push(new_job);


            } else if (command_parts[0] == "sync") {
                //search if there is an active job with this source dir
                //cout << "sdfvff: " << command_parts[1] << endl;
                if (sync_info_mem_store[command_parts[1]]->active == true) {   
                    char message[256];
                    snprintf(message, 256, "[%s] Sync already in progress: %s\n", get_current_time(), command_parts[1].c_str());
                    cout << message << endl;
                    if ( (fd_fss_out = open("fss_out", O_WRONLY | O_NONBLOCK) ) < 0) {
                        perror("Failed to open fss_in");
                        exit(1);
                    }
                   ssize_t n = write(fd_fss_out, message, sizeof(message));
                   close(fd_fss_out);
                   if (n<=0){
                    perror("error writing to fss_out");
                   }
                   continue;
                }   //if there is no active job with this source dir
                job_struct new_job;
                new_job.source = command_parts[1];
                new_job.dest = sync_info_mem_store[new_job.source]->destination;
                new_job.filename = "ALL";
                new_job.operation = "FULL";
                new_job.fromconsole = true;
                new_job.sync = true;
                jobs_queue.push(new_job);

            } else if (command_parts[0] == "status") {
                auto it = sync_info_mem_store.find(command_parts[1]);
                if (it == sync_info_mem_store.end()) {  //wasnt found
                    char message[256];
                    snprintf(message, 256, "[%s] Directory not monitored: %s\n", get_current_time(),  command_parts[1].c_str());
                    cout << message << endl;
                    if ( (fd_fss_out = open("fss_out", O_WRONLY | O_NONBLOCK) ) < 0) {
                        perror("Failed to open fss_in");
                        exit(1);
                    }
                    ssize_t n = write(fd_fss_out, message, sizeof(message));
                    close(fd_fss_out);
                    if (n<=0){
                        perror("error writing to fss_out");
                    }
                    continue;
                } 
                //was found
                char message[256];
                snprintf(message, 256, "[%s] Status requested for %s\nDirectory: %s\nTarget: %s\nLast Sync: %s\nErrors: %d\nStatus: %s\n", get_current_time(), command_parts[1].c_str(), command_parts[1].c_str(), sync_info_mem_store[command_parts[1]]->destination.c_str(), sync_info_mem_store[command_parts[1]]->last_sync_time, sync_info_mem_store[command_parts[1]]->error_count, sync_info_mem_store[command_parts[1]]->status.c_str());
                cout << message << endl;
                if ( (fd_fss_out = open("fss_out", O_WRONLY | O_NONBLOCK) ) < 0) {
                    perror("Failed to open fss_in");
                    exit(1);
                }
                ssize_t n = write(fd_fss_out, message, sizeof(message));
                close(fd_fss_out);
                if (n<=0){
                    perror("error writing to fss_out");
                }
                //cout << "[" << get_current_time() << "]" << " Status requested for " << command_parts[1] << endl << "Directory: " << command_parts[1] << endl << "Target: " << sync_info_mem_store[command_parts[1]]->destination << endl << "Last Sync: " << sync_info_mem_store[command_parts[1]]->last_sync_time << endl << "Errors: " << sync_info_mem_store[command_parts[1]]->error_count << endl << "Status: " << sync_info_mem_store[command_parts[1]]->status << endl;

            }else if (command_parts[0] == "cancel") {

                //remove from inotify
                auto it = sync_info_mem_store.find(command_parts[1]);
                if (it == sync_info_mem_store.end() || it->second->monitored == false) {    //if it doesnt exist or is not monitored
                    char message[256];
                    snprintf(message, 256, "[%s] Directory not monitored: %s\n", get_current_time(), command_parts[1].c_str());
                    cout << message << endl;
                    if ( (fd_fss_out = open("fss_out", O_WRONLY | O_NONBLOCK) ) < 0) {
                        perror("Failed to open fss_in");
                        exit(1);
                    }
                    ssize_t n = write(fd_fss_out, message, sizeof(message));
                    close(fd_fss_out);
                    if (n<=0){
                        perror("error writing to fss_out");
                    }
                    continue;
                }
                //if it is currently monitored
                if(inotify_rm_watch(inotify_fd, sync_info_mem_store[command_parts[1]]->wd) == -1) {
                    perror("problem removing watch descriptor");
                    exit(-1);
                }
                sync_info_mem_store[command_parts[1]]->wd = -1;
                sync_info_mem_store[command_parts[1]]->monitored = false;
                sync_info_mem_store[command_parts[1]]->status = "Inactive";
                char message[256];
                snprintf(message, 256, "[%s] Monitoring stopped for %s\n", get_current_time(), command_parts[1].c_str());
                cout << message << endl;
                if ( (fd_fss_out = open("fss_out", O_WRONLY | O_NONBLOCK) ) < 0) {
                    perror("Failed to open fss_in");
                    exit(1);
                }
                ssize_t n = write(fd_fss_out, message, sizeof(message));
                close(fd_fss_out);
                if (n<=0){
                    perror("error writing to fss_out");
                }

            } else if (command_parts[0] == "shudown") {

            } else {
                perror(" the command i just read is wrong\n");
            }
        }
    }
    logfile.close();
    return 0;
}

int parse_report(string exec_report, report_info_struct& report_info) {

    string line;

    istringstream iss(exec_report);
    char splitter = '=';
    while(getline(iss, line)) {
        size_t splitter_pos = line.find(splitter);
        string key = line.substr(0, splitter_pos);
        string value = line.substr(splitter_pos+1);

        if (key == "TIMESTAMP") {
            report_info.timestamp = value;
        } else if (key == "SOURCE") {
            report_info.source = value;
        } else if (key == "DEST") {
            report_info.dest = value;
        } else if (key == "PID") {
            report_info.pid = atoi(value.c_str());
        } else if (key == "OP") {
            report_info.operation = value;
        } else if (key == "STATUS") {
            report_info.status = value;
        } else if (key == "ERRORS") {
            report_info.errors = value;
        } else if (key == "NUM") {
            report_info.num = atoi(value.c_str());
        }

    }

    return 0;
}