/*
 * =====================================================================================
 *
 *       Filename:  sem-ex.c
 *
 *    Description:  Example of testing code of MyThread.
 *
 *        Version:  1.0
 *        Created:  03/08/2012 11:09:44 AM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Xing Shi Cai
 * =====================================================================================
 */
/* Includes */
#include <stdio.h>      /* Input/Output */
#include <stdlib.h>     /* General Utilities */
#include <math.h>

#include "mythreads.h"

/* prototype for thread routine */
void handler ( );

/* global vars */

/* semaphores are declared global so they can be accessed 
   in main() and in thread routine,
   here, the semaphore is used as a mutex */

int counter_mutex;

/* shared variables */
int counter; 
double result = 0.0;

int main()
{
    int thread_num = 10;
    int j;
    char* thread_names[] = {
        "thread 0",
        "thread 1",
        "thread 2",
        "thread 3",
        "thread 4",
        "thread 5",
        "thread 6",
        "thread 7",
        "thread 8",
        "thread 9"
    };

    /* Initialize MyThreads library. */
    init_my_threads();

    /* 250 ms */
    set_quantum_size(250);

    counter_mutex = create_semaphore(1);

    for(j=0; j<thread_num; j++)
    {
        create_my_thread(thread_names[j], (void *) &handler, 64000);
    }

    /* Print threads informations before run */
    my_threads_state();

    /* When this function returns, all threads should have exited. */
    runthreads();
    
    destroy_semaphore(counter_mutex);

    /* Print threads informations after run */
    my_threads_state();

    printf("The counter is %d\n", counter);
    printf("The result is %f\n", result);

    exit(0);
}

const char *handlerName="";

void handler ()
{
  printf("%s:%d - %s, name [%s]\n", __FILE__, __LINE__, __FUNCTION__, handlerName);
    int i;
    for(i=0; i < 5; i++)
    {
        /* If you remove this protection, you should be able to see different
         * out of every time you run this program.  With this protection, you
         * should always be able to see result to be 151402.656521 */
        semaphore_wait(counter_mutex);       /* down semaphore */

        /* START CRITICAL REGION */
	/*
        int j;
        for (j = 0; j < 1000; j++) {
            result = result + sin(counter) * tan(counter);
        }
	*/
	result += counter;
        counter++;
	// sleep(1);

        /* END CRITICAL REGION */    

        semaphore_signal(counter_mutex);       /* up semaphore */
    }
    
    exit_my_thread(); /* exit thread */
}
