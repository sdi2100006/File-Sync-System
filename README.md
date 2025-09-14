# File-Sync-System
Name: Konstantinos Arastsiaris   


## 1. General
File Sync System Implementation using server-client format

## 2. Files
- **fss_manager.cpp**: Main Server logic - Workflow of the program 
- **manager_helpers.cpp**: Functions used by the server
- **worker.cpp**: Main Worker(client) logic - Handles the syncing operations in a low lever
- **worker_helpers**: Functions used by the worker.cpp
- **utilities.cpp**: General helper functions used by all files 

## 3. Program Workflow
1. Parses args, cleanup and making of named pipes
2. Parses config files and saves all directries in the sync_info_mem_store structure. Also, creates a job for each directory read from the config file and puts it at the back of a deque.
3. Enters main loop, removes a job from the queue and logs messages
4. Forks a worker to complete the job, creates a pipe between the parent and the child
5. Does a non blocking poll to read events from the available children file descriptors and logs messages
6. Does a non blocking poll to read available events from the inotify file descriptor and creatrs a new job to push at the FRONT of the deque. Also updates the sync_info_mem_Store str
7. Does a non blocking read to read possible incoming commands from the fss_console, handles each command appropriately by creating new jobs and pushing them at the FRONT, updating the sync_info_mem_store struct and logging messages 
8. Decrements workers_count

## 2. Design Decisions
- **STL Unordered Map for sync_info_mem_store**: By using the directory path as a key and a struct conataining all the information as value i ensure fast access and simpliciy. *Drawback*: User can either use absolute or relative paths in order to not mess up the key.
- **Sync_info_mem_store and queue populated at the time of parsing the config file**: When reading the config file i enter all the directories in the struct and i also create jobs and push them in the queue. This way all i have to do is pop the jobs from the queue and assign them to a worker. *Drawback*: had to use a deque in order for high priority jobs to be completed first - more complex.
- **Use of deque instead of queue**: All the config file jobs are placed at the deque so when a command or inotify job happens it is placaed at the front and is completed first without waiting for all the other config jobs to finish first.
- **Use of Poll in reading inotify events**: Because inotifys read is blocking by default i used poll to make it non blocking. Not sure how  else i could have done it. Not sure it is the best idea.
- **3 Seperate code blocks for reading from file descriptors**: I think it more clear and easy to have the reading exec report, reading inotify events and reading commands logic seperate instead of having a poll that monitors and reads events from all the above file descriptors
- **Worker Terminaion Logic**: In the signal handler i dont decriment the workers_count variable directly because i had problems. I use a counter that count how many zombies where killed and then in the main loop of the manager program i decrement workers count accordingly and reset the other counter.
- **Shutdown Logic**: When shutdown command is recieved i close the inotify file descriptor which automatically closes all the watch descriptors automatically so no more inotify events can be read. Then the program continues performing all the queued jobs but doesnt log Added direcory and monioring started because its not true until the queue is empty and the al the workers have ended.
- **Use of C++**: I decited to use C++ for the STL data structures and ease of use of strings BUT i ended up using C+ because i used a c with strings + STL :) :). Also had quite bit of problems mixing char* char[] and string together but i found .c_str()

## 3. Assumptions
- **The users enters correct existing directories**: There is some error handling for wrong directories and commands given by the user but i dont check all cases.
- **Either use absolute or relative paths for the entirety of the program**: Because the directory path is used as the key to the unordered map. I used relative paths for testing so i hope that by making the buffer sizes bigger i accommodated for absolute paths.
- **Sync command SKIP message**: [2025-03-30 10:24:00] Sync already in progress /home/user/docs is gonna be printed if there is another worker for the same directory actively running at the exact same time the command was given. 
- **Before the program is terminated with shutdown all the congif file jobs will have finished**

## 4. How to run 
- Compile using **"Make"**
- Run like this **"./fss_manager -l <manager_logfile> -c <config_file> -n <worker_limit>"** and **"./fss_console -l <console-logfile>"**
