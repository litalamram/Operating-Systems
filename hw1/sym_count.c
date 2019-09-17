/*
 * sym_count.c
 *
 *  Created on: 22 Mar 2018
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


#define BUFSIZE 1024
int cnt = 0, file = -1;
char symbol;

void sigcont_handler(int signum, siginfo_t *info, void *ptr){
	printf("Process %d continues\n", getpid());
}

void sigterm_handler(int signum, siginfo_t *info, void *ptr){
	printf("Process %d finishes. Symbol %c. Instances %d.\n", getpid(), symbol, cnt);
	close(file);
	exit(EXIT_SUCCESS);
}


int main(int argc, char** argv)
{
	if (argc < 3){
		printf("Error: not enough arguments");
		return EXIT_FAILURE;
	}

	// Structure to pass to the registration syscall
	struct sigaction sigcont_action;
	memset(&sigcont_action, 0, sizeof(sigcont_action));
	// Assign pointer to the handler function
	sigcont_action.sa_sigaction = sigcont_handler;
	// Setup the flags
	sigcont_action.sa_flags = SA_SIGINFO;
	// Register the handler to SIGCONT
	if( 0 != sigaction(SIGCONT, &sigcont_action, NULL) )
	{
		printf("Signal handle registration failed. %s\n", strerror(errno));
		return errno;
	}
	struct sigaction sigterm_action;
	memset(&sigterm_action, 0, sizeof(sigterm_action));
	// Assign pointer to the handler function
	sigterm_action.sa_sigaction = sigterm_handler;
	// Setup the flags
	sigterm_action.sa_flags = SA_SIGINFO;
	sigfillset(&sigterm_action.sa_mask);
	// Register the handler to SIGCONT
	if( 0 != sigaction(SIGTERM, &sigterm_action, NULL) )
	{
		printf("Signal handle registration failed. %s\n", strerror(errno));
		return errno;
	}

	//open the file for reading
	file = open(argv[1], O_RDWR);
	//check the file was opened successfully
	if( file < 0 )
	{
		printf("Error opening file: %s\n", strerror( errno ) );
		return errno;
	}

	char buffer[BUFSIZE]; //buffer for reading from the file
	int len;
	symbol = argv[2][0];	//the symbol to find
	int i;

	while ((len = read(file, buffer, BUFSIZE)) > 0){
		for (i=0;i<len;i++){
			if (buffer[i] == symbol){
				cnt++;
				printf("Process %d, symbol %c, going to sleep\n", getpid(), symbol);
				raise(SIGSTOP);
			}
		}
	}

	if(len < 0) //error reading the file
	{
		close(file); // close file
		printf("Error reading from file: %s\n", strerror(errno));
		return errno;
	}

	//EOF
	raise(SIGTERM);

	return EXIT_SUCCESS;

}

