/*
 * hw4.c
 *
 *  Created on: 22 May 2018
 *      Author: lital
 */

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#define BUF_SIZE 1048576 //1024*1024

//globals
char shared_buff [BUF_SIZE] = {0}; //shared buffer
int outFD; //output file descriptor
int outFileSize = 0; //total size of output file
int cnt = 0; //number of threads finished the current step
int maxLen = 0; //
int numThreads; //number of threads at current step
int currStep = 0; //the current step

pthread_mutex_t lock;
pthread_cond_t  cond;

/** Frees all the memory associated with the program */
void freeResources(){
	close(outFD);
	pthread_mutex_destroy( &lock );
	pthread_cond_destroy( &cond );
}

/**Prints the error massage specified,
 * frees the memory ,
 * and exits with EXIT_FAILURE
 *
 * @param msg - the error massage to print
 * */
void handleError(const char *msg, const char *err) {
	printf("%s: %s\n",msg, err);
	freeResources();
	exit(EXIT_FAILURE);
}

/**In every step, reads the next chunk of size BUF_SIZE from the input file specified,
 * XORs the data in the chunk into the shared buffer.
 * If it is the last thread to XOR its data, it also writes the result to the output file.
 *
 * @param t - the input file name
 * */
void* reader(void* t)
{
	int rc;
	int localStep = 0; //current step of the thread
	char *fName = (char *)t; //input file name
	int fd = open(fName, O_RDONLY);
	if (fd < 0){ //error
		handleError("ERROR: Unable opening file", strerror(errno));
	}

	char buffer[BUF_SIZE];
	char *p;
	int lenRead, lenWrote;

	while ((lenRead = read(fd, buffer, BUF_SIZE)) >= 0){ //read next chunk from input file

		//lock
		rc = pthread_mutex_lock(&lock);
		if( 0 != rc ) {
			close(fd);
			handleError("ERROR in pthread_mutex_lock()", strerror(rc));
		}

		//wait for next step
		while (localStep != currStep){
			rc = pthread_cond_wait(&cond, &lock);
			if( 0 != rc ) { //error
				close(fd);
				handleError("ERROR in pthread_cond_wait()", strerror(rc));
			}
		}

		localStep++;

		//xor to shared buffer
		for (int i = 0; i < lenRead; i++){
			shared_buff[i] ^= buffer[i];
		}

		cnt++;

		if (lenRead > maxLen){
			maxLen = lenRead;
		}

		//the last thread
		if (cnt == numThreads){
			//write to file
			p = shared_buff;
			int n = maxLen;
			while ((lenWrote = write(outFD, p, n)) != n){
				n -= lenWrote;
				p += lenWrote;
			}
			if (lenWrote < 0){ //error
				close(fd);
				handleError("ERROR unable writing to file", strerror(errno));
			}
			//update globals
			memset(shared_buff, 0, BUF_SIZE);
			outFileSize += maxLen;
			maxLen = 0;
			cnt = 0;
			//continue to next step
			currStep++;
			pthread_cond_broadcast(&cond);
		}

		//EOF
		if (lenRead == 0){
			numThreads--;
			if (cnt > 0){
				cnt--;
			}
		}

		//unlock
		rc = pthread_mutex_unlock(&lock);
		if( 0 != rc ) {
			close(fd);
			handleError("ERROR in pthread_mutex_unlock()", strerror( rc ));
		}

		if (lenRead == 0) {//exit the loop
			break;
		}
	}

	if (lenRead < 0){ //error
		close(fd);
		handleError("ERROR: Unable reading file", strerror(errno));
	}

	close(fd);

	pthread_exit(NULL);
}

int main (int argc, char *argv[])
{
	char *outName = argv[1]; //output file name
	int numInputs = argc-2; //num of input files
	numThreads = numInputs;

	printf("Hello, creating %s from %d input files\n", outName, numInputs);

	outFD = open(outName, O_CREAT|O_WRONLY|O_TRUNC, 777);
	if (outFD < 0){ //error
		perror("ERROR: Unable opening file");
		exit(EXIT_FAILURE);
	}

	pthread_t thread[numInputs];
	int       rc;
	void*     status;

	//Initialize mutex
	rc = pthread_mutex_init( &lock, NULL );
	if(rc){ //error
		handleError("ERROR in pthread_mutex_init()", strerror(rc));
	}
	//Initialize cond
	rc = pthread_cond_init(&cond, NULL);
	if(rc){ //error
		handleError("ERROR in pthread_cond_init()", strerror(rc));
	}

	currStep = 0;
	//Launch threads
	for (int i=0; i<numInputs; i++){
		rc = pthread_create( &thread[i],
				NULL,
				reader,
				(void*) argv[i+2] );
		if(rc) { //error
			handleError("ERROR in pthread_create()", strerror(rc));
		}
	}

	// Wait for threads to finish
	for( int i = 0; i < numInputs; i++ ) {
		rc = pthread_join(thread[i], &status);
		if (rc) { //error
			handleError("ERROR in pthread_mutex_join()", strerror( rc ));
		}
	}

	printf("Created %s with size %d bytes\n", outName, outFileSize);

	freeResources();
	pthread_exit(EXIT_SUCCESS);
}


