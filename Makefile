TARGET=sut

all: request_uri_parser.c
	gcc -o $(TARGET) fastercgi.c request_uri_parser.c sut.c -g -lcassandra -luv -lstdc++

request_uri_parser.c: request_uri_parser.rl
	ragel request_uri_parser.rl

clean:
	rm -f *.sock *.o $(TARGET)
