#include "libnetfiles.h"
#include "colors.h"
#include "argumentparser.h"
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>
#include <ctype.h>

struct addrinfo* res;
int mode;

//helper functions
int connecttoserver(){
	struct addrinfores;
	int return_fd;
	for(r = res; r != NULL; r = r->ai_next){
		return_fd = socket(r->ai_family, r->ai_socktype, 0);

		if(return_fd == -1)
			continue;

		if(connect(return_fd, r->ai_addr, r->ai_addrlen) == -1){
			close(return_fd);
			continue;
		}
		return return_fd;
	}
	fprintf(stderr, COL_RED "[ERROR]: Could not connect to server.\n" COL_RESET);
	return -1;
}

int writetoserver(int socket_fd, char * str, int len){
	if(write(socket_fd, str, len) == -1){
		fprintf(stderr, COL_RED "[ERROR]: Could not write to socket.\n" COL_RESET);
		close(socket_fd);
		return -1;
	}
	shutdown(socket_fd, SHUT_WR);
	return 0;
}

int processerror(int socket_fd, char* buff, int nbytes){
	int i;
	errno = 0;
	buff[nbytes] = '\0';
	do{
		for(i = 0; i < nbytes; i++){
			if(isdigit(buff[i])){
				errno = errno * 10 + (buff[i] - '0');
			}
		}
	}while((nbytes = read(socket_fd, buff, 50)));
	return -1;
}


//net functions implementation

int netserverinit(char* hostname, int openmode){

	int rc;
	struct addrinfo hints;

	mode = openmode;

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	
	printf(COL_GREEN "Successfully set parameters.\n" COL_RESET);
	rc = getaddrinfo(hostname, "64064", &hints, &res);

	if(rc != 0){
		fprintf(stderr, COL_RED "[ERROR]: GAI: Return code %d: %s\n" COL_RESET, rc, gai_strerror(rc));
		return -1;
	}
	return 0;
	/* for(r = res; r != NULL; r->ai_next){ */
	/* 	socket_fd = socket(r->ai_family, r->ai_socktype, 0); */

	/* 	if(socket_fd == -1){ */
	/* 		continue; */
	/* 	} */
	/* 	// found a socket */
	/* 	res = r; */
	/* 	return 0; */
	/* } */
	/* fprintf(stderr, COL_RED "[ERROR]: Could not find socket.\n" COL_RESET); */
	/* return -1; */
}

int netopen(const char* pathname, int flags){

	int nbytes;
	int fd = 0;
	int socket_fd;
	char buff[50];

	if((socket_fd = connecttoserver()) == -1)
		return -1;

	char string[24 + strlen(pathname)];
	sprintf(string, "open %d %d %s %d\n", mode, (int)strlen(pathname), pathname, flags);	// generate string to write
	if(writetoserver(socket_fd, string, strlen(string)) == -1)	// write request
		return -1;


	nbytes = read(socket_fd, buff, 50);				// read response (priming read)

	if(buff[0] == 'e'){
		return processerror(socket_fd, buff, nbytes);
	}

	int i;
	int state = 0;
	do {
		buff[nbytes] = '\0';
		for(i = 0; i < nbytes; i++){
			if(state == 0){
				if(isdigit(buff[i]))
					state = 1;
			}
			if(state == 1){
				state = parseInt(&fd, buff, i, state);
			}
			if(buff[i] == EOF){
				break;
			}
		}
		if(buff[i] == EOF){
			break;
		}
	} while((nbytes = read(socket_fd, buff, 50)));
	close(socket_fd);
	return -fd;
}

ssize_t netread(int filedes, void* buf, size_t numbytes){
	// int rc;
	int nbytes;
	int lenRead = 0;
	int bufindex = 0;
	int socket_fd;
	char buff[19+1+numbytes+1];// number + space + read + null byte

	if((socket_fd = connecttoserver()) == -1)
		return -1;

	char string[45];
	sprintf(string, "read %d %d\n", (-filedes), (int) numbytes);	// generate string to write
	if(writetoserver(socket_fd, string, strlen(string)) == -1)	// write request
		return -1;

	nbytes = read(socket_fd, buff, numbytes);				// read response

	if(buff[0] == 'e'){
		return processerror(socket_fd, buff, nbytes);
	}

	int i;
	int state = 0;
	do {
	  buff[nbytes] = '\0';
		for(i = 0; i < nbytes; i++){
			if(state == 0){
			        // not working
				state = parseInt(&lenRead, buff, i, state);
			}
			else if(state == 1){
			        // not working
				state = parseString(socket_fd, buf, buff, nbytes, lenRead, &i, &bufindex, state);
			}
			if(buff[i] == EOF){
				break;
			}
		}
		if(buff[i] == EOF){
			break;
		}
	} while((nbytes = read(socket_fd, buff, nbytes + sizeof(nbytes))));
	// nbytes = read(socket_fd, buff, nbytes + sizeof(nbytes);
	close(socket_fd);
	((char *)buf)[lenRead] = '\0';
	return lenRead;
}

ssize_t netwrite(int fildes, const void* buf, size_t numbytes){
  	// int rc;
	int nbytes;
        int lenWritten = 0;
	int socket_fd;
	char buff[numbytes+1];

	if((socket_fd = connecttoserver()) == -1)
		return -1;

	char string[47 + numbytes];
	sprintf(string, "write %d %d %s\n", (-fildes), (int) numbytes, (const char *)buf);	// generate string to write
	if(writetoserver(socket_fd, string, strlen(string)) == -1)	// write request
		return -1;

	nbytes = read(socket_fd, buff, numbytes);				// read response

	if(buff[0] == 'e'){
		return processerror(socket_fd, buff, nbytes);
	}

	int i;
	int state = 0;
	do {
	  buff[nbytes] = '\0';
		for(i = 0; i < nbytes; i++){
			if(state == 0){
			        // not working
				state = parseInt(&lenWritten, buff, i, state);
			}
			if(buff[i] == EOF){
				break;
			}
		}
		if(buff[i] == EOF){
			break;
		}
	} while((nbytes = read(socket_fd, buff, nbytes + sizeof(nbytes))));
	//nbytes = read(socket_fd, buff, nbytes + sizeof(nbytes);

	close(socket_fd);
	((char *)buf)[lenWritten] = '\0';
	return lenWritten;

}

int netclose(int fd){
        int nbytes;
	//int filedes = 0;
	int socket_fd;
	char buff[50];

	if((socket_fd = connecttoserver()) == -1)
		return -1;

	char string[6 + sizeof(fd)];
	sprintf(string, "close %d\n", (-fd));	// generate string to write
	if(writetoserver(socket_fd, string, strlen(string)) == -1)	// write request
		return -1;


	nbytes = read(socket_fd, buff, 50);				// read response (priming read)

	if(buff[0] == 'e'){
		return processerror(socket_fd, buff, nbytes);
	}

	//int i;
	//int state = 0;
	/* do { */
	/* 	buff[nbytes] = '\0'; */
	/* 	for(i = 0; i < nbytes; i++){ */
	/* 		if(state == 0){ */
	/* 			if(isdigit(buff[i])) */
	/* 				state = 1; */
	/* 		} */
	/* 		if(state == 1){ */
	/* 			state = parseInt(&filedes, buff, i, state); */
	/* 		} */
	/* 		if(buff[i] == EOF){ */
	/* 			break; */
	/* 		} */
	/* 	} */
	/* 	if(buff[i] == EOF){ */
	/* 		break; */
	/* 	} */
	/* }  */
	while((nbytes = read(socket_fd, buff, 50)));
	close(socket_fd);
        return 0;
}
