SERVER_FILE = server.cpp
CLIENT_FILE = client.cpp

RUN_CLIENT = ../bin/client
RUN_SERVER = ../bin/server

compile_server:
	g++ $(SERVER_FILE) -o $(RUN_SERVER) -w

compile_client:
	g++ $(CLIENT_FILE) -o $(RUN_CLIENT) -w

all: compile_server compile_client

server:
	$(RUN_SERVER)

client:
	$(RUN_CLIENT)


clear:
	rm -f $(RUN_FILES)
