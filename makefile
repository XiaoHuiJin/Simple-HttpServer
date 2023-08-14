CXX = gcc
Obj = server

server: server.c main.c
		$(CXX) -o $(Obj) -g $^ -lpthread

clean:
	rm -rf main