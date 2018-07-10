CC = gcc
FLAGS = -Wall -Werror -g -fsanitize=address -lasan -lpthread -c -o
COMPILE = $(CC) $(FLAGS)
HEADER = libnetfiles.h netfilethreads.h argumentparser.h
SERVOBJ = netfileserver.o netfilethreads.o argumentparser.o
CLIENTOBJ = client.o libnetfiles.o argumentparser.o

default: client server

server: $(SERVOBJ) Makefile $(HEADER)
	$(COMPILE:-c=) netfileserver $(SERVOBJ)

client: $(CLIENTOBJ) Makefile $(HEADER)
	$(COMPILE:-c=) client $(CLIENTOBJ)

libnetfiles.o: libnetfiles.c

%.o: %.c $(HEADER)
	$(COMPILE) $@ $<

clean:
	rm -rf *~ *.o .*.swp "#*.c#" netfileserver client

runserv:
	./netfileserver

runcli:
	./client localhost testplan.txt B unrestricted
