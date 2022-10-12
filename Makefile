SHELL		= /bin/bash
CC			= gcc
INCLUDES	= -I ./headers
FLAGS		= -g -Wall -pedantic 

master_worker_dep	= ./src/aux_function.c ./src/list.c ./src/conc_queue.c ./src/pool_worker.c ./src/master_thread.c 

collector_dep =  ./src/aux_function.c

.PHONY		: clean

farm		:
		make clean
		make master_worker
		make collector

master_worker		: ./src/main.c
		$(CC) $(FLAGS) $(master_worker_dep) $< $(INCLUDES) -o $@
collector	: ./src/collector.c
		$(CC) $(FLAGS) $(collector_dep) $< $(INCLUDES) -o $@
clean		: 
		-rm farm.sck
		-rm master_worker
		-rm collector