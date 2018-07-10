#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <string.h>
#include <pthread.h>
#include <netdb.h>
#include <unistd.h>
#include "colors.h"
#include "netfilethreads.h"

// protocol currently expected for open: o [file name len][1 space][file name][1 space][flags]

int main(int argc, char** argv){

	int socket_fd;                               // socket file descriptor
	struct addrinfo hints, *res, *r;
	struct sockaddr_storage client_addr;
	socklen_t client_addr_len;
	int rc, err, connection_id;
	// ssize_t nread;
	char buff[1024];
	struct threadinput * input;
	pthread_t tid;

	if (argc != 1){
	  fprintf(stderr, COL_RED "[ERROR]: Invalid number of arguments.\n");
	  return 0;   
	}

	// generate parameters
	printf(COL_GREEN "Generating Parameters\n" COL_RESET);
	printf(COL_GREEN "Using Parameters " COL_CYAN "AF_INET, INADDR_ANY, 64064\n" COL_RESET);
	
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	
	printf(COL_GREEN "Successfully set parameters.\n" COL_RESET);
	
	//getaddrinfo
	rc = getaddrinfo(NULL, "64064", &hints, &res);
	if (rc != 0) {
	  fprintf(stderr, "Return code %d: %s\n", rc, gai_strerror(rc));
	  return 1;
	}


	//loop to find connections since getaddrinfo() returns a list of address structures
	for (r = res; r != NULL; r = r->ai_next){

	  // connect
	  printf(COL_YELLOW "Attempting to find socket with settings " COL_CYAN "AF_INET, SOCK_STREAM, 0.\n" COL_RESET);
	  socket_fd = socket(r->ai_family, r->ai_socktype, 0); //AF_INET = internet domain, SOCK_STREAM = TCP, 0 = IP protocol

	  if(socket_fd == -1){
	    fprintf(stderr, COL_RED "[ERROR]: Could not find socket.\n");
	    return 0;
	  }
	  printf(COL_GREEN "Successfully found socket, using fd of %d.\n" COL_RESET, socket_fd);

	  // bind
	  printf(COL_YELLOW "Attempting to bind.\n" COL_RESET);
	  if( (bind(socket_fd, r->ai_addr, r->ai_addrlen)) < 0){
	    fprintf(stderr, COL_RED "[ERROR]: Could not bind.\n" COL_RESET);
	    freeaddrinfo(r);
	    close(socket_fd);
	    return 0;
	  }
	  else {
	    printf(COL_GREEN "Successfully bound to socket.\n" COL_RESET);
	    break;
	  }
	  close(socket_fd);
	}
	
	freeaddrinfo(r);
	
	// listen
	printf(COL_YELLOW "Attempting to listen.\n" COL_RESET);
	if ((err = listen(socket_fd, 2) == -1)){
		fprintf(stderr, COL_RED "[ERROR]: Failed to listen. (That deaf socket)\n" COL_RESET);
		return 0;
	}
	printf(COL_GREEN "Success! Now listening.\n" COL_RESET);

	while(1){
		//accept
		printf(COL_YELLOW "Waiting for new connection." COL_RESET "\n");
		client_addr_len = sizeof(struct sockaddr_storage);
		connection_id = accept(socket_fd, (struct sockaddr *) &client_addr, &client_addr_len);
	
		if (connection_id == -1){
		  fprintf(stderr, COL_RED "[ERROR]: Failed to accept)\n" COL_RESET);
		  return 0;
		}
		else{
		  memset(buff, 0, sizeof(buff));
		}

		printf(COL_GREEN "Success! Now connected.\n" COL_RESET);

		read(connection_id, buff, 1);

		input = malloc(sizeof(struct threadinput));
		input->clientaddr = client_addr;
		input->socket_fd = connection_id;

	 	switch(buff[0]){
	 		case 'o':
	 			pthread_create(&tid, NULL, netthreadopen, input);
	 			pthread_detach(tid);
	 			break;
	 		case 'r':
	 			pthread_create(&tid, NULL, netthreadread, input);
	 			pthread_detach(tid);
	 			break;
	 		case 'w':
	 			pthread_create(&tid, NULL, netthreadwrite, input);
	 			pthread_detach(tid);
	 			break;
	 		case 'c':
	 			pthread_create(&tid, NULL, netthreadclose, input);
	 			pthread_detach(tid);
	 			break;
	 		default:
	 			fprintf(stderr, COL_RED "[ERROR]: Invalid Client Packet.\n" COL_RESET);
	 	}
	}
	close(socket_fd); //never get here lol
	return 0; // makes no difference other than happy compiler :)
}
