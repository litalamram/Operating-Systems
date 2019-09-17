/*
 * message_sender.c
 *
 *  Created on: 2 May 2018
 *      Author: lital
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include "message_slot.h"

int main (int argc, char *argv[]){
	if (argc < 4){ //not enough arguments
		printf("ERROR: not enough arguments\n");
		exit(-1);
	}

	char *filePath = argv[1]; //message slot file path
	int chanel = atoi(argv[2]); //the target message channel id
	char *message = argv[3]; //the message to pass

	int fd = open(filePath, O_RDWR); //open the specified message slot device file
	if (fd < 0){ //error
		perror("ERROR: failed open file");
		exit (-1);
	}

	int ret_val = ioctl(fd, MSG_SLOT_CHANEL, chanel); //set the channel id to the id specified
	if (ret_val < 0 ){//error
		perror("ERROR: failed set chanel");
		close(fd);
		exit(-1);
	}

	ret_val = write(fd, message, strlen(message)); //write the specified message to the file
	if (ret_val < 0){ //error
		perror("ERROR: failed writing to file");
		close(fd);
		exit(-1);
	}

	close(fd); //close the device

	printf("%d bytes written to %s\n", ret_val, filePath); //print a status message

	return 0;

}

