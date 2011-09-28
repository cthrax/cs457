CC = gcc
EXEC = clean ss awget

all: $(EXEC)

clean: 
	rm -f *.o *.pgm $(EXEC)

awget:
	$(CC) -o $@ src/awget.c

ss:
	$(CC) -o $@ src/ss.c
