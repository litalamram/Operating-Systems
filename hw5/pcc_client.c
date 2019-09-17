/*
 * pcc_client.c
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
#include <sys/stat.h>
#include <fcntl.h>

#define BUFFER_SIZE 2048

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
	int left = sizeof(uint32_t);;
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

/** Writes the specified char array to the file descriptor
 *
 * @param
 * fd - socket file descriptor
 * len - length of the array
 * arr - the array
 *
 * @return
 * 0 - on success
 * -1 - on error
 * */
int writeArr(int fd, int len, unsigned char *arr){
	int bytes_to_send = len;
	int bytes_sent;
	while (bytes_to_send > 0) {
		bytes_sent = write(fd, arr, bytes_to_send);
		if (bytes_sent == -1) {
			return -1;
		}
		bytes_to_send -= bytes_sent;
		arr += bytes_sent;
	}
	return 0;

}

int main(int argc, char *argv[]) {
	//command line arguments
	char *host = argv[1];
	char *port = argv[2];
	unsigned long len = strtoul(argv[3], NULL, 10);

	//create socket
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd == -1) {
		perror("Failed creating socket");
		exit(EXIT_FAILURE);
	}

	//parse host to ip and connect to host
	struct addrinfo *result;
	int s = getaddrinfo(host, port, NULL, &result);
	if (s != 0) {
		perror("Erron in getaddrinfo()");
		exit(EXIT_FAILURE);
	}
	//try each address getaddrinfo() returned until we successfully connect(2).
	struct addrinfo *rp;
	for (rp = result; rp != NULL; rp = rp->ai_next) {
		if (connect(sockfd, rp->ai_addr, rp->ai_addrlen) != -1) {//success
			break;
		}

	}
	//no address succeeded
	if (rp == NULL) {
		perror("Could not connect");
		exit(EXIT_FAILURE);
	}
	freeaddrinfo(result);

	//send the length to the server
	if (writeInt(sockfd, htonl(len)) < 0){
		perror("Failed sending data to server");
		exit(EXIT_FAILURE);
	}

	//read from "/dev/urandom" and send the data to server
	int fd = open ("/dev/urandom",O_RDONLY);
	if (fd == -1){
		perror("Failed opening file");
		exit(EXIT_FAILURE);
	}
	unsigned char buffer[BUFFER_SIZE];
	unsigned long toRead = len;
	int bytesRead;
	int chunk;
	while (toRead > 0){
		chunk = (toRead < BUFFER_SIZE) ? toRead : BUFFER_SIZE;
		bytesRead = read(fd, buffer, chunk);
		if (bytesRead < 0){
			perror("Failed reading file");
			exit(EXIT_FAILURE);
		}

		//send to server
		if (writeArr(sockfd, bytesRead, buffer) < 0){
			perror("Failed sending data to server");
			exit(EXIT_FAILURE);
		}

		toRead -= bytesRead;

	}
	close(fd);


	//read from server
	uint32_t num;
	if (readInt(sockfd, &num) < 0){
		perror("Failed getting data from server");
		exit(EXIT_FAILURE);
	}
	num = ntohl(num);
	printf("# of printable characters: %u\n", num);

	close(sockfd);

	return EXIT_SUCCESS;
}
