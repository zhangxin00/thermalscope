all:
	gcc kaslr.c -O0 -lpthread -lm -o kaslr
clean:
	rm -f kaslr

