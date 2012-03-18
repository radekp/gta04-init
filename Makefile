all: init

init: gta04-init.c
	gcc -static -Wall -o init gta04-init.c

clean:
	rm -f init
