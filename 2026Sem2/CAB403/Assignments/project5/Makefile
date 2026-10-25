all: car controller call internal safety
car: car.c
	gcc -Wall -g -o car car.c -lpthread -lrt
controller: controller.c
	gcc -Wall -o controller controller.c -lpthread
call: call.c
	gcc -Wall -o call call.c
internal: internal.c
	gcc -Wall -o internal internal.c -lrt
safety: safety.c
	gcc -Wall -o safety safety.c -lrt
clean:
	rm -f car controller call internal safety