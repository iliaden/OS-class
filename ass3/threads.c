#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <sys/time.h>

#include <slack/list.h>
#include "mythreads.h"

static List *threadEntries=NULL;
static struct itimerval tval;

static MyThread *executingThread = NULL;
ucontext_t mainContext;

static MyThread *next_runnable_thread()
{
  MyThread *entry = NULL;
  
  int maxCnt = list_length(threadEntries);
  int cnt;
  for( cnt=0; cnt < maxCnt; cnt++) {
      entry = list_shift(threadEntries);
      
      switch( entry->state) {
      case Stopped:
	continue;
      case Waiting:
	threadEntries=list_append( threadEntries, entry);
	continue;
      case Runnable:
	entry->state = Running;
      case Running:
	threadEntries=list_append( threadEntries, entry);
	
	return entry;
      default:
	printf("ERROR at %s:%d\n", __FILE__, __LINE__);
	break;
      }
    }
  return NULL;
}

static void timerHandler(int sig)
{
  extern const char *handlerName;

  // First, run trough the list and change all the Runnable to Running
  /* Entry will point to the next runnable/running thread */
  MyThread *entry=NULL;

  ucontext_t *currentContext;
  const char *dbgCurrName="main thread";

  if( sig == 0) {
    printf("%s:%d - called from runThreads\n", __FILE__, __LINE__);
  } else {
    printf("%s:%d - called from timer\n", __FILE__, __LINE__);
  }

  if( executingThread == NULL) {
    // It's the very beginning, the function was called from
    // runthreads() and should save it's context in the global
    // variable
    //    getcontext( &mainContext);
    currentContext = &mainContext;
  } else {
    // We want to save the current context of the running thread
    // getcontext( executingThread->context);
    currentContext = executingThread->context;
    dbgCurrName = executingThread->name;
  }
  
  if( (entry = next_runnable_thread()) != NULL) {
    printf("%s:%d - switch from %s to %s\n", __FILE__, __LINE__, dbgCurrName, entry->name);

    executingThread = entry;
    dbgCurrName = executingThread->name;

    handlerName = entry->name;
    swapcontext( currentContext, entry->context);

    printf("%s:%d - we' returned from context of thread %s\n", __FILE__, __LINE__, dbgCurrName);
  }

  printf("%s:%d currentcontext %lx, maincontext %lx\n", __FILE__, __LINE__, (long)currentContext, (long)&mainContext);
  swapcontext( currentContext, &mainContext);

  printf("%s:%d\n", __FILE__, __LINE__);
  return;
}

int init_my_threads()
{
  threadEntries = list_create(NULL);
  sigset( SIGALRM, timerHandler);
}

/* Sets the value for the timer. Quantum is measured in millisecs */
void set_quantum_size(int quantum)
{
  tval.it_interval.tv_sec = quantum/1000;
  tval.it_interval.tv_usec = 1000*(quantum%1000);

  tval.it_value.tv_sec = tval.it_interval.tv_sec;
  tval.it_value.tv_usec = tval.it_interval.tv_usec;
}

/* Creates an entry for the thread in the thread list, initializes the context */
int create_my_thread(char *threadname, void (*threadfunc)(), int stacksize)
{
  MyThread *entry = (MyThread *)malloc( sizeof(MyThread));

  entry->name = strdup( threadname);
  entry->state = Runnable;
  entry->cpuTime = 0;

  entry->context = (ucontext_t *)malloc( sizeof(ucontext_t));

  // Get the current execution context
  getcontext( entry->context);

  // Modify the context to a new stack
  entry->context->uc_link = &mainContext;
  entry->context->uc_stack.ss_sp = malloc(stacksize );
  entry->context->uc_stack.ss_size = stacksize;
  entry->context->uc_stack.ss_flags = 0;        
  if ( entry->context->uc_stack.ss_sp == 0 ) {
    perror( "malloc: Could not allocate stack" );
    return(-1);
  }

  // Create the new context
  makecontext( entry->context, threadfunc, 0 );

  threadEntries = list_append( threadEntries, entry);

  printf("%s:%d - thread [%s] created, context %lx\n", __FILE__, __LINE__, entry->name, (long)entry->context);
  return list_length( threadEntries);
}

void runthreads()
{
  // First of all, check if we have any threads to run.
  if( list_length( threadEntries) == 0)
    return;

  // Now, set the timer
  setitimer( ITIMER_REAL, &tval, 0);

  // Now we just want to wait until the handler will finish running in
  // circles serving threads. To avoid synchronization problems, we'll
  // just call the handler directly. When the handler will have
  // nothing more to do (all threads served), it will return.

  printf("%s:%d - before calling handler, list has %d entries\n",
	 __FILE__, __LINE__, list_length( threadEntries));

  timerHandler(0);

  while( next_runnable_thread() != NULL)
    sleep(1);
  return;
}

int create_semaphore(int value)
{
  return 0;
}
void semaphore_signal(int semaphore)
{
  return;
}
void semaphore_wait(int semaphore)
{
  return;
}
void destroy_semaphore(int semaphore)
{
  return;
}

/* Prints threads states */
void my_threads_state()
{
  return;
}

void exit_my_thread()
{
  if( executingThread == NULL) {
    printf("ERROR, executingThread is NULL!\n");
  } else
    executingThread->state = Stopped;
}
