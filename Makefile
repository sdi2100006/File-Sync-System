OBJS = fss_manager.o worker.o utilities.o fss_console.o manager_helpers.o worker_helpers.o
SRCS = src/fss_manager.cpp src/worker.cpp src/utilities.cpp src/fss_console.cpp src/manager_helpers.cpp src/worker_helpers.cpp
CC = g++
FLAGS = -g -c 
OUT_MANAGER = fss_manager
OUT_WORKER = worker
OUT_CONSOLE = fss_console


all: $(OUT_MANAGER) $(OUT_WORKER) $(OUT_CONSOLE)

$(OUT_MANAGER): fss_manager.o utilities.o manager_helpers.o
	$(CC)  -g fss_manager.o utilities.o manager_helpers.o -o $(OUT_MANAGER)

$(OUT_WORKER): worker.o utilities.o worker_helpers.o
	$(CC) -g worker.o utilities.o worker_helpers.o -o $(OUT_WORKER)

$(OUT_CONSOLE): fss_console.o
	$(CC) -g fss_console.o utilities.o -o $(OUT_CONSOLE)

fss_manager.o: src/fss_manager.cpp
	$(CC) $(FLAGS) src/fss_manager.cpp

worker.o: src/worker.cpp
	$(CC) $(FLAGS) src/worker.cpp

utilities.o: src/utilities.cpp
	$(CC) $(FLAGS) src/utilities.cpp

fss_console.o: src/fss_console.cpp
	$(CC) $(FLAGS) src/fss_console.cpp

manager_helpers.o: src/manager_helpers.cpp
	$(CC) $(FLAGS) src/manager_helpers.cpp

worker_helpers.o: src/worker_helpers.cpp
	$(CC) $(FLAGS) src/worker_helpers.cpp

# Clean up
clean:
	rm -f *.o $(OUT_MANAGER) $(OUT_WORKER) $(OUT_CONSOLE)
	clear

