# Makefile for Client and Server with shared encryption

# Compiler
CC = g++

# Compiler Flags
CFLAGS = -std=c++17 -Wall

# Common source files for both client and server
COMMON_SRC = encryption.cpp
COMMON_OBJ = encryption.o

# Client-specific source files
CLIENT_SRC = client.cpp
CLIENT_OBJ = client.o
CLIENT_OUT = client

# Server-specific source files
SERVER_SRC = server.cpp
SERVER_OBJ = server.o
SERVER_OUT = server

# Build both client and server
all: $(CLIENT_OUT) $(SERVER_OUT)

# Compile the client
$(CLIENT_OUT): $(CLIENT_OBJ) $(COMMON_OBJ)
	$(CC) $(CFLAGS) -o $(CLIENT_OUT) $(CLIENT_OBJ) $(COMMON_OBJ)

# Compile the server
$(SERVER_OUT): $(SERVER_OBJ) $(COMMON_OBJ)
	$(CC) $(CFLAGS) -o $(SERVER_OUT) $(SERVER_OBJ) $(COMMON_OBJ)

# Compile client object
$(CLIENT_OBJ): $(CLIENT_SRC)
	$(CC) $(CFLAGS) -c $(CLIENT_SRC)

# Compile server object
$(SERVER_OBJ): $(SERVER_SRC)
	$(CC) $(CFLAGS) -c $(SERVER_SRC)

# Compile common encryption object
$(COMMON_OBJ): $(COMMON_SRC)
	$(CC) $(CFLAGS) -c $(COMMON_SRC)

# Clean command to remove compiled files
clean:
	rm -f $(CLIENT_OBJ) $(SERVER_OBJ) $(COMMON_OBJ) $(CLIENT_OUT) $(SERVER_OUT)

# Phony target to avoid conflicts with file names
.PHONY: all clean
