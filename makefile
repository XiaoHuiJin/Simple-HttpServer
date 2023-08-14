CXX = gcc
Obj = server

server: server.c main.c
		$(CXX) -o $(Obj) -g $^

clean:
	rm -rf main