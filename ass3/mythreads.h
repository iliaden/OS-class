/*
 * =====================================================================================
 *
 *       Filename:  mythreads.h
 *
 *    Description:  Fake MyThreads Library Header File.
 *
 *        Version:  1.0
 *        Created:  03/08/2012 11:54:01 AM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Xing Shi Cai (XSC), xingshi.cai@mail.mcgill.ca
 *   Organization:  McGill Univ.
 *
 * =====================================================================================
 */

#include <ucontext.h>

/* Thread description structure */
typedef struct
{
  char *name;
  enum {
    Runnable,
    Running,
    Waiting,
    Stopped
  } state;

  ucontext_t *context;
  long long cpuTime;
}  MyThread;

int init_my_threads();

/* Creates an entry for the thread in the thread list, initializes the context */
int create_my_thread(char *threadname, void (*threadfunc)(), int stacksize);

/* */
void exit_my_thread();

void runthreads();

/* Sets the value for the timer. Quantum is measured in millisecs */
void set_quantum_size(int quantum);

/* */
int create_semaphore(int value);

/* Changes the thread state to WAITING, adds the thread to the semaphore wait list */
void semaphore_wait(int semaphore);

/* Find the first thread waiting for the semaphore, change it's state to running */
void semaphore_signal(int semaphore);

/* If somebody is waiting for this semaphore, the call fails. Otherwise, it's remoived from the structure */
void destroy_semaphore(int semaphore);

/* Prints threads states */
void my_threads_state();
