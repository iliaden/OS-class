mtex: threads.o sem-ex.c
	gcc -g threads.o sem-ex.c -lslack -lm -o $@
threads.o: threads.c mythreads.h
	gcc -g -DHAVE_PTHREAD_RWLOCK=1 -c threads.c -o $@