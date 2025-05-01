#pragma once
#include <string>
#include <unordered_map>
#include <deque>
#include <vector>
#include "../include/types.hpp"

using namespace std;

#define READ 0
#define WRITE 1
#define REPORT_SIZE 512
#define MESSAGE_SIZE 1024
#define INOTIFY_SIZE 4096

int parse_report(string, report_info_struct&);

void parse_commandline(int argc, char* argv[], string& manager_logfile, string& config_file, int& worker_limit);

void cleanup(string manager_logfile);

void create_named_pipes();

void read_config_and_init_struct(string config_file, unordered_map<string, sync_info_struct*>& sync_info_mem_store, deque <job_struct>& jobs_queue);

void spawn_workers(struct job& job, int& sync_fd, unordered_map<string, sync_info_struct*>& sync_info_mem_store, int worker_limit,struct pollfd* poll_fds, int& workers_count);

int handle_exec_report_events(struct pollfd* poll_fds, int worker_limit, int& sync_fd, char* sync_response, ofstream& logfile, int fd_fss_out, unordered_map<string, sync_info_struct*>& sync_info_mem_store);

void handle_and_log_job(struct job& job, char* sync_response, bool shutdown, ofstream& logfile, unordered_map<string, sync_info_struct*>& sync_info_mem_store, int fd_fss_out, int inotify_fd);

int handle_inotify_events(bool shutdown, pollfd poll_inotify_fds, int inotify_fd, unordered_map<string, sync_info_struct*>& sync_info_mem_store, deque <job_struct>& jobs_queue);

int handle_add(vector<string>& command_parts, unordered_map<string, sync_info_struct*>& sync_info_mem_store, int fd_fss_out, deque<job_struct>& jobs_queue);

int handle_sync(vector<string>& command_parts, unordered_map<string, sync_info_struct*>& sync_info_mem_store, int fd_fss_out, int inotify_fd, deque<job_struct>& jobs_queue);

int handle_status(vector<string>& command_parts, unordered_map<string, sync_info_struct*>& sync_info_mem_store, int fd_fss_out);

int handle_cancel(vector<string>& command_parts, unordered_map<string, sync_info_struct*>& sync_info_mem_store, int fd_fss_out, int inotify_fd);

void handle_shutdown(bool& shutdown, int fd_fss_out, int inotify_fd);

int handle_command(int fd_fss_in, int fd_fss_out, int inotify_fd ,unordered_map<string, sync_info_struct*>& sync_info_mem_store, deque <job_struct>& jobs_queue, bool& shutdown);
