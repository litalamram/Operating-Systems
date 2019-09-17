/*
 * sym_mng.c
 *
 *  Created on: 23 Mar 2018
 *      Author: lital
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>

void cleanup(int *counters, int *pids, int pids_len){
	//kill all processes
	int i;
	for (i=0;i<pids_len;i++){
		kill(SIGKILL, pids[i]);
	}
	int status;
	//wait for all child processes to end
	while (-1 != wait(&status));
	free(pids); //free pids array
	free(counters); //free counters array
}

int main(int argc, char** argv){

	if (argc < 4){
		printf("Error: not enough arguments");
		return EXIT_FAILURE;
	}

	char *fName = argv[1];		//path to data file
	char *pattern = argv[2];	//pattern to find
	int bnd = atoi(argv[3]);	//termination bound
	int pattern_len = strlen(pattern);
	int *pids = NULL, *counters = NULL;
	int pids_len = 0;

	pids = (int*)malloc(sizeof(int)*pattern_len);
	if (pids == NULL){
		printf("ERROR: malloc has failed\n");
		cleanup(counters, pids, pids_len);
		return EXIT_FAILURE;
	}

	counters = (int*)calloc(pattern_len, sizeof(int));
	if (counters == NULL){
		printf("ERROR: malloc has failed\n");
		cleanup(counters, pids, pids_len);
		return EXIT_FAILURE;
	}

	int i;	
	int curr_child = -1;
	//create child processes for each symbol in the pattern
	for (i=0;i<pattern_len;i++){
		curr_child = fork();
		if (curr_child == -1) { //process creation failed
			printf("ERROR: process creation failed %s\n", strerror(errno));
			cleanup(counters, pids, pids_len);
			return EXIT_FAILURE;
		}
		if(curr_child  == 0) { //child process
			char *child_args[] = {"./sym_count", fName, pattern+i, NULL};
			execvp(child_args[0], child_args);
			//execvp failed
			printf("ERROR: execvp failed for process %d, %s\n",getpid(), strerror(errno));
			//exit the child process
			exit(errno);
		}
		else { //parent process
			pids[i] = curr_child;
			pids_len++;
		}
	}

	int status = -1;
	//iterate over the launched processes
	while (pids_len > 0){
		sleep(1);
		for (i=0;i<pids_len;i++){
			curr_child = waitpid(pids[i], &status, WUNTRACED | WNOHANG);
			if (WIFSTOPPED(status) && WSTOPSIG(status) == SIGSTOP){ //process is stopped
				counters[i]++;
				if (counters[i] == bnd) { //the stop counter is equal to the termination bound
					//terminate the process
					if (kill(pids[i], SIGTERM) < 0){
						//failed sending the signal
						printf("ERROR: failed sending signal %s\n", strerror(errno));
						cleanup(counters, pids, pids_len);
						return errno;
					}
					//continue the process so it will get the SIGTERM signal
					if (kill(pids[i], SIGCONT) < 0){
						//failed sending the signal
						printf("ERROR: failed sending signal %s\n", strerror(errno));
						cleanup(counters, pids, pids_len);
						return errno;
					}
				}
				else {
					//continue the process
					if (kill(pids[i], SIGCONT) < 0){
						//failed sending the signal
						printf("ERROR: failed sending signal %s\n", strerror(errno));
						cleanup(counters, pids, pids_len);
						return errno;
					}
				}
			}
			else { //process is finished
				//exclude the process
				pids[i] = pids[pids_len-1];
				pids_len--;
			}
		}
	}
	cleanup(counters, pids, pids_len);
	return EXIT_SUCCESS;
}
