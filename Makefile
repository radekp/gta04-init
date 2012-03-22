all: init

runinitlib.o: runinitlib.c
	klcc -c runinitlib.c

init: runinitlib.o gta04-init.c
	klcc -static -Wall -o init gta04-init.c runinitlib.o

clean:
	rm -f init
