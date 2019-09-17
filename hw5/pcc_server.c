/*
 * pcc_server.c
 *
 *  Created on: 10 Jun 2018
 *      Author: lital
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <errno.h>

#define NUM_PCC 95 //number of printable chars
#define MIN_PCC 32 //minimum value of printable char
#define MAX_PCC 126 //maximum value of printable char
#define BUFFER_SIZE 2048
#define CONNECTION_QUEUE_SIZE 100

unsigned long pcc_count[NUM_PCC] = {0}; //global counter for printable chars
pthread_t *threads; //threads array
int num_threads; //total number of threads
pthread_mutex_t lock;
int listenfd; //listening socket file descriptor
int isTerm = 0; //has SIGTERM been received

/**
 * Writes the specified 4 bytes number to the file descriptor
 *
 * @param fd - the file descriptor
 * @param num - the 4 bytes number to write
 *
 * @return
 * 0 - on success
 * -1 - on error
 * */
int writeInt(int fd, uint32_t num){
	char *data = (char*)&num;
	int left = sizeof(uint32_t);
	int bytes_sent;
	while (left > 0) {
		bytes_sent = write (fd, data, left);
		if (bytes_sent < 0){
			return -1;
		}
		else {
			data += bytes_sent;
			left -= bytes_sent;
		}
	}
	return 0;
}

/** Reads 4 bytes from the specified file descriptor
 *
 * @param fd - socket file descriptor
 * @param res - pointer to get the number read
 *
 * @return
 * -1 - on error
 * 0 - on success
 * */
int readInt (int fd, uint32_t *res){
	char *data = (char*)res;
	int left = sizeof(uint32_t);
	int bytes_read;

	while(left > 0) {
		bytes_read = read(fd, data, left);
		if (bytes_read < 0){
			return -1;
		}
		else{
			data += bytes_read;
			left -= bytes_read;
		}

	}
	return 0;
}

/** Returns whether the specified char is printable*/
int isPrintableChar(unsigned char c){
	return c>=MIN_PCC && c<=MAX_PCC;
}


/** Updates the number of times each printable character
 * has been appeared in the buffer
 *
 * @param buffer - array of characters
 * @param len - the lrngth of the buffer array
 * @param cntArr - array contains the number of times
 * 				   each printable characters appears
 * */
void updateLocalCounter(unsigned char *buffer, int len, unsigned long *cntArr){
	for (int i=0; i<len; i++){
		if (isPrintableChar(buffer[i])){ //printable character
			cntArr[buffer[i]-MIN_PCC]++;
		}
	}
}

/** Updates the global pcc_count array
 * according to the array specified
 *
 * @param cntArr - array contains the number of times
 * 				   each printable characters appears
 * */
void updateGlobalCounter(unsigned long *cntArr){
	for (int i=0; i<NUM_PCC; i++){
		pcc_count[i] += cntArr[i];
	}
}

/** Returns the sum of all values in the array*/
unsigned long sumArr(unsigned long *arr, int len){
	unsigned long sum = 0;
	for (int i=0; i<len; i++){
		sum+=arr[i];
	}
	return sum;
}

/** Reads the number of bytes specified from the file descriptor
 * and counts how many times each printable character appeared
 *
 * @param fd - file descriptor
 * @param toRead - number of bytes to read
 * @param cntArr - array to get the number of times each printable char appeared
 *
 * @return
 * 0 - on success
 * -1 - on error*/
int getCntArr(int fd, unsigned long toRead, unsigned long *cntArr){
	unsigned char buff[BUFFER_SIZE];
	int read_bytes;
	int chunk;
	while (toRead > 0){
		chunk = (toRead < BUFFER_SIZE) ? toRead : BUFFER_SIZE;
		read_bytes = read(fd, buff, chunk);
		if (read_bytes == -1) {
			return -1;
		}

		updateLocalCounter(buff, read_bytes, cntArr);

		toRead -= read_bytes;
	}

	return 0;
}

/** Reads a stream of bytes from the client,
 * computes its printable character count,
 * and writes the result to the client over the TCP connection.
 * Also updates the pcc_count global data structure
 *
 * @param t - connection file descriptor*/
