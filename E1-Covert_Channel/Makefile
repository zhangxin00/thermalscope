all:
	gcc receiver-irq.c -Os -lpthread -lm -o irq
	gcc receiver-throttle.c -Os -lpthread -lm -o throttle
	gcc sender.c -Os -lpthread -lm -o sender
	gcc error-rate.c -Os -lpthread -lm -o err
clean:
	rm -f irq throttle sender err

