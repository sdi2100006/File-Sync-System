#pragma once
#include <bits/stdc++.h>
#include <unordered_map>
#include <string.h>

using namespace std;

typedef struct sync_info {
    string source;
    string destination;
    string status ;
    int last_sync_time;
    bool active;
    int error_count;
    int wd;
} sync_info_struct;




