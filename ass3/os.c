#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <slack/list.h>

struct semaphore
{
    char* name;
    int value;
    List * threads;
};

struct my_thread
{
    char * name;
    int id;
    
}


struct itimerval tval;
sigset(SIGALARM, handler);

set_quantum_size(int quantum)
{
    tval.it_interval.tv_sec = quantum/1000000;
    tval.it_interval.tv_usec= quantum%1000000;
    tval.it_value.tv_sec = quantum/1000000;
    tval.it_value.tv_usec= quantum%1000000;
    //restart timer?
}
//quantum must be set at runthreads()

