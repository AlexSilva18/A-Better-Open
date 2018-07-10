#ifndef NETTHREADS
#define NETTHREADS

#include <sys/socket.h>

#define BUFFSIZE 50

enum clientmode{
	UNRESTRICTED,
	EXCLUSIVE,
	TRANSACTION
};

struct threadinput{
	struct sockaddr_storage clientaddr;
	int socket_fd;
};

struct clientfile{
	int fd;
	enum clientmode mode;
	struct clientfile * next;
};

struct filemutex{
	char * filename;
	struct clientfile * fds;
	pthread_mutex_t editmutex;
	int readLock;
	int numReading;
	int writeLock;
	int numWriting;
	struct filemutex * next;
};

void * netthreadopen(void *);
void * netthreadread(void *);
void * netthreadwrite(void *);
void * netthreadclose(void *);

#endif
