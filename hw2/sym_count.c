#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>


#define MAX_LINE 100

//globals
int inputFile = -1, pipeFile = -1;
int cnt = 0;
char symbol;
char *arr = NULL; //pointer to the shared memory
int fileSize = 0;

/**Frees all the memory associated with the program
 *
 * @param fd1 - file descriptor
 * @param fd2 - file descriptor
 * @param arr - pointer to the shared memory
 * @param fileSize - the shared memory size
 *
 * */
void freeResources(int fd1, int fd2, char* arr, int fileSize){
	if (fd1 != -1) close(fd1);
	if (fd2 != -1) close(fd2);
	if (arr != NULL){
		if (munmap(arr, fileSize) == -1){
			perror("ERROR: failed un-mapping file");
			exit(EXIT_FAILURE);
		}
	}
}

/**Prints the error massage specified,
 * frees the memory associated with the given array,
 * and exits
 *
 * @param fd1 - file descriptor
 * @param fd2 - file descriptor
 * @param arr - pointer to the shared memory
 * @param fileSize - the shared memory size
 *
 * */
void handleError(const char *msg, int fd1, int fd2, char *arr, int fileSize) {
	perror(msg);
	freeResources(inputFile, pipeFile, arr, fileSize);
	exit(EXIT_FAILURE);
}

/**Handles a SIGTERM signal
 *
 * frees the memory associated with the program
 * and exits*/
void sigtermHandler(int signum, siginfo_t *info, void *ptr){
	freeResources(inputFile, pipeFile, arr, fileSize);
	exit(EXIT_SUCCESS);
}

/**Handles a SIGPIPE signal
 * Prints a message,
 * frees the memory associated with the program,
 * and exits*/
void sigpipeHandler(int signum, siginfo_t *info, void *ptr){
	printf("SIGPIPE for process %d. Symbol %c. Counter %d. Leaving.\n", getpid(), symbol, cnt);
	freeResources(inputFile, pipeFile, arr, fileSize);
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
int registerHandler(int signal, struct sigaction *action, void (*sa_action) (int, siginfo_t *, void *)){
	action->sa_sigaction = sa_action;
	action->sa_flags = SA_SIGINFO;
	sigfillset(&action->sa_mask);
	// Register the handler to the signal
	if( 0 != sigaction(signal, action, NULL) ){
		return -1;
	}
	return 0;
}

int main(int argc, char** argv)
{
	if (argc < 4){
		printf("Error: not enough arguments");
		return EXIT_FAILURE;
	}
	inputFile = -1;
	pipeFile = -1;
	char *fileName = argv[1];	//the input file name
	symbol = argv[2][0];	    //the symbol to find
	char *pipeName = argv[3];	//the pipe file name
	cnt = 0;

	//structures to pass to the registration syscall
	struct sigaction sigterm_action;
	memset(&sigterm_action, 0, sizeof(sigterm_action));
	//register the handler to SIGTERM
	if (registerHandler(SIGTERM, &sigterm_action, sigtermHandler) < 0){
		handleError("ERROR: Signal handle registration failed", inputFile, pipeFile, arr, fileSize);
	}

	struct sigaction sigpipe_action;
	memset(&sigpipe_action, 0, sizeof(sigpipe_action));
	//register the handler to SIGPIPE
	if (registerHandler(SIGPIPE, &sigpipe_action, sigpipeHandler) < 0){
		handleError("ERROR: Signal handle registration failed", inputFile, pipeFile, arr, fileSize);
	}

	//input file
	inputFile = open(fileName, O_RDWR);
	if (inputFile == -1){
		handleError("ERROR: failed opening file\n", inputFile, pipeFile, arr, fileSize);
	}
	//get file size
	struct stat fileStat;
	if (stat(fileName, &fileStat) < 0){
		handleError("ERROR: failed getting file information\n", inputFile, pipeFile, arr, fileSize);
	}
	fileSize = fileStat.st_size;
	// Create a string pointing to the shared memory
	arr = (char *)mmap(NULL, fileSize, PROT_READ, MAP_SHARED, inputFile, 0);
	if (arr == MAP_FAILED){
		arr = NULL;
		handleError("ERROR: failed mapping file to memory", inputFile, pipeFile, arr, fileSize);
	}

	//count instances of the symbol
	for (int i=0;i<fileSize;i++){
		if (arr[i] == symbol){
			cnt++;
		}
	}

	//open pipe file
	pipeFile = open(pipeName, O_WRONLY);
	if (pipeFile == -1) {//error openenig file
		handleError("ERROR: Failed opening file", inputFile, pipeFile, arr, fileSize);
	}
	
	//write the result to the pipe file
	char res[MAX_LINE];
	sprintf(res, "Process %d finishes. Symbol %c. Instances %d.\n", getpid(), symbol, cnt);
	if (write(pipeFile, res, strlen(res)) == -1){ //error writing to file
		handleError("ERROR: Failed writing to pipe", inputFile, pipeFile, arr, fileSize);
	}

	// Free resources
	freeResources(inputFile, pipeFile, arr, fileSize);
	exit(EXIT_SUCCESS);

}


