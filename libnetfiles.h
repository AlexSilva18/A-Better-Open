#ifndef LIBNETFILES_H
#define LIBNETFILES_H

#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <string.h>
#include <pthread.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>


#define UNRESTRICTED	0
#define EXCLUSIVE	1
#define TRANSACTION	2

struct addrinfo hints, *res, *r;
struct sockaddr_storage client_addr;
socklen_t client_addr_len;

//returns zero on success, otherwise -1 and h_errno set correctly
int netserverinit (char *hostname, int mode);

// returns new file descriptor otherwise -1 and errno is set appropriately
//to avoid error and disambiguate fd's from the systems fd's, make fd's negative (but not -1)
int netopen(const char *pathname, int flags);

// returns non-negative number of bytes actually read otherwsie, -1 and set errno in the caller's context to indicate the error
ssize_t netread(int fildes, void *buf, size_t nbyte);

// returns number of bytes actually written to fd, otherwise -1 and errno set to indicate the error.
ssize_t netwrite (int fildes, const void *buf, size_t nbyte);

// returns zero on success, otherwise -1 and errno is set appropriately
int netclose (int fd);
  

#endif
