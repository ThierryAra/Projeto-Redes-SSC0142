SERVER_FILE = server.cpp
CLIENT_FILE = client.cpp	

RUN_CLIENT = ../bin/client
RUN_SERVER = ../bin/server 
	
all:
	RUN_SERVER RUN_CLIENT	
	
server:
	g++ $(SERVER_FILE) -o $(RUN_SERVER)
	./$(RUN_SERVER)
	
client:
	g++ $(CLIENT_FILE) -o $(RUN_CLIENT)
	./$(RUN_CLIENT)

clear:
	rm -f $(RUN_FILES)
