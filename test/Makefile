#################################### OPTIONS ##################################

CFLAGS = -O4 -Wall
CC = gcc

#################################### TARGETS ##################################
TARGETS = getjpeg

all: $(TARGETS)

clean:
	rm -f *.o *~ core *.i $(TARGETS)

#################################### RULES ####################################

getjpeg: getjpeg.c
	$(CC) $(CFLAGS) -o getjpeg getjpeg.c

