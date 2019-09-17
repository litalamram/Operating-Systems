#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>

//Definitions
#define SHARED_FILE "/tmp/osfifo"
#define BUFFER_SIZE 512
#define MAX_NAME_LEN 100

//Type used to store relevant data about a child process
typedef struct child_info_t {
	int pid; //child pid
	int fd; //fifo file
	char pipeName[MAX_NAME_LEN]; //name of the fifo file
} ChildInfo;

//globals
ChildInfo *children = NULL;
int numChildren = 0;

/** Kills all the child processes,
 * and frees all the memory associated with the array specified
 *
 * @param children - the children array
 * @param numChildren - length of the array
 * */
void freeResources(ChildInfo *children, int numChildren){

	for (int i=0;i<numChildren;i++){
		//kill process
		kill(SIGTERM, children[i].pid);
		//close and unlink fifo file
		if (children[i].fd != -1) {
			close(children[i].fd);
			unlink(children[i].pipeName);
		}
	}

	free(children); //free children array

	//wait for all children processes to end
	int status;
	while (-1 != wait(&status));
}

/**Prints the error massage specified,
 * frees the memory associated with the given array,
 * and exits
 *
 * @param msg - the error massage to print
 * @param children - the children array
 * @param numChildren - length of the array
 * */
void handleError(const char *msg, ChildInfo *children, int numChildren) {
	perror(msg);
	freeResources(children, numChildren);
	exit(EXIT_FAILURE);
}

/**Handles a SIGPIPE signal
 * Prints a message,
 * frees the memory associated with the program,
 * and exits*/
void sigpipeHandler(int signum, siginfo_t *info, void *ptr){
	printf("SIGPIPE for Manager Process %d. Leaving.\n", getpid());
	freeResources(children, numChildren);
	exit(EXIT_FAILURE);
}

/**Registers the handler function to the specified signal
 *
 * @param signal - the signal to be handled
 * @param action - structure to pass to the registration syscall
 * @param handlerFunc - the handler function to the specified signal
 *
 * @return
 * -1 on error
 * 0 on success*/
int registerHandler(int signal, struct sigaction *action, void (*handlerFunc) (int, siginfo_t *, void *)){
	action->sa_sigaction = handlerFunc;
	action->sa_flags = SA_SIGINFO;
	sigfillset(&action->sa_mask);
	// Register the handler to the signal
	if( 0 != sigaction(signal, action, NULL) ){
		return -1;
	}
	return 0;
}

/**Prints the file content
 *
 * @param fd - file descriptor
 *
 * @return
 * -1 on error
 * 0 on success
 * */
int printFile (int fd){
	int read_bytes;
	char buff[BUFFER_SIZE];
	while ((read_bytes = read(fd, buff, BUFFER_SIZE)) > 0) {
		buff[read_bytes] = '\0';
		printf("%s", buff);
	}
	if (read_bytes == -1){ //error
		return -1;
	}
	return 0;
}

int main(int argc, char** argv){

	if (argc < 3){
		printf("Error: not enough arguments");
		return EXIT_FAILURE;
	}

	char *fName = argv[1];		//path to data file
	char *pattern = argv[2];	//pattern to find
	int patternLen = strlen(pattern);

	children = NULL;
	numChildren = 0;

	// Structure to pass to the registration syscall
	struct sigaction sigpipe_action;
	memset(&sigpipe_action, 0, sizeof(sigpipe_action));
	if (registerHandler(SIGPIPE, &sigpipe_action, sigpipeHandler) < 0){
		handleError("ERROR: Signal handle registration failed", children, numChildren);
	}

	//allocate memory for children array
	children = (ChildInfo*)malloc(sizeof(ChildInfo)*patternLen);
	if (children == NULL){
		handleError("ERROR: malloc has failed", children, numChildren);
	}

	int curr_child;
	char currIndex[MAX_NAME_LEN];
	char currPipeName[MAX_NAME_LEN];
	//create a child processes for each symbol of the pattern
	for (int i=0; i<patternLen; i++){
		//create the fifo file name
		strcpy(currPipeName, SHARED_FILE);
		sprintf(currIndex, "%d", i);
		strcat(currPipeName, currIndex);

		if (mkfifo(currPipeName, 0600) == -1){
			handleError("ERROR: Failed making fifo", children, numChildren);
		}

		curr_child = fork();
		if(curr_child == -1) { //process creation failed
			unlink(currPipeName);
			handleError("ERROR: process creation failed", children, numChildren);
		}

		else if(curr_child  == 0) { //child process
			char *child_args[] = {"./sym_count", fName, pattern+i, currPipeName, NULL};
			if (execvp(child_args[0], child_args) == -1){
				//execvp failed
				handleError("ERROR: execvp failed for process", children, numChildren);
			}
		}
		else { //parent process
			numChildren++;
			//store the child info
			children[i].pid = curr_child;
			strcpy(children[i].pipeName, currPipeName);
			children[i].fd = open(children[i].pipeName, O_RDONLY | O_NONBLOCK);
			if (children[i].fd == -1){ //failed opening fifo file
				unlink(children[i].pipeName);
				handleError("ERROR: Failed opening file", children, numChildren);
			}
		}
	}

	//iterate over the launched processes
	int status;
	int res;
	while (numChildren > 0){
		sleep(1);
		for (int i=0;i<numChildren;i++){
			res = waitpid(children[i].pid, &status, WNOHANG);
			if(res == -1){ //error
				handleError("ERROR: waitpid failed", children, numChildren);
			}
			else if(res == children[i].pid && WIFEXITED(status)) { //process is finished
				//print the data reported by the process
				if(printFile(children[i].fd) == -1) {
					handleError("Failed reading from file", children, numChildren);
				}
				//close and unlink fifo file
				close(children[i].fd);
				unlink(children[i].pipeName);
				//exclude the process
				children[i] = children[numChildren-1];
				numChildren--;
			}
		}
	}

	freeResources(children, numChildren);
	exit(EXIT_SUCCESS);
}
