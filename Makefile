# Makefile for fss_manager and worker

OBJS = fss_manager.o worker.o utilities.o fss_console.o
SRCS = src/fss_manager.cpp src/worker.cpp src/utilities.cpp src/fss_console.cpp
CC = g++
FLAGS = -g -c 
OUT_MANAGER = fss_manager
OUT_WORKER = worker
OUT_CONSOLE = fss_console

# Build both executables
all: $(OUT_MANAGER) $(OUT_WORKER) $(OUT_CONSOLE)

# Link fss_manager executable
$(OUT_MANAGER): fss_manager.o utilities.o
	$(CC)  -g fss_manager.o utilities.o -o $(OUT_MANAGER)

# Link worker executable
$(OUT_WORKER): worker.o utilities.o
	$(CC) -g worker.o utilities.o -o $(OUT_WORKER)

$(OUT_CONSOLE): fss_console.o
	$(CC) -g fss_console.o -o $(OUT_CONSOLE)

# Compile individual object files
fss_manager.o: src/fss_manager.cpp
	$(CC) $(FLAGS) src/fss_manager.cpp

worker.o: src/worker.cpp
	$(CC) $(FLAGS) src/worker.cpp

utilities.o: src/utilities.cpp
	$(CC) $(FLAGS) src/utilities.cpp

fss_console.o: src/fss_console.cpp
	$(CC) $(FLAGS) src/fss_console.cpp



# Clean up
clean:
	rm -f *.o $(OUT_MANAGER) $(OUT_WORKER) $(OUT_CONSOLE)
	rm -f fss_in fss_out managerlogfile.txt
	clear

