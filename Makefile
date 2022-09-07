
#NAME: CAO XU
#EMAIL: cxcharlie@gmail.com
#ID: 704551688

default: client server

client: lab1b-client.c
	gcc -g -Wall -Wextra -lz lab1b-client.c -o lab1b-client

server: lab1b-server.c
	gcc -g -Wall -Wextra -lz lab1b-server.c -o lab1b-server

clean:
	rm -f lab1b-client lab1b-server lab1b-704551688.tar.gz

dist: lab1b-client.c lab1b-server.c Makefile README
	tar -czvf lab1b-704551688.tar.gz lab1b-client.c lab1b-server.c Makefile README




