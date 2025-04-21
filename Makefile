# Makefile for fss_manager and worker

OBJS = fss_manager.o worker.o utilities.o
SRCS = src/fss_manager.cpp src/worker.cpp src/utilities.cpp
CC = g++
FLAGS = -c
OUT_MANAGER = fss_manager
OUT_WORKER = worker

# Build both executables
all: $(OUT_MANAGER) $(OUT_WORKER)

# Link fss_manager executable
$(OUT_MANAGER): fss_manager.o utilities.o
	$(CC) fss_manager.o utilities.o -o $(OUT_MANAGER)

# Link worker executable
$(OUT_WORKER): worker.o utilities.o
	$(CC) worker.o utilities.o -o $(OUT_WORKER)

# Compile individual object files
fss_manager.o: src/fss_manager.cpp
	$(CC) $(FLAGS) src/fss_manager.cpp

worker.o: src/worker.cpp
	$(CC) $(FLAGS) src/worker.cpp

utilities.o: src/utilities.cpp
	$(CC) $(FLAGS) src/utilities.cpp

# Clean up
clean:
	rm -f *.o $(OUT_MANAGER) $(OUT_WORKER)
	rm -f fss_in fss_out managerlogfile.txt
	clear

