CC = gcc
EXEC = clean ss awget

all: $(EXEC)

clean: 
	rm -f *.o *.pgm $(EXEC)

awget: awget.c
	$(CC) -o $@ awget.c

ss: ss.c
	$(CC) -o $@ ss.c