void* clientThread (void *t){
	int connfd = (int)t;
	//read the length of the char array from the cliennt
	uint32_t len;
	if (readInt(connfd, &len) < 0){ //error
		perror("ERROR: Failed sending data to client");
		exit(EXIT_FAILURE);
	}
	len = ntohl(len);

	//read the char array from the client
	//and compute the number of times each printable char appeared
	unsigned long cntArr[NUM_PCC];
	if (getCntArr(connfd, len, cntArr) < 0){ //error
		perror("ERROR: Failed getting data from client");
		exit(EXIT_FAILURE);
	}

	//lock
	int rc = pthread_mutex_lock(&lock);
	if( 0 != rc ) { //error
		perror("ERROR in pthread_mutex_lock()");
		exit(EXIT_FAILURE);
	}
	//update the global pcc_count
	updateGlobalCounter(cntArr);
	//unlock
	rc = pthread_mutex_unlock(&lock);
	if( 0 != rc ) { //error
		perror("ERROR in pthread_mutex_unlock()");
		exit(EXIT_FAILURE);
	}

	unsigned long cnt = sumArr(cntArr, NUM_PCC); //total number of printable characters

	//send the printable character count to the client
	if (writeInt(connfd, htonl(cnt)) < 0){
		perror("Failed sending data");
		exit(EXIT_FAILURE);
	}

	close(connfd);

	return NULL;

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

/**Handles a SIGTERM signal
 * Closes the listening socket file descriptor
 * and sets done to true
 * */
void sigtermHandler(int signum, siginfo_t *info, void *ptr){
	close(listenfd);
	isTerm = 1;
}

int main(int argc, char *argv[]) {
	//structures to pass to the registration syscall
	struct sigaction sigterm_action;
	memset(&sigterm_action, 0, sizeof(sigterm_action));
	//register the handler to SIGINT
	if (registerHandler(SIGINT, &sigterm_action, sigtermHandler) < 0){
		perror("ERROR: Signal handle registration failed");
		exit(EXIT_FAILURE);
	}

	//port
	unsigned int port = strtoul(argv[1], NULL, 10);

	//create a listening socket
	listenfd = socket(AF_INET, SOCK_STREAM, 0);
	if (listenfd == -1) {
		perror("Failed creating listening socket");
		exit(EXIT_FAILURE);
	}

	struct sockaddr_in serv_addr;
	memset(&serv_addr, 0, sizeof(struct sockaddr_in));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(port);

	//bind socket to specified port
	if (bind(listenfd, (struct sockaddr *)&serv_addr, sizeof(struct sockaddr_in)) == -1) {
		perror("Failed binding socket");
		close(listenfd);
		exit(EXIT_FAILURE);
	}

	if (listen(listenfd, CONNECTION_QUEUE_SIZE) == -1) {
		perror("Failed to start listening to incoming connections");
		close(listenfd);
		exit(EXIT_FAILURE);
	}

	//init threads array
	threads = (pthread_t*)malloc(sizeof(pthread_t));
	if (threads == NULL){
		printf("ERROR: malloc has failed\n");
		exit(EXIT_FAILURE);
	}
	num_threads = 0;

	//main loop
	while (!isTerm) {
		//accept new connection
		struct sockaddr_in client_addr;
		socklen_t client_addr_size = sizeof(struct sockaddr_in);
		int connfd = accept(listenfd, (struct sockaddr *)&client_addr, &client_addr_size);
		if (connfd == -1) { //error
			if (errno == EINTR && isTerm){ //accept failed due to SIGTERM
				break;
			}
			else {
				perror("ERROR: Failed accepting client connection");
				exit(EXIT_FAILURE);
			}
		}
		//create new thread to handle the connection
		int rc = pthread_create(&threads[num_threads++], NULL, clientThread, (void*)connfd);
		threads = realloc(threads, (num_threads+1)*sizeof(pthread_t));
		if (threads == NULL){ //error
			printf("ERROR: realloc has failed\n");
			exit(EXIT_FAILURE);
		}
		if(rc) { //error
			perror("ERROR in pthread_create()");
			exit(EXIT_FAILURE);

		}

	}

	// Wait for all active threads to finish
	for( int i = 0; i < num_threads; i++ ) {
		int rc = pthread_join(threads[i], NULL);
		if (rc) { //error
			perror("ERROR in pthread_mutex_join()");
			exit(EXIT_FAILURE);
		}
	}
	//print out the number of times each printable character has been observed
	for (int i=0; i<NUM_PCC; i++){
		printf("char '%c' : %u times\n", i+MIN_PCC, (uint32_t)pcc_count[i]);
	}

	exit(EXIT_SUCCESS);
}
