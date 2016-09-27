#############################################
# Simple makefile for execution of project1
# set path = ($path /cygdrive/c/cygwin64/bin)
#############################################
# all    - compile the project
# test1  - clean, compile and run in simple test config
# run_it - generic target to compile AND run

all: raycast.c
	gcc raycast.c -o raycast

# assuming VERBOSE is set to 1 in raycast.c
verbose: raycast.c
	gcc raycast.c -o raycast_v

clean:
	rm raycast.exe

#############################################
# targets for testing each of the simple test configs
#############################################
test1:
	gcc raycast.c -o raycast
	./raycast 400 500 test_good_01.json test.ppm

test2:
	gcc raycast.c -o raycast
	./raycast 1000 800 test_good_02.json test.ppm

test3:
	gcc raycast.c -o raycast
	./raycast 60 30 test_good_03.json test.ppm

test4:
	gcc raycast.c -o raycast
	./raycast 20 20 test_good_04.json test.ppm
