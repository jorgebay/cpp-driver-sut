TARGET=sut

all:
	gcc -o $(TARGET) fastercgi.c sut.c -g -lcassandra -luv -lstdc++

clean:
	rm -f *.sock *.o $(TARGET)
