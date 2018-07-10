#include "argumentparser.h"
#include <stdio.h>
#include <ctype.h>
#include <string.h>


int parseInt(int * currVal, char * buff, int i, int state){
	if(isdigit(buff[i])){
		*currVal = *currVal * 10 + (buff[i] - '0');
		return state;
	}
	return state += 1;
}

int parseString(int socket_fd, char * dest, char * buff, int nbytes, int len, int * i, int * destindex, int state){

	int j = *i;
	while(j < *i + len - *destindex && j < nbytes && buff[j] != EOF){
		j++;
	}
	strncpy(dest + *destindex, buff + *i, j - *i);
	dest[*destindex + j - *i] = '\0';
	if(j < *i + len - *destindex){
		// we did not finish doing our thing
		*destindex = j - *i;
		*i = j;
	}
	else{
		*i = j;
		return ++state;
	}
	return state;
}
