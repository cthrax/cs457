CC=gcc
CFLAGS=-c -Wall
LDFLAGS=-I include/a1/
CLIENT_SOURCES=src/client/main.c src/client/tcp_client.c
CLIENT_OBJECTS=$(CLIENT_SOURCES:.c=.o)
CLIENT_EXECUTABLE=proj1_client 
SERVER_SOURCES=src/server/main.c src/server/udp_server.c src/server/tcp_server.c src/server/server_common.c
SERVER_OBJECTS=$(SERVER_SOURCES:.c=.o)
SERVER_EXECUTABLE=proj1_server

all: $(CLIENT_SOURCES) $(CLIENT_EXECUTABLE) $(SERVER_SOURCES) $(SERVER_EXECUTABLE)
	
$(CLIENT_EXECUTABLE): $(CLIENT_OBJECTS) 
	$(CC) $(LDFLAGS) $(CLIENT_OBJECTS) -o $@
	
$(SERVER_EXECUTABLE): $(SERVER_OBJECTS) 
	$(CC) $(LDFLAGS) $(SERVER_OBJECTS) -o $@

.c.o:
	$(CC) $(LDFLAGS) $(CFLAGS) $< -o $@

clean:
	rm -rf $(SERVER_OBJECTS) $(CLIENT_OBJECTS) $(SERVER_EXECUTABLE) $(CLIENT_EXECUTABLE)
