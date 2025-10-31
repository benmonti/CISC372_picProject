
image:image.c image.h
	gcc -g image.c -o image -lm
pthreads_image:pthreads_image.c pthreads_image.h
	gcc -g pthreads_image.c -o pthreads_image -lm -lpthread
clean:
	rm -f image pthreads_image output.png