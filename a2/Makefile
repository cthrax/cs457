CC = gcc
EXEC = clean ss awget

all: $(EXEC)

clean: 
	rm -f *.o *.pgm $(EXEC)

awget:
	$(CC) -o $@ awget.c

ss:
	$(CC) -o $@ ss.c
