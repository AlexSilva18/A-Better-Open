#ifndef ARG_PARSE_H
#define ARG_PARSE_H

int parseInt(int * curr_val, char * buff, int i, int state);
int parseString(int socket_fd, char * dest, char * buff, int nbytes, int len, int * i, int * bufindex, int state);

#endif
