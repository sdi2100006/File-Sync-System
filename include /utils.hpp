#pragma once

typedef struct sync_info_mem_store {
    char* source;
    char* destination;
    int status ;
    int lasy_sync_time;
    bool active;
    int error_count;
} sync_info_mem_store;