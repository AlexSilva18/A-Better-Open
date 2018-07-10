#include "netfilethreads.h"
#include <stdio.h>
#include <sys/socket.h>
#include <pthread.h>
#include <ctype.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include "colors.h"
#include "argumentparser.h"

struct filemutex * head;

enum readstate{
	BEGINNING,
	ARG1,
	ARG2,
	ARG3,
	ARG4,
	ARG5
};

void * netthreadopen(void * args){

	char buff[BUFFSIZE];
	int i;
	enum readstate state = BEGINNING; // from netfilethreads.h. Can have have values of BEGINNING, ARG1-ARG4
	char* filename;
	int filelen = 0;
	int filenameindex = 0; // index for copying filename
	int flags = 0;
	int socket_fd = ((struct threadinput*) args)->socket_fd;
	int pfd = 0;
	enum clientmode mode = 0;
	int nbytes;
	// struct sockaddr_storage client_address = ((struct threadinput*) args)->clientaddr; // this bit is not used yet, so it's commented for now.
	free(args);

	while((nbytes = read(socket_fd, buff, BUFFSIZE))){
		for(i = 0; i < nbytes; i++){
			if(state == BEGINNING){ // before we saw the thing
				if(isdigit(buff[i]))
					state = ARG1;
			}
			if(state == ARG1){
				state = parseInt((int *)&mode, buff, i, state);
			}
			else if(state == ARG2){
				if((state = parseInt(&filelen, buff, i, state)) == ARG3)
					filename = malloc(sizeof(char) * filelen + 1);
			}
			else if(state == ARG3){
				state = parseString(socket_fd, filename, buff, nbytes, filelen, &i, &filenameindex, state);
			}
			else if(state == ARG4){
				state = parseInt(&flags, buff, i, state);
			}
		}
	}
	if ((pfd = open(filename, flags)) == -1){
		fprintf(stderr, COL_RED "[ERROR]: Unable to open file.\n" COL_RESET);
		char string [1 + 1 + 19]; // 'e' + ' ' + errno
		sprintf(string, "e %d", errno);
		write(socket_fd, string, (int)strlen(string));
	}else{
		// open filename
		// \/ this is a test
		printf(COL_MAGENTA "Opening filename %s of length %d with flags %d and mode %d" COL_RESET "\n", filename, filelen, flags, mode);
		char string [sizeof(pfd)];
		sprintf(string, "%d\n", pfd);
		write(socket_fd, string, (int)strlen(string));
	}
	shutdown(socket_fd, SHUT_WR);
	close(socket_fd);

	struct clientfile * client = malloc(sizeof(struct clientfile));
	client->fd = pfd;
	client->mode = mode;
	client->next = NULL;
	struct filemutex * temp;
	for(temp = head; temp != NULL; temp = temp->next){
		if(strcmp(filename, temp->filename) == 0){
			client->next = temp->fds;
			temp->fds = client->next;
			break;
		}
	}
	if(client->next == NULL){			// if this is still null, then the file isn't already open
							//(otherwise, it would be what used to be first for that file)
		temp = malloc(sizeof(struct filemutex));
		temp->filename = filename;
		pthread_mutex_init(&temp->editmutex, NULL);
		temp->fds = client;
		temp->next = head;
		head = temp;				// by inserting in the front, we take care of the situation when head == NULL
	}

	return NULL;
}

