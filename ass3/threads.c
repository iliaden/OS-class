#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <sys/time.h>

#include "mythreads.h"

#define SEMAPHORE_COUNT 100

static List *threadEntries=NULL;
static struct itimerval tval, notval;

static MyThread *executingThread = NULL;
ucontext_t mainContext;
ucontext_t mainContextTmp;

// TODO - use list instead of fixed size array!
static semaphore *semaphores[SEMAPHORE_COUNT];
static int activeSem = 0;
static int inHandler = 0;


static MyThread *next_runnable_thread()
{
  MyThread *entry = NULL;
  
  int maxCnt = list_length(threadEntries);
  int cnt;
  for( cnt=0; cnt < maxCnt; cnt++) {
      entry = list_shift(threadEntries);
      
      switch( entry->state) {
      case Stopped:
	threadEntries=list_append( threadEntries, entry); //FIXME: remove this line to clean-up threads.
  //for now, we leave it to print the time each thread took.
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
 /* if (inHandler == 1 )
  {
    printf ("we're already inside a handler/\n");
    return;
  }
  
  inHandler = 1;*/

  // First, run trough the list and change all the Runnable to Running
  /* Entry will point to the next runnable/running thread */
  MyThread *entry=NULL;

  ucontext_t *currentContext;
  const char *dbgCurrName="main thread";
  

  if( executingThread == NULL) {
    // It's the very beginning, the function was called from
    // runthreads() and should save it's context in the global
    // variable
    currentContext = &mainContext;
  } else {
    // We want to save the current context of the running thread
    currentContext = executingThread->context;
    dbgCurrName = executingThread->name;
  }
  
  if( (entry = next_runnable_thread()) != NULL) {
    if( entry == executingThread) {
      /* printf("%s:%d - hmmmm... executing thread IS current thread... how strange...\n", 
	     __FILE__, __LINE__);
      */
      entry->cpuTime += tval.it_interval.tv_sec*1000000;
      entry->cpuTime += tval.it_interval.tv_usec;
      inHandler = 0;
      return;
    }
    // printf("%s:%d - switch from %s to %s\n", __FILE__, __LINE__, dbgCurrName, entry->name);
//    my_threads_state();

    executingThread = entry;
    dbgCurrName = executingThread->name;


    //we also pre-emptively increment the cpuTime of the executing thread by quantum.
    //Thus one thread will tell that it has used UP TO quantum more time than it actually has,
    //but it will be the exact value at every tick...
    entry->cpuTime += tval.it_interval.tv_sec*1000000;
    entry->cpuTime += tval.it_interval.tv_usec;

    swapcontext( currentContext, entry->context);

    //printf("%s:%d - we' returned from context of thread %s\n", __FILE__, __LINE__, dbgCurrName);
  }

  // printf("%s:%d currentcontext %lx, maincontext %lx, marker %lx\n", __FILE__, __LINE__, (long)currentContext, (long)&mainContext, (long)(&entry));
  inHandler = 0;
  return;
}

int init_my_threads()
{
  threadEntries = list_create(NULL);
  sigset( SIGALRM, timerHandler);
  int ii;
  for (ii=0; ii<SEMAPHORE_COUNT;ii++)
  {
    semaphores[ii] = NULL;
  }
}

/* Sets the value for the timer. Quantum is measured in millisecs */
void set_quantum_size(int quantum)
{
  //fix to avoid getting stuck in the handler.
  if ( quantum < 250 )
    quantum = 250;

  tval.it_interval.tv_sec = quantum/1000000;
  tval.it_interval.tv_usec = (quantum%1000000);

  tval.it_value.tv_sec = tval.it_interval.tv_sec;
  tval.it_value.tv_usec = tval.it_interval.tv_usec;

  notval.it_interval.tv_sec = 0;
  notval.it_interval.tv_usec = 0;

  notval.it_value.tv_sec = 0;
  notval.it_value.tv_usec = 0;
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

  // printf("%s:%d - thread [%s] created, context %lx\n", __FILE__, __LINE__, entry->name, (long)entry->context);
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


  MyThread * tmp; 
  while( (tmp = next_runnable_thread()) != NULL)
  {
    // printf("tmp: [%s], state: [%d]\n",tmp->name, tmp->state);
    
    sleep(1);
  }

//  printf("%s:%d - no runnable threads.\n", __FILE__, __LINE__);

  // Stop the timer
  setitimer( ITIMER_REAL, &notval, 0);


  return;
}

int create_semaphore(int value)
{
  //find next available slot
  int pos =0;

  for (pos = 0; pos < SEMAPHORE_COUNT; pos++)
    if (semaphores[pos] == NULL ) 
      break;
    else
      continue;

  //create semaphore
  semaphore * mysem = malloc(sizeof(semaphore));

  //initialize semaphore thread list.
  mysem->threads = list_create(NULL);

  //initialize semaphore with value
  mysem->value = value;

  //insert semaphore into table
  semaphores[pos] = mysem;

  //increment activve semaphore count
  activeSem++;

  //return its position.
  return pos;
}
void semaphore_signal(int semaphorePos)
{
  //TODO: disable interrupts
  setitimer( ITIMER_REAL, &notval, 0);
  semaphore * mysem = semaphores[semaphorePos];
  //if there is at least one thread waiting, we activate it
  if ( list_length(mysem->threads) > 0 )
  {
    //get thread, change its state. do not reinsert.
    MyThread * entry = list_shift(mysem->threads);
    entry->state = Running;
  }
  //increment value of semaphore.
  mysem->value++;

  //TODO: re-enable interrupts
  setitimer( ITIMER_REAL, &tval, 0);
  return;
}
void semaphore_wait(int semaphorePos)
{
  setitimer( ITIMER_REAL, &notval, 0);
  //TODO: disable interrupts

  MyThread *me = executingThread;

  semaphore *mysem = semaphores[semaphorePos];
  mysem->value --;
  if( mysem->value < 0 ) {
    me->state = Waiting;
    mysem->threads = list_append(mysem->threads, executingThread); 
  }
  // re-enable interrupts
  setitimer( ITIMER_REAL, &tval, 0);

  // Here we have to wait until our state will become "Running" again
  while( me->state != Running)
    usleep(tval.it_interval.tv_sec*1000000 + tval.it_interval.tv_usec);

  return;
}
void destroy_semaphore(int semaphorePos)
{
  return; 
 semaphore * mysem = semaphores[semaphorePos];

  if (list_length(mysem->threads) > 0 )
  {
    printf("Error! Trying to remove a non-empty semaphore");
    return;
  }

  //free the allocated memory
  //first free the list
  list_release(mysem->threads);
  //next free the semaphore
  free(mysem);

  //remove from semaphores
  semaphores[semaphorePos] = NULL;
  //update active semaphore count
  activeSem--;
  return;
}

static char * getStateStr(MyThread * entry)
{
  switch(entry->state)
  {
    case Stopped:
      return "Stopped";
    case Runnable:
      return "Runnable";
    case Running:
      return "Running";
    case Waiting:
      return "Waiting";
    default:
      return "Unknown";
  }
}

/* Prints threads states */
void my_threads_state()
{
  MyThread * entry = NULL;
  //print header
  printf("Name            State     Time\n");

  int maxCnt = list_length(threadEntries);
  int cnt;
  for( cnt=0; cnt < maxCnt; cnt++) {
    entry = list_shift(threadEntries);
    printf( "%-15s %-10s %d\n", entry->name, getStateStr(entry), entry->cpuTime);
    threadEntries = list_append(threadEntries, entry);
  }
  return;
}

void exit_my_thread()
{
  setitimer( ITIMER_REAL, &notval, 0);
  if( executingThread == NULL) {
    printf("ERROR, executingThread is NULL!\n");
  } else
  {
   // printf("\nending thread [%s]\n", executingThread->name);
    executingThread->state = Stopped;
  }
  setitimer( ITIMER_REAL, &tval, 0);
}
