#include "libnetfiles.h"
#include "colors.h"

int main(int argc, char *argv[]){

	if (argc != 5){
	  fprintf(stderr, COL_RED "[ERROR]: Invalid number of arguments.\n" COL_RESET);
	  return 0;   
	}
	char *pathname = argv[2];
	int flags;
	int mode;

	switch (argv[3][0]){
		case 'R':
		  flags = O_RDONLY;
		  break;
		case 'W':
		  flags = O_WRONLY;
		  break;
		case 'B':
		  flags = O_RDWR;
		  break;
		default:
		  printf("Error setting flag.\n");
		  return 0;
	}

	switch(argv[4][0]){
		case 'u':
			mode = UNRESTRICTED;
			break;
		case 'e':
			mode = EXCLUSIVE;
			break;
		case 't':
			mode = TRANSACTION;
			break;
		default:
			printf("Error setting mode.\n");
			return 0;
	}
	
	// generate parameters
	printf(COL_GREEN "Generating Parameters\n" COL_RESET);
	printf(COL_GREEN "Using Parameters " COL_BLUE "AF_INET, INADDR_ANY, 64064\n" COL_RESET);

	if (netserverinit(argv[1], mode) < 0){
	    fprintf(stderr, COL_RED "[ERROR]: Unable to initialize connection\n" COL_RESET);
	    return 0;
	  }
	
	int fd = netopen(pathname, flags);
	if (fd == -1){
	  fprintf(stderr, COL_RED "[ERROR]: Unable to obtain file descriptor\n" COL_RESET);
	  return 0;
	}
	printf("Got file descriptor of %d from netopen\n", fd);
	char buff[501];
	int bytes_read;
	bytes_read = netread(fd, buff, 50);
	printf("Number of bytes read: %d, and read of %s\n", bytes_read, buff);
	int bytes_written;
	bytes_written = netwrite(fd, buff, bytes_read);
	printf("Number of bytes written: %d, and wrote %s\n", bytes_written, buff);
	
	if(netclose(fd) == -1){
	  fprintf(stderr, COL_RED "[ERROR]: Unable to close file\n" COL_RESET);
	    return 0;
	}
	printf("Client operations successful\n");

	return 0;
}