void * netthreadread(void * args){

	char buff[BUFFSIZE];
	int i;
	enum readstate state = BEGINNING;
	int fd = 0;
	int readsize = 0;
	int nbytes;
	int socket_fd = ((struct threadinput*) args)->socket_fd;
	// struct sockaddr_storage client_address = ((struct threadinput*) args)->clientaddr;
	free(args);

	while((nbytes = read(socket_fd, buff, BUFFSIZE))){
		for(i = 0; i < nbytes; i++){
			if(state == BEGINNING){
				if(isdigit(buff[i]))
					state = ARG1;
			}
			if(state == ARG1){
				state = parseInt(&fd, buff, i, state);
			}
			else if(state == ARG2){
				state = parseInt(&readsize, buff, i, state);
			}
		}
	}
	printf(COL_MAGENTA "Recieved a read request of %d on file descriptor %d" COL_RESET "\n", readsize, fd);
	int bytes_read = -1;
	char buf[readsize];

	struct filemutex * ptr;
	enum clientmode mode;
	for(ptr = head; ptr != NULL; ptr = ptr->next){
		struct clientfile * file;
		for(file = ptr->fds; file != NULL; file = file->next){
			if(file->fd == fd){
				mode = file->mode;
				break;
			}
		}
		if(file != NULL)
			break;
	}

	if(ptr == NULL){
		fprintf(stderr, COL_RED "[ERROR]: File descriptor %d does not exist.\n", fd);
		errno = EACCES;
	}
	else{
		printf(COL_MAGENTA "Mode is %d\n" COL_RESET, mode);
		switch(mode){
			case UNRESTRICTED:
			case EXCLUSIVE:
				pthread_mutex_lock(&ptr->editmutex);
				if(ptr->readLock){
					fprintf(stderr, COL_RED "[ERROR]: Access denied.\n");
					errno = EACCES;
					pthread_mutex_unlock(&ptr->editmutex);
					break;
				}
				ptr->numReading++;
				pthread_mutex_unlock(&ptr->editmutex);

				bytes_read = read(fd, buf, readsize);

				pthread_mutex_lock(&ptr->editmutex);
				ptr->numReading--;
				pthread_mutex_unlock(&ptr->editmutex);
				break;
			case TRANSACTION:
				pthread_mutex_lock(&ptr->editmutex);
				if(ptr->numReading != 0 || ptr->numWriting != 0 || ptr->writeLock || ptr->readLock){
					fprintf(stderr, "[ERROR]: Access Denied.\n");
					errno = EACCES;
					pthread_mutex_unlock(&ptr->editmutex);
					break;
				}
				ptr->readLock = 1;
				ptr->writeLock = 1;
				ptr->numReading++;
				pthread_mutex_unlock(&ptr->editmutex);

				bytes_read = read(fd, buf, readsize);

				pthread_mutex_lock(&ptr->editmutex);
				ptr->numReading--;
				ptr->readLock = 0;
				ptr->writeLock = 0;
				pthread_mutex_unlock(&ptr->editmutex);
				break;
			default:
				fprintf(stderr, COL_RED "[ERROR]: Invalid mode: %d.\n", mode);
				errno = EACCES;
				break;
		}
	}

	if(bytes_read == -1){
		fprintf(stderr, COL_RED "[ERROR]: Could not read.\n" COL_RESET);
		char string[1 + 1 + sizeof(errno)];
		sprintf(string, "e %d", errno);
		write(socket_fd, string, strlen(string));
	}else{
		char string [19 + bytes_read];// num bytes read + what was read
		sprintf(string, "%d %s", bytes_read, buf);
		write(socket_fd, string, strlen(string));
	}
	shutdown(socket_fd, SHUT_WR);
	close(socket_fd);
	return NULL;
}

void * netthreadwrite(void * args){

	char buff[BUFFSIZE];
	int i;
	enum readstate state = BEGINNING;
	int fd = 0;
	int writesize = 0;
	int nbytes;
	char * buf; // stores what the client wants to write
	int bufindex = 0;
	int socket_fd = ((struct threadinput*) args)->socket_fd;
	// struct sockaddr_storage client_address = ((struct threadinput*) args)->clientaddr;
	free(args);

	while((nbytes = read(socket_fd, buff, BUFFSIZE))){
		for(i = 0; i < nbytes; i++){
			if(state == BEGINNING){
				if(isdigit(buff[i]))
					state = ARG1;
			}
			if(state == ARG1){
				state = parseInt(&fd, buff, i, state);
			}
			else if(state == ARG2){
				if((state = parseInt(&writesize, buff, i, state)) == ARG3)
					buf = (char *)malloc(sizeof(char) * writesize + 1);
			}
			else if(state == ARG3){
				state = parseString(socket_fd, buf, buff, nbytes, writesize, &i, &bufindex, state);
			}
		}
	}
	printf(COL_MAGENTA "Recieved a write request of %d on file descriptor %d" COL_RESET "\n", writesize, fd);
	int bytes_written = -1;

	struct filemutex * ptr;
	enum clientmode mode;
	for(ptr = head; ptr != NULL; ptr = ptr->next){
		struct clientfile * file;
		for(file = ptr->fds; file != NULL; file = file->next){
			if(file->fd == fd){
				mode = file->mode;
				break;
			}
		}
		if(file != NULL)
			break;
	}

	if(ptr == NULL){
		fprintf(stderr, COL_RED "[ERROR]: File descriptor %d does not exist.\n", fd);
		errno = EACCES;
	}
	else{
		switch(mode){
			case UNRESTRICTED:
				pthread_mutex_lock(&ptr->editmutex);
				if(ptr->writeLock){
					fprintf(stderr, COL_RED "[ERROR]: Access Denied.\n");
					errno = EACCES;
					pthread_mutex_unlock(&ptr->editmutex);
					break;
				}
				ptr->numWriting++;
				pthread_mutex_unlock(&ptr->editmutex);

				bytes_written = write(fd, buf, writesize);

				pthread_mutex_lock(&ptr->editmutex);
				ptr->numWriting--;
				pthread_mutex_unlock(&ptr->editmutex);
				break;
			case EXCLUSIVE:
				pthread_mutex_lock(&ptr->editmutex);
				if(ptr->numWriting != 0 || ptr->writeLock){
					fprintf(stderr, "[ERROR]: Access Denied.\n");
					errno = EACCES;
					pthread_mutex_unlock(&ptr->editmutex);
					break;
				}
				ptr->writeLock = 1;
				ptr->numWriting++;
				pthread_mutex_unlock(&ptr->editmutex);

				bytes_written = write(fd, buf, writesize);

				pthread_mutex_lock(&ptr->editmutex);
				ptr->numWriting--;
				ptr->writeLock = 0;
				pthread_mutex_unlock(&ptr->editmutex);
				break;
			case TRANSACTION:
				pthread_mutex_lock(&ptr->editmutex);
				if(ptr->numReading != 0 || ptr->numWriting != 0 || ptr->writeLock || ptr->readLock){
					fprintf(stderr, "[ERROR]: Access Denied.\n");
					errno = EACCES;
					pthread_mutex_unlock(&ptr->editmutex);
					break;
				}
				ptr->readLock = 1;
				ptr->writeLock = 1;
				ptr->numWriting++;
				pthread_mutex_unlock(&ptr->editmutex);

				bytes_written = write(fd, buf, writesize);

				pthread_mutex_lock(&ptr->editmutex);
				ptr->numWriting--;
				ptr->readLock = 0;
				ptr->writeLock = 0;
				pthread_mutex_unlock(&ptr->editmutex);
				break;
			default:
				fprintf(stderr, COL_RED "[ERROR]: Invalid mode: %d.\n", mode);
				errno = EACCES;
				break;
		}
	}

	if(bytes_written == -1){
		fprintf(stderr, COL_RED "[ERROR]: Error writing.\n" COL_RESET);
		char string[1 + 1 + sizeof(errno)];
		sprintf(string, "e %d", errno);
		write(socket_fd, string, strlen(string));
	}else{
		char string [sizeof(bytes_written)];
		sprintf(string, "%d", bytes_written);
		write(socket_fd, string, strlen(string));
	}
	shutdown(socket_fd, SHUT_WR);
	close(socket_fd);
	return NULL;

}

