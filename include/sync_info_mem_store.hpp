#pragma once
#include <bits/stdc++.h>
#include <unordered_map>
#include <string.h>

using namespace std;

typedef struct sync_info {
    string source;
    string destination;
    string status ;
    char last_sync_time[20];
    bool active;
    bool monitored;
    int error_count;
    int wd;
} sync_info_struct;




