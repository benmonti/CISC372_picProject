
image:image.c image.h
	gcc -g image.c -o image -lm
pthreads_image:pthreads_image.c pthreads_image.h
	gcc -g pthreads_image.c -o pthreads_image -lm -lpthread
openMP_image:openMP_image.c openMP_image.h
	gcc -g openMP_image.c -o openMP_image -lm -fopenmp
clean:
	rm -f image pthreads_image openMP_image output.png