void * netthreadclose(void * args){
	char buff[BUFFSIZE];
	int i;
	enum readstate state = BEGINNING; // from netfilethreads.h. Can have have values of BEGINNING, ARG1-ARG4
	//char* filename;
	int fd = 0;
	//int filelen = 0;
	//int filenameindex = 0; // index for copying filename
	int socket_fd = ((struct threadinput*) args)->socket_fd;
	int nbytes;
	// struct sockaddr_storage client_address = ((struct threadinput*) args)->clientaddr; // this bit is not used yet, so it's commented for now.
	free(args);

	while((nbytes = read(socket_fd, buff, BUFFSIZE))){
		buff[nbytes] = '\0';
		for(i = 0; i < nbytes; i++){
			if(state == BEGINNING){
				if(isdigit(buff[i]))
					state = ARG1;
			}
			if(state == ARG1){
				state = parseInt(&fd, buff, i, state);
			}
		}
	}
	printf(COL_MAGENTA "Closing fd %d" COL_RESET "\n", fd);
	if ((close(fd)) == -1){
		char string[1 + 1 + sizeof(errno)]; // 'e' + ' ' + errno
		fprintf(stderr, COL_RED "[ERROR]: Unable to close file.\n" COL_RESET);
		sprintf(string, "e %d", errno);
		write(socket_fd, string, (int)strlen(string));
	}
	else{
		write(socket_fd, "0", 1);
	}

	shutdown(socket_fd, SHUT_WR);
	close(socket_fd);

	struct filemutex * prevptr = NULL;
	struct filemutex * ptr;
	struct clientfile * prev = NULL;
	struct clientfile * file;
	for(ptr = head; ptr != NULL; ptr = ptr->next){
		for(file = ptr->fds; file != NULL; file = file->next){
			if(file->fd == fd){
				break;
			}
			prev = file;
		}
		if(file != NULL)
			break;
		prevptr = ptr;
	}
	if(ptr == NULL){
		fprintf(stderr, COL_RED "[ERROR]: File descriptor %d does not exist.\n", fd);
		return NULL;
	}

	if(prev == NULL)
		ptr->fds = file->next;
	else
		prev->next = file->next;
	free(file);

	if(ptr->fds == NULL){
		if(prevptr == NULL)
			head = ptr->next;
		else
			prevptr->next = ptr->next;
		pthread_mutex_destroy(&ptr->editmutex);
		free(ptr->filename);
		free(ptr);
	}
	return NULL;
}
