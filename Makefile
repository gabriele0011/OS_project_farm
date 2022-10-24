SHELL				= /bin/bash
CC					= gcc
INCLUDES			= -I ./headers
FLAGS				= -g -Wall -pedantic 

farm_dep			= ./src/aux_function.c ./src/list.c ./src/conc_queue.c ./src/pool_worker.c ./src/collector.c ./src/master_thread.c 

generafile_dep		= ./src/generafile.c

.PHONY				: all clean test

all					:
					make clean
					make generafile
					make farm

generafile			: ./src/generafile.c
						$(CC) $(FLAGS) $(generafile_dep) -o $@					

farm				: ./src/main.c
						$(CC) $(FLAGS) $(farm_dep) $< $(INCLUDES) -o $@

test				:
					make generafile
					make farm
					chmod +x script/test.sh
					script/test.sh


clean				:
					-rm farm
					-rm generafile
					-rm file1.dat
					-rm file2.dat
					-rm file3.dat
					-rm file4.dat
					-rm file5.dat
					-rm file10.dat
					-rm file12.dat
					-rm file13.dat
					-rm file14.dat
					-rm file15.dat
					-rm file16.dat
					-rm file17.dat
					-rm file18.dat
					-rm file20.dat
					-rm file100.dat
					-rm file116.dat
					-rm file117.dat
					-rm -r testdir
					
