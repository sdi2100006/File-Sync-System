#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <unordered_map>
#include "../include/sync_info_mem_store.hpp"
#include <queue>
#include <sys/inotify.h>
#include <time.h>
#include <poll.h>
#include "../include/utilities.hpp"
#include <deque>

#define READ 0
#define WRITE 1
#define REPORT_SIZE 256
#define MESSAGE_SIZE 256
#define INOTIFY_SIZE 4096

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
    bool shutdown = false;
    
    unordered_map<string, sync_info_struct*> sync_info_mem_store;
    deque <job_struct> jobs_queue;

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
                cout << "wrong call" << std::endl;
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

        /*Making a new struct to keep info about this directory*/
        sync_info_struct* sync_info_struct_ptr = new sync_info_struct;
        sync_info_struct_ptr->source = source;
        sync_info_struct_ptr->destination = destination;
        sync_info_struct_ptr->active = false;
        sync_info_struct_ptr->monitored = false;
        sync_info_struct_ptr->status = "Inactive";
        sync_info_struct_ptr->wd = -1;
            
        /*Put the sruct pointer containing info about the directory in the hash map*/
        sync_info_mem_store[source] = sync_info_struct_ptr;
        job_struct job;
        job.source = source;
        job.dest = destination;
        job.operation = "FULL";
        job.filename = "ALL";
        job.fromconsole = false;
        job.sync = false;
        jobs_queue.push_back(job);
    }

    //create and open  logfile 
    ofstream logfile(manager_logfile);

    //start the inotify thingy 
    int inotify_fd = inotify_init();
    if (inotify_fd==-1) {
        perror("inotify_init problem");
        exit(1);
    }

    //start the initial sync
    int poll_ready, workers_count=0;
    pollfd poll_fds[worker_limit]={0};
    pollfd poll_inotify_fds;
    poll_inotify_fds.fd = inotify_fd;
    poll_inotify_fds.events = POLLIN; 
    char sync_response[MESSAGE_SIZE];

    while (/*shutdown == false || !jobs_queue.empty()*/ true) {
        //cout << "queue size: " << jobs_queue.size() <<endl;
        //cout << workers_count <<  endl;

        if (!jobs_queue.empty() && workers_count < worker_limit) {
            
            job_struct job = jobs_queue.front();
            jobs_queue.pop_front();
            total_jobs++;

            
            if (job.sync == true) {
                //cout << "[" << get_current_time() << "]" << " Syncing directory: " << job.source << " -> " << job.dest << endl;
                //logfile << "[" << get_current_time() << "]" << " Syncing directory: " << job.source << " -> " << job.dest << endl;
                //char message[MESSAGE_SIZE];
                memset(sync_response, 0, MESSAGE_SIZE);
                snprintf(sync_response, MESSAGE_SIZE, "[%s] Syncing directory: %s -> %s\n", get_current_time(), job.source.c_str(), job.dest.c_str());
                if (shutdown == false) {
                    cout << sync_response;
                    logfile << sync_response;
                }
                //cout << sync_response;
                //logfile << sync_response;
                /*if ( (fd_fss_out = open("fss_out", O_WRONLY | O_NONBLOCK) ) < 0) {
                    perror("Failed to open fss_out");
                    exit(1);
                }*/
                //write(fd_fss_out, message, strlen(message));
                //close(fd_fss_out);

                sync_info_mem_store[job.source]->wd = inotify_add_watch(inotify_fd, job.source.c_str(), IN_MODIFY | IN_CREATE | IN_DELETE);
                if ( sync_info_mem_store[job.source]->wd == -1) {
                    perror("inotify_add_wach problem");
                    exit(1);
                }
                sync_info_mem_store[job.source]->monitored = true;
                sync_info_mem_store[job.source]->status = "Active";
            }

            if (sync_info_mem_store[job.source]->monitored == false ) {
                if (shutdown == false) {
                    char message[MESSAGE_SIZE];
                    snprintf(message, MESSAGE_SIZE, "[%s] Added directory: %s -> %s\n[%s] Monitoring started for %s", get_current_time(), job.source.c_str(), job.dest.c_str(), get_current_time(), job.source.c_str());
                    cout << message << endl;
                    logfile << message << endl;
                    if (job.fromconsole == true) {
                        if ( (fd_fss_out = open("fss_out", O_WRONLY | O_NONBLOCK) ) < 0) {
                            perror("Failed to open fss_out");
                            exit(1);
                        }
                        write(fd_fss_out, message, strlen(message));
                        close(fd_fss_out);
                    }
                

                    /*cout << "[" << get_current_time() << "]" << " Added directory: " << job.source << " -> " << job.dest << endl;
                    logfile << "[" << get_current_time() << "]" << " Added directory: " << job.source << " -> " << job.dest << endl;

                    cout << "[" << get_current_time() << "]" << " Monitoring started for " << job.source << endl;
                    logfile << "[" << get_current_time() << "]" << " Monitoring started for " << job.source << endl*/

                    sync_info_mem_store[job.source]->wd = inotify_add_watch(inotify_fd, job.source.c_str(), IN_MODIFY | IN_CREATE | IN_DELETE);
                    if ( sync_info_mem_store[job.source]->wd == -1) {
                        perror("inotify_add_wach problem");
                        exit(1);
                    }
                    sync_info_mem_store[job.source]->monitored = true;
                    sync_info_mem_store[job.source]->status = "Active";
                }
            }

            /*Forking logic*/
            int pipe_fd[2]; //pipe for worker - manager communication
            if (pipe(pipe_fd)==-1){ 
                perror("pipe"); 
                exit(1);
            }
            if (job.sync == true) {
                sync_fd = pipe_fd[READ];
            }
            pid_t workerpid;
            //cout << "fork" << endl;
            workerpid = fork();
            if (workerpid == -1) {
                perror("Failed to fork");
                exit(1);
            }
            if (workerpid == 0) {   //child
                close(pipe_fd[READ]);   //child is only writing
                dup2(pipe_fd[WRITE], 1);    //redirect stdout to point at the pipe. Now everything that the child prints goes to the pipe 
                close(pipe_fd[WRITE]); //aparenetly its not needed because of dup2
                int retval = execl("./worker", "./worker", job.source.c_str(), job.dest.c_str(), job.filename.c_str(), job.operation.c_str(), NULL);
                if(retval == -1) {
                    perror("execl");
                    exit(1);
                }
            } else {    //parent
                close(pipe_fd[WRITE]);  //parent only reads from child
                sync_info_mem_store[job.source]->active = true; //a worker for this dir is avtively running
                int free_pos;
                for (int j=0 ; j<worker_limit ; j++) {  //find the first available slot in the table 
                    if (poll_fds[j].fd == 0) {
                        free_pos = j;
                        break;
                    }
                }
                poll_fds[free_pos].fd = pipe_fd[READ]; //add the pipe fd in poll for monitoring 
                poll_fds[free_pos].events = POLLIN;    //monitor only if a fd is ready for reading 
                workers_count++;
            }   
        }
        //poll logic

        /*Poll for reading exec reports from workers*/
        int poll_ready = poll(poll_fds, worker_limit, 100); //non blocking poll-maybe make the timeout 10 or 100
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
                if (s <0) {
                    perror ("reading exec report error");
                    exit(1);
                }
                exec_report[s]= '\0';
                report_info_struct report_info;
                if (parse_report(exec_report, report_info) < 0) {
                    perror("parse_report");
                    exit(-1);
                }
                //cout << "report parsed" << endl;

                if (poll_fds[i].fd == sync_fd) {    //special case for the sync command - diferent logs 
                    sync_fd = -1;   //reinitialize
                    //cout << "[" << get_current_time() << "] " <<"Sync completed " << report_info.source << " -> " << report_info.dest << " Errors:" << sync_info_mem_store[report_info.source]->error_count << endl;
                    //logfile << "[" << get_current_time() << "] " <<"Sync completed " << report_info.source << " -> " << report_info.dest << " Errors:" << sync_info_mem_store[report_info.source]->error_count << endl;
                    char message[MESSAGE_SIZE];
                    snprintf(message, MESSAGE_SIZE, "[%s] Sync completed: %s -> %s Errors:%d", get_current_time(), report_info.source.c_str(), report_info.dest.c_str(), sync_info_mem_store[report_info.source]->error_count);
                    cout << message << endl;
                    logfile << message << endl;
                    char complete_message[2*MESSAGE_SIZE];
                    snprintf(complete_message, 2*MESSAGE_SIZE, "%s%s", sync_response, message);
                    if ( (fd_fss_out = open("fss_out", O_WRONLY | O_NONBLOCK) ) < 0) {
                        perror("Failed to open fss_out");
                        exit(1);
                    }
                    write(fd_fss_out, complete_message, strlen(complete_message));
                    close(fd_fss_out);
                } 
                //if (shutdown == false)
                    //cout << "[" << report_info.timestamp << "] " << "[" << report_info.source << "] " << "[" << report_info.dest << "] " << "[" << report_info.pid << "] " << "[" << report_info.operation << "] " << "[" << report_info.status << "] " << "[" << report_info.errors << "] " << endl;
                
                logfile << "[" << report_info.timestamp << "] " << "[" << report_info.source << "] " << "[" << report_info.dest << "] " << "[" << report_info.pid << "] " << "[" << report_info.operation << "] " << "[" << report_info.status << "] " << "[" << report_info.errors << "] " << endl;
            
                /*Update sync_info_mem store after a worker ended*/
                strcpy(sync_info_mem_store[report_info.source]->last_sync_time, report_info.timestamp.c_str());
                sync_info_mem_store[report_info.source]->active = false;    //a worker is no longer actively running for this directory
                sync_info_mem_store[report_info.source]->error_count = report_info.num;
                
                close(poll_fds[i].fd);  //not needed any more, initialize again so another descriptor can be placed in its position
                poll_fds[i].fd=0;
                poll_fds[i].events=0;
                poll_fds[i].revents=0;
            }
        }

        /*poll for reading events from inotify*/
        if (shutdown == false) {    //we dont want to do this after a shutdown command
            //cout << "before inotify" << endl;
            int poll_inotify = poll(&poll_inotify_fds, 1, 100);
            if (poll_inotify == -1) {
                if (errno == EINTR) {   //this is for the error "poll interupted by signal SIGCHLD "
                    continue;
                } else {
                    perror("poll");
                    exit(1);
                }
            }
            char inotify_buffer[INOTIFY_SIZE];
            if (poll_inotify_fds.revents & POLLIN) {    //there is data to read from inotify
                const struct inotify_event *event;
                int length = read(inotify_fd, inotify_buffer, INOTIFY_SIZE);
                if (length < 0) {
                    perror("read from inotify");
                }
                //cout << "reaaad" << endl;
                int i=0;
                while (i < length) {
                    struct inotify_event* event = (struct inotify_event*)&inotify_buffer[i];
                    job_struct new_job; //create new job to push to the queue
                    for (auto it=sync_info_mem_store.begin() ; it != sync_info_mem_store.end() ; ++it) {    //search sync_info_mem_store to find the wd
                        if (it->second->wd == event->wd) {
                            new_job.source = it->first;
                            new_job.dest = it->second->destination;
                            new_job.filename = event->name;
                        }
                    }
                    if (event->mask & IN_CREATE) {
                        new_job.operation = "ADDED";
                    } else if (event->mask & IN_MODIFY) {
                        new_job.operation = "MODIFIED";
                    } else if (event->mask & IN_DELETE) {
                        new_job.operation = "DELETED";
                    } else {
                        i += sizeof(struct inotify_event) + event->len; //SKIP
                        continue;
                    }
                    new_job.fromconsole = false;
                    new_job.sync = false;
                    jobs_queue.push_front(new_job);        //PUSH TO THE FRONT OF DEQUEUE  
                    //cout << "pushed" << endl;  
                    i += sizeof(struct inotify_event) + event->len;
                }
            }
        }  

        /*check for command from console*/
        char console_buffer[MESSAGE_SIZE] = {0};
        ssize_t s = read(fd_fss_in, console_buffer, MESSAGE_SIZE);      //no need for poll because read is non blocking due to fss in being non blocking
        if (s > 0) {
         /* Break down the command in parts*/
            console_buffer[s] = '\0';   //null terminated because i had errors with garbage values
            vector<string> command_parts;
            string command(console_buffer);     //make char[] a string
            string token;
            istringstream iss(command);
            while (iss >> token) {
                command_parts.push_back(token);
            }

            /*handle which command*/
            if (command_parts[0] == "add") {

                //create new job to push
                job_struct new_job;
                new_job.source = command_parts[1];
                new_job.dest = command_parts[2];
                new_job.filename = "ALL";
                new_job.operation = "FULL";
                new_job.fromconsole = true;
                new_job.sync = false;


                auto it = sync_info_mem_store.find(new_job.source);
                if (it != sync_info_mem_store.end() && it->second->monitored == true) { //if found and currently monitored skip
                    char message[MESSAGE_SIZE];
                    snprintf(message, MESSAGE_SIZE, "[%s] Already in queue: %s", get_current_time(), new_job.source.c_str());
                    cout << message << endl;
                    if ( (fd_fss_out = open("fss_out", O_WRONLY | O_NONBLOCK) ) < 0) {
                        perror("Failed to open fss_in");
                        exit(1);
                    }
                   ssize_t n = write(fd_fss_out, message, strlen(message));
                   close(fd_fss_out);
                   if (n<=0){
                    perror("error writing to fss_out");
                   }
                    continue;
                }

                if (it == sync_info_mem_store.end()) {  //if not found means its a new directory  
                    /*Making a new struct to keep info about this directory*/
                    sync_info_struct* sync_info_struct_ptr = new sync_info_struct;
                    sync_info_struct_ptr->source = new_job.source;
                    sync_info_struct_ptr->destination = new_job.dest;
                    sync_info_struct_ptr->active = false;
                    sync_info_struct_ptr->monitored = false;
                    sync_info_struct_ptr->status = "Active";
                    sync_info_struct_ptr->wd = -1;
                    sync_info_mem_store[new_job.source] = sync_info_struct_ptr;     //add it to the sync info mem store 
                }// else { //found but monitored == false 
                 //   sync_info_mem_store[new_job.source]->status = "Active";
                   // sync_info_mem_store[new_job.source]->monitored = true;
                //}
                //restart the monitoring 
                //sync_info_mem_store[new_job.source]->wd = inotify_add_watch(inotify_fd, sync_info_mem_store[new_job.source]->source.c_str(), IN_MODIFY | IN_CREATE | IN_DELETE);
                jobs_queue.push_front(new_job);//AT THE FRONT 

            } else if (command_parts[0] == "sync") {

                auto it = sync_info_mem_store.find(command_parts[1]);
                if (it == sync_info_mem_store.end()) {  //not found
                    char message[MESSAGE_SIZE];
                    snprintf(message, MESSAGE_SIZE, "Directory %s has not been added before", command_parts[1].c_str());
                    //cout << message << endl;
                    if ( (fd_fss_out = open("fss_out", O_WRONLY | O_NONBLOCK) ) < 0) {
                        perror("Failed to open fss_in");
                        exit(1);
                    }
                   ssize_t n = write(fd_fss_out, message, strlen(message));
                   close(fd_fss_out);
                   if (n<=0){
                    perror("error writing to fss_out");
                   }
                   continue;
                }
                //if found 
                if (sync_info_mem_store[command_parts[1]]->active == true) {   //currently running a worker for this directory -> skip
                    char message[MESSAGE_SIZE];
                    snprintf(message, MESSAGE_SIZE, "[%s] Sync already in progress: %s", get_current_time(), command_parts[1].c_str());
                    cout << message << endl;
                    if ( (fd_fss_out = open("fss_out", O_WRONLY | O_NONBLOCK) ) < 0) {
                        perror("Failed to open fss_in");
                        exit(1);
                    }
                   ssize_t n = write(fd_fss_out, message, strlen(message));
                   close(fd_fss_out);
                   if (n<=0){
                    perror("error writing to fss_out");
                   }
                   continue;
                }   
                //if there is no active job with this source dir but the directory exists -> make new job 
                job_struct new_job;
                new_job.source = command_parts[1];
                new_job.dest = sync_info_mem_store[new_job.source]->destination;
                new_job.filename = "ALL";
                new_job.operation = "FULL";
                new_job.fromconsole = true;
                new_job.sync = true;
                jobs_queue.push_front(new_job); //TO TEH FRONT

            } else if (command_parts[0] == "status") {

                auto it = sync_info_mem_store.find(command_parts[1]);
                if (it == sync_info_mem_store.end()) {  //wasnt found -> SKIP
                    char message[MESSAGE_SIZE];
                    snprintf(message, MESSAGE_SIZE, "[%s] Directory not monitored: %s", get_current_time(),  command_parts[1].c_str());
                    cout << message << endl;
                    if ( (fd_fss_out = open("fss_out", O_WRONLY | O_NONBLOCK) ) < 0) {
                        perror("Failed to open fss_in");
                        exit(1);
                    }
                    ssize_t n = write(fd_fss_out, message, strlen(message));
                    close(fd_fss_out);
                    if (n<=0){
                        perror("error writing to fss_out");
                    }
                    continue;
                } 
                //was found
                char message[MESSAGE_SIZE];
                snprintf(message, MESSAGE_SIZE, "[%s] Status requested for %s\nDirectory: %s\nTarget: %s\nLast Sync: %s\nErrors: %d\nStatus: %s", get_current_time(), command_parts[1].c_str(), command_parts[1].c_str(), sync_info_mem_store[command_parts[1]]->destination.c_str(), sync_info_mem_store[command_parts[1]]->last_sync_time, sync_info_mem_store[command_parts[1]]->error_count, sync_info_mem_store[command_parts[1]]->status.c_str());
                cout << message << endl;
                if ( (fd_fss_out = open("fss_out", O_WRONLY | O_NONBLOCK) ) < 0) {
                    perror("Failed to open fss_in");
                    exit(1);
                }
                ssize_t n = write(fd_fss_out, message, strlen(message));
                close(fd_fss_out);
                if (n<=0){
                    perror("error writing to fss_out");
                }

            }else if (command_parts[0] == "cancel") {

                auto it = sync_info_mem_store.find(command_parts[1]);
                if (it == sync_info_mem_store.end() || it->second->monitored == false) {    //if it doesnt exist or is not monitored -> skip
                    char message[MESSAGE_SIZE];
                    snprintf(message, MESSAGE_SIZE, "[%s] Directory not monitored: %s", get_current_time(), command_parts[1].c_str());
                    cout << message << endl;
                    if ( (fd_fss_out = open("fss_out", O_WRONLY | O_NONBLOCK) ) < 0) {
                        perror("Failed to open fss_in");
                        exit(1);
                    }
                    ssize_t n = write(fd_fss_out, message, strlen(message));
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
                char message[MESSAGE_SIZE];
                snprintf(message, MESSAGE_SIZE, "[%s] Monitoring stopped for %s", get_current_time(), command_parts[1].c_str());
                cout << message << endl;
                if ( (fd_fss_out = open("fss_out", O_WRONLY | O_NONBLOCK) ) < 0) {
                    perror("Failed to open fss_in");
                    exit(1);
                }
                ssize_t n = write(fd_fss_out, message, strlen(message));
                close(fd_fss_out);
                if (n<=0){
                    perror("error writing to fss_out");
                }

            } else if (command_parts[0] == "shutdown") {

                shutdown = true;
                char message[MESSAGE_SIZE];
                snprintf(message, MESSAGE_SIZE, "[%s] Shutting down manager...\n[%s] Waiting for all active workers to finish.\n[%s] Processing remaining queued tasks.", get_current_time(), get_current_time(), get_current_time());
                cout << message << endl;
                if ( (fd_fss_out = open("fss_out", O_WRONLY | O_NONBLOCK) ) < 0) {
                    perror("Failed to open fss_in");
                    exit(1);
                }
                ssize_t n = write(fd_fss_out, message, strlen(message));
                //close(fd_fss_out);   
                if (n<=0){
                    perror("error writing to fss_out");
                }

                //stop inotify
                if (close(inotify_fd) == -1) {  //removes all descriptors 
                    perror("close inotify");
                }

            } else {
                perror(" the command i just read is wrong\n");
            }


        }

        /*Reniew how many workers we have active at the moment*/
        if (terminated_workers > 0) {   //from the signal handler
            workers_count -= terminated_workers;
            terminated_workers = 0;     //reset the flag
        }

        if (jobs_queue.empty() && workers_count == 0 && shutdown == true) {
            break;
        }
    }

    for(auto it = sync_info_mem_store.begin() ; it != sync_info_mem_store.end() ; ++it) {
        delete it->second;
    }



    char message[MESSAGE_SIZE];
    snprintf(message, MESSAGE_SIZE, "[%s] Manager shutdown complete", get_current_time());
    cout << message << endl;
    ssize_t n = write(fd_fss_out, message, strlen(message));
    //close(fd_fss_out);   
    if (n<=0){
        perror("error writing to fss_out");
    }

    logfile.close();
    close(fd_fss_out);
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