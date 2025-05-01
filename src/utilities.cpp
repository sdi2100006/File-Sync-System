#include "../include/utilities.hpp"
#include <ctime>
#include <cstdlib>

char* get_current_time () {
    time_t raw_time;
    struct tm *time_info;
    static char formatted_time[20];

    time(&raw_time);
    time_info = localtime(&raw_time);
    strftime(formatted_time, sizeof(formatted_time), "%F %T", time_info);
    //cout << "DEBUG: " << formatted_time << endl;
    return formatted_time;
}

int get_random_index(int worker_limit) {
    return rand() % worker_limit;
}