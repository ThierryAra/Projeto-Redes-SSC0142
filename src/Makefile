SERVER_FILE = server.cpp
CLIENT_FILE = client.cpp	

RUN_CLIENT = ../bin/client
RUN_SERVER = ../bin/server 
	
server:
	g++ $(SERVER_FILE) -o $(RUN_SERVER) -w
	./$(RUN_SERVER)
	
client:
	g++ $(CLIENT_FILE) -o $(RUN_CLIENT) -w
	./$(RUN_CLIENT)
	
clear:
	rm -f $(RUN_FILES)
