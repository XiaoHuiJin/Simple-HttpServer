CC = gcc
CFLAGS = -Wall -std=gnu99

TARGET = SERVER
SRCS = main.c \
	   ./error/error_print.c \
	   ./server/server.c	\
	   ./http/http_response.c

INC = -I./error -I./server -I./http

OBJS = $(SRCS:.c=.o)

$(TARGET):$(OBJS)
#	@echo TARGET:$@
#	@echo OBJECTS:$^
	$(CC) -o $@ -g $^ -lpthread

clean:
	rm -rf $(TARGET) $(OBJS)
 
%.o:%.c
	$(CC) $(CFLAGS) $(INC) -o $@ -c $<