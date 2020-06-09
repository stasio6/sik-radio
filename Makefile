CFLAGS = -O2 -Wall -Wextra -std=gnu11
CPPFLAGS = -O2 -Wall -Wextra -std=c++11

.PHONY: clean

all: radio-proxy radio-client

radio-proxy: err.o parser.o socket_manager.o my_time.o network.o radio-proxy.o
	g++ -o radio-proxy err.o parser.o socket_manager.o my_time.o network.o radio-proxy.o

radio-client: err.o parser.o socket_manager.o my_time.o network.o radio-client.o
	g++ -o radio-client err.o parser.o socket_manager.o my_time.o network.o radio-client.o

err.o: err.c err.h
	gcc $(CFLAGS) -c err.c

parser.o: parser.cpp parser.h
	g++ $(CPPFLAGS) -c parser.cpp

socket_manager.o: socket_manager.cpp socket_manager.h err.h
	g++ $(CPPFLAGS) -c socket_manager.cpp

my_time.o: my_time.cpp my_time.h
	g++ $(CPPFLAGS) -c my_time.cpp

network.o: network.cpp network.h err.h
	g++ $(CPPFLAGS) -c network.cpp

radio-proxy.o: radio-proxy.cpp err.h parser.h socket_manager.h my_time.h network.h
	g++ $(CPPFLAGS) -c radio-proxy.cpp

radio-client.o: radio-client.cpp err.h parser.h socket_manager.h my_time.h network.h
	g++ $(CPPFLAGS) -c radio-client.cpp

clean:
	rm -f *.o radio-proxy radio-client