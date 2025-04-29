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
using namespace std;

int main (int argc, char* argv[]) {
    //parse arguments
    int opt;
    string console_logfile;
    while ((opt = getopt(argc, argv, "l:")) != -1) {
        switch (opt) {
            case 'l':
                console_logfile = optarg;
                break;
            default:
                cout << "wrong call" << endl;
                exit(1);
        }
    }

    ofstream logfile(console_logfile);

    while (true) {
        cout << "> ";
        string user_input;
        getline(cin, user_input);

        istringstream iss(user_input);
        vector<string> command;
        string command_part;
        while (iss >> command_part) {
            command.push_back(command_part);
        }

        bool command_error = false;

        if (command[0] == "add") {
            if (command.size() != 3) {
                command_error = true;
            } else {
                logfile << "[" << get_current_time() << "]" << " Command " << command[0] << " " << command[1] << " -> " << command[2] << endl; 
            }
        } else if (command[0] == "status") {
            if (command.size() != 2) {
                command_error = true;
            } else {
                logfile << "[" << get_current_time() << "]" << " Command " << command[0] << " " << command[1] << endl; 
            }
        } else if (command[0] == "cancel") {
            if (command.size() != 2) {
                command_error = true;
            } else {
                logfile << "[" << get_current_time() << "]" << " Command " << command[0] << " " << command[1] << endl; 
            }
        } else if (command[0] == "sync") {
            if (command.size() != 2) {
                command_error = true;
            } else {
                logfile << "[" << get_current_time() << "]" << " Command " << command[0] << " " << command[1] << endl; 
            }
        } else if (command[0] == "shutdown") {
            if (command.size() != 1) {
                command_error = true;
            } else {
                logfile << "[" << get_current_time() << "]" << " Command " << command[0] << endl;
            }
        } else {
            command_error = true;
        }
        
        if (command_error == true) {
            cout << "command error try again" << endl;
            continue;
        }

        //commands are correct

        int fd_fss_in = open("fss_in", O_WRONLY  /*|O_NONBLOCK*/);
        if ( fd_fss_in == -1 ){
            perror("failed to open fss_in");
            exit(1);
        }
        
        if (write(fd_fss_in, user_input.c_str(), user_input.length()) == -1) {  //we sent the command to fss_manager and we have to wait for a response
            perror("error writing to fss_in from console");
            close(fd_fss_in);
            exit(-1);
        }  
        //logfile << user_input << endl;
        close(fd_fss_in);


        int fd_fss_out = open("fss_out", O_RDONLY );
        if ( fd_fss_out == -1){
            perror("failed to open fss_out");
            exit(1);
        }
            
        char buffer[256];
        ssize_t s = read(fd_fss_out, buffer, sizeof(buffer));
        //close(fd_fss_out);
        if (s > 0) {
            buffer[s] = '\0'; // Null-terminate because i had garbage values
            cout << buffer << endl;
            logfile << buffer << endl;
        } else {
            cout << "No response or error reading from fss_out" << endl;
        }


        if (command[0] == "shutdown") {
            //int fd_fss_out = open("fss_out", O_RDONLY );
           // if ( fd_fss_out < 0 ){
              //  perror("failed to open fss_out");
               // exit(1);
            //}
                
            char buffer[256];
            ssize_t s = read(fd_fss_out, buffer, sizeof(buffer));
            close(fd_fss_out);
            if (s > 0) {
                buffer[s] = '\0'; // Null-terminate
                cout << buffer << endl;
               logfile << buffer << endl;
            } else {
                cout << "No response or error reading from fss_out shutdown" << endl;
            }
        }
        close(fd_fss_out);
        if (command[0] == "shutdown") {
            break;
        }
        cout << endl;
    }
}
