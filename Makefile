all: init

init: gta04-init.c
	klcc -static -Wall -o init gta04-init.c

clean:
	rm -f init
