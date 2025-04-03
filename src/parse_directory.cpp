#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <dirent.h>
#include <iostream>
#include <string.h>
#include <vector>

using namespace std;

vector<char*> parse_directory(const char dirname[]) {
    DIR *dir_ptr;
    struct dirent *direntp;
    vector<char*> temp;

    if( (dir_ptr = opendir(dirname) ) == NULL ) {
        perror("opendir");
    } else {
        while ( (direntp=readdir(dir_ptr)) != NULL ) {

            if ( strcmp(direntp->d_name, ".") == 0 || strcmp(direntp->d_name, "..") == 0 )
                continue;

            //cout << "inode: " << direntp->d_ino << " name: " << direntp->d_name << endl; 
            temp.push_back(direntp->d_name);
        }
        closedir(dir_ptr);
    }
    return temp;
}

int main () {
    vector<char*> result;
    result = parse_directory("/home/arkostid/hw1-sdi2100006/src");

    for (int i=0 ; i<=result.size() ; i++) {
        cout << result[i] << endl;;
    }
    cout << endl;
}   