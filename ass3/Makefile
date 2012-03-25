CFLAGS=-I..
mtex: threads.o sem-ex.c
	gcc -g ${CFLAGS} threads.o sem-ex.c -DHAVE_PTHREAD_RWLOCK=1 -L../slack -lslack -lm -o $@
threads.o: threads.c mythreads.h
	gcc -g  ${CFLAGS} -DHAVE_PTHREAD_RWLOCK=1 -c threads.c -o $@

clean:
	-rm -f *.o mtex