#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>

#define WORKING 1
#define WAITING 2
#define INTRANSIT 3

#define UP 0
#define DOWN 1
#define WAIT 2

#define MAXDEBUG 0
#define AVGDEBUG 1
#define AGGREGATE 2

pthread_mutex_t in_mutex = PTHREAD_MUTEX_INITIALIZER; 
pthread_mutex_t out_mutex = PTHREAD_MUTEX_INITIALIZER; 
pthread_mutex_t in_ding_mutex = PTHREAD_MUTEX_INITIALIZER; 
pthread_mutex_t out_ding_mutex = PTHREAD_MUTEX_INITIALIZER; 
pthread_mutex_t elevator_floor_mutex = PTHREAD_MUTEX_INITIALIZER; 
pthread_mutex_t requests_mutex = PTHREAD_MUTEX_INITIALIZER; 

pthread_cond_t in_ding_waiter = PTHREAD_COND_INITIALIZER;
pthread_cond_t out_ding_waiter = PTHREAD_COND_INITIALIZER;

//clock
pthread_mutex_t clock_mutex = PTHREAD_MUTEX_INITIALIZER; 
pthread_cond_t tick_waiter = PTHREAD_COND_INITIALIZER;
pthread_mutex_t tick_mutex = PTHREAD_MUTEX_INITIALIZER; 


//global static params (unchanged after init
int clients_num; //number of clients in the system
int time_elapsed; //main() initializes to 0. everyone can read. clock can write.
int floor_count; //main() reads this from command-line args
int max_work_time; //global variable that determines the maximum amount of time a worker can work
int max_capacity; //global variable indicating max numbeer of people that can be in the elevator any any given moment
int debug_lvl = MAXDEBUG;

//changing global variables
int passenger_count=0; //how many people are on the elevator
int elevator_floor; //current floor where the elevator is
int people_transported=0; //total number of passengers transported since start of simulation
int people_100 = 0; //number of passengers transported in the past 100 ticks.
int *targets;   // main() must allocate the memory - [floor_count]
int *passengers; //array of size passengers. 1 means passenger is on elevator; 0 means he's not
int **requests; //main() must allocate the memory - [2][floor_count]

//threads
void *my_clock(void* interval); //main ticker
void *client(void* work_len); //client thread. one for each client
void *elevator(void* max_cap); //elevator thread

int my_random(int mod); //gets a random number between 0 and mod
void wait_for_tick(); //waits until the next clock tick.
void send_out_ding(); //lets one person out. blocking call.
void send_in_ding(); //let one person in. blocks until mutex is released by other thread.
void wait_for_out_ding(); //waits until the elevator allows one person to exit; attempts to grab semaphore
int wait_for_in_ding(int curr_floor, int target_floor, int worker_id);  //waits until the elevator allows one person to enter; attempts to grab semaphore
void press_button(int curr_floor, int target_floor, int client_id); //press the right button (up/down)
int get_inside(int curr_floor, int target_floor); //person enters elevator
int get_outside(int floor); //person exits elevator
int read_requests(int dir, int floor); //tells if there are any requests to go in direction DIR from floor FLOOR.
void print_passengers(); //prints the list of passengers who are on board the elevator; if it is empty, does nothing

void print_passengers() //print passengers who are on plane
{
    int ii, start=0; //start means that there was at least one passenger found on elevator.
    for (ii = 0; ii < clients_num; ii++)
    {
        if (!start && passengers[ii] )
        {
            printf ("List of passengers on board: %d",ii);
            start=1;
        }
        else if ( passengers[ii] )
            printf(", %d", ii);
    } 
    if (start)
        printf(".\n");
}

int get_elevator_floor() //thread-safe call to know where the elevator currently is. 
{
    int tmp;
    pthread_mutex_lock( &elevator_floor_mutex );
        tmp = elevator_floor;
    pthread_mutex_unlock( &elevator_floor_mutex );
    return tmp;
}

int my_random(int mod) //returns a random value between 0 and mod.  
{
    return rand() % mod;
}

/*  clock thread. every INTERVAL (passed as argument), it does the following:
 *  1) counts the amount of ticks elapsed since the start of the simulation.
 *  2) if needed, prints current tick count, and the list of passengers on board. If needed, prints the aggregated info.
 *  3) notifies everyone of a time tick
 *  4) sleeps for INTERVAL, then repeats this.
 * */
void * my_clock(void * arg) 
{
    int interval = (int)arg;
    while(1)
    {
        //increment time_elapsed
        time_elapsed++;

        if ( (debug_lvl == MAXDEBUG ) || (debug_lvl == AVGDEBUG ) )
            printf("\n\nStarting tick %d\n", time_elapsed);
        if ( debug_lvl == MAXDEBUG )
            print_passengers();
        
        else if (debug_lvl == AGGREGATE && time_elapsed%100 == 0)
        {
            printf("People transported in the last 100 ticks: %d\n",people_100);
            printf("People transported since start of simulation (%d ticks): %d\n",time_elapsed, people_transported);
            pthread_mutex_lock( &out_mutex );
                people_100=0;
            pthread_mutex_unlock( &out_mutex );
        }
            
        //send tick.
        pthread_cond_broadcast( &tick_waiter);        
        //wait 1 clock cycle
        usleep(interval);
    }
}

void wait_for_tick() //waits until the next clock tick. blocking call.
{
    pthread_cond_wait( &tick_waiter, &tick_mutex);
    pthread_mutex_unlock(&tick_mutex);
}

/*  Client thread.
 *  At every tick:
 *      if client is working:
 *          work.
 *          if work shift is over, do:
 *              compute new floor where he wants to go.
 *              pust appropriate button
 *      if client is waiting for elevator:
 *          if the elevator is at his floor, and allows people inside:
 *              try to get in (done via mutex; only one person allowed to enter at a single time - necessary for max passenger count constraint)
 *      if client is inside elevator:
 *          if elevator is at the target floor, and it is letting people out:
 *              try to leave the elevator (again, mutex needed to keep passenger count stable)
 *  */
void *client(void * arg )
{
    //init
    int work_len = 1+my_random(max_work_time-1); //how long this client must work for until changing floors.
    int worker_id = (int)arg; //client's ID. must be unique, and the count must start at 0.
    int state = WORKING; //client's state. everyone starts WORKING so that they don't all rush at the same time.
    int worked_time = my_random(work_len); //amount of time elapsed since start of work 
    int target_floor = my_random(floor_count); //where this clients wants to go
    int curr_floor = my_random(floor_count); // we start at a random floor

    //main loop
    while(1)
    {
        //this line ensures all the operations happen only once per tick.
        wait_for_tick(); 

        if (state == WORKING)
        {
            worked_time++;
            if ( worked_time > work_len ) 
            {  
                while ( target_floor == curr_floor)
                    target_floor = my_random(floor_count); //avoids going to current floor

                press_button(curr_floor, target_floor,worker_id);
                state = WAITING;                
            }
        }
        else if ( state == WAITING)
        {
            if ( wait_for_in_ding(curr_floor, target_floor, worker_id) == 0) //operation succeeded
                state = INTRANSIT;
            pthread_mutex_unlock(&in_ding_mutex);
            /*  if there are many clients waiting for dings:
             *      if this client responds to the ding
             *          if the elevator it at the right spot- perfect, we enter.
             *      otherwise
             *          this client sees the elevator is at the wrong floor
             *          he does nothing
             *          loop goes to wait_for_tick (this client won't do anything else this tick)
             *          elevator sends another ding because there are still requests to enter and space available
             *
             *  this solves many problems with synchronization or getting stuck in infinite loops.
             */
            
        }
        else if ( state == INTRANSIT)
        {
            wait_for_out_ding(); // a single out ding happens at each clock cycle
            curr_floor=get_elevator_floor(); //so we know where everyone is at all time. BIG BROTHER IS WATCHING YOU
            if (curr_floor == target_floor)
            {
                get_outside(target_floor);
                if ( (debug_lvl == MAXDEBUG ) || (debug_lvl == AVGDEBUG ) )
                    printf("Client %d left the elevator at floor %d and will now work for %d ticks.\n",worker_id,curr_floor,work_len);
                passengers[worker_id] = 0;
                state=WORKING;
                worked_time = 0;
                work_len = 1+my_random(max_work_time-1);
            }
        }
        else
        { // dafuq? how did we get here. how about we terminate this faulty person?
            break;
        }
    }

    return NULL;
}

//function that returns the number of requests in a given direction from a given floor in a thread-safe manner.
int read_requests(int dir, int floor)
{
    int tmp;
    pthread_mutex_lock(&requests_mutex);
        tmp = requests[dir][floor];
    pthread_mutex_unlock(&requests_mutex);
    return tmp;
}

//generates a request for the elevator from a given floor and in an appropriate direction.
//It also prints logs at the most detailed level.
void press_button(int curr_floor, int target_floor, int client_id)
{
    if (curr_floor < target_floor) 
    {
        pthread_mutex_lock(&requests_mutex);
            requests[UP][curr_floor]++;
            if ( (debug_lvl == MAXDEBUG ) )//|| (debug_lvl == AVGDEBUG ) )
                printf("Client %d pressed button UP at floor %d, hoping to get to floor %d.\n",client_id,curr_floor,target_floor);
        pthread_mutex_unlock(&requests_mutex);
    }
    else
    {
        pthread_mutex_lock(&requests_mutex);
            requests[DOWN][curr_floor]++;
            if ( (debug_lvl == MAXDEBUG ) )//|| (debug_lvl == AVGDEBUG ) )
                printf("Client %d pressed button DOWN at floor %d, hoping to get to floor %d.\n",client_id,curr_floor,target_floor);
        pthread_mutex_unlock(&requests_mutex);
    }
}

//function that allows for one person to enter the elevator. It also updates all the necessary global variables.
int get_inside(int curr_floor, int target_floor) 
{
    if ( passenger_count < max_capacity)
    {
        if (curr_floor < target_floor) 
        {
            pthread_mutex_lock(&requests_mutex);
                requests[UP][curr_floor]--;
            pthread_mutex_unlock(&requests_mutex);
        }
        else
        {
            pthread_mutex_lock(&requests_mutex);
                requests[DOWN][curr_floor]--;
            pthread_mutex_unlock(&requests_mutex);
        }
        targets[target_floor]++;
        passenger_count++;
        return 0;
    }
    return -1;
}


int get_outside(int floor) //allows one person to leave the elevator
{
    pthread_mutex_lock( &out_mutex );
        passenger_count--;
        targets[floor]--;
        people_transported++;
        people_100++;
    pthread_mutex_unlock( &out_mutex );
    return 0;
}

int compute_direction(int dir, int curr_floor)
{
    int ii;
    if (curr_floor == 0)
        return UP;
    if (curr_floor == floor_count)
        return DOWN;
    if (dir == UP)
    {
        //we make sure we're not at the top floor
        if (curr_floor == floor_count)
            return DOWN;
        //we check that someone INSIDE the elevator wants to get higher
        for ( ii=curr_floor+1 ; ii < floor_count; ii++)
        {// semaphore here?
            if (targets[ii] > 0 )
                return UP;
        }
        //we check that there are requests from above
        for ( ii=curr_floor+1 ; ii < floor_count; ii++)
        {
            if ( (read_requests(UP, ii) > 0 ) || read_requests(DOWN,ii) > 0 )
                return UP;
        }
        return DOWN;
        //option to add the idea of not moving if there are no requests...
    }
    else if (dir == DOWN)
    {
        //we make sure we're not at the bottom floor
        if (curr_floor == 0)
            return UP;
        //we check that someone INSIDE the elevator wants to get lower
        for ( ii=curr_floor-1 ; ii >= 0 ; ii--)
        {//semaphore here?
            if (targets[ii] > 0 )
                return DOWN;
        }
        //we check that there are requests from above
        for ( ii=curr_floor-1 ; ii >= 0 ; ii--)
        {
            if ( (read_requests(UP, ii) > 0 ) || read_requests(DOWN,ii) > 0 )
                return DOWN;
        }
        return UP;
        //option to add the idea of not moving if there are no requests...
    }
    return -1;
}

void *elevator(void * arg)
{
    //init?
    int dir = UP;    
    int curr_floor = 0;

    while(1)
    {
        wait_for_tick(); //everything starts with a tick from the clock
        dir = compute_direction(dir, curr_floor); //where we need to go, given curr dir & floor
        if ( dir == UP)
        {
            //we move up
            curr_floor++;
            if ( (debug_lvl == MAXDEBUG ) )//|| (debug_lvl == AVGDEBUG ) )
                printf("elevator moved UP to floor %d\n", curr_floor);
        }
        else if (dir == DOWN)
        {   //am I forgetting something here?
            curr_floor--;
            if ( (debug_lvl == MAXDEBUG ) )//|| (debug_lvl == AVGDEBUG ) )
                printf("elevator moved DOWN to floor %d\n", curr_floor);
        }
        else 
        {
            if ( (debug_lvl == MAXDEBUG ) )//|| (debug_lvl == AVGDEBUG ) )
                printf("elevator stayed on floor %d\n", curr_floor);
            //no need to move
            continue;
        }

        pthread_mutex_lock( &elevator_floor_mutex );
        elevator_floor=curr_floor;
        pthread_mutex_unlock( &elevator_floor_mutex );
        
        //let passengers out
        while (targets[curr_floor] > 0 )
            send_out_ding();

        //let passengers in
        while ( ((read_requests(dir, curr_floor) > 0) || ((read_requests(1-dir, 0) > 0) && curr_floor==0) || ((read_requests(1-dir, floor_count-1) > 0) && curr_floor==floor_count-1) ) && passenger_count < max_capacity )
            send_in_ding();            
    }
}

void send_out_ding() //lets one person out. blocking call.
{
    pthread_cond_signal(&out_ding_waiter);
}
void wait_for_out_ding() //blocking call
{
    pthread_mutex_lock(&out_ding_mutex);    
    pthread_cond_wait(&out_ding_waiter, &out_ding_mutex);
    pthread_mutex_unlock(&out_ding_mutex);
}
void send_in_ding() //lets one person in. blocking call.
{
    pthread_mutex_lock(&in_ding_mutex);    
    pthread_cond_signal(&in_ding_waiter);
    pthread_mutex_unlock(&in_ding_mutex);
}
int wait_for_in_ding(int curr_floor, int target_floor, int worker_id) //blocking call
{
    pthread_mutex_lock(&in_ding_mutex);    
    pthread_cond_wait(&in_ding_waiter, &in_ding_mutex);
    if ( get_elevator_floor() == curr_floor ) //this following operations must be atomic, and thread-safe.
    {
        //get inside...quick!
        //or at least try
        if (get_inside(curr_floor, target_floor) == 0 ) //operation succeeded. and that is a blocking operation through mutex
        {
            if ( (debug_lvl == MAXDEBUG ) || (debug_lvl == AVGDEBUG ) )
                printf("Client %d entered the elevator at floor %d heading toward floor %d\n",worker_id,curr_floor,target_floor); 
            passengers[worker_id] = 1;
            return 0;
        }
    }
    return 1;
}

int main(int argc, char * argv[])
{
    //init
    srand (time(NULL)); //should be done once... to be fixed eventually
    time_elapsed=0;
    clients_num=50;
    max_capacity = 2147483647;
    int tick_interval = 1000;
    max_work_time=30;
    int ii; 
    floor_count=50;
    elevator_floor=0;

    //parse input
    //format:   -n number of floors
    //          -p number of people
    //          -m max_capacity
    //          -w max_work_len
    //          -t number of miliseconds per tick
    //          -vvv  //max details (everything)
    //          -vv //medium: who is entering/exiting
    //          -v //100-ticks aggregations
    for ( ii = 1; ii < argc; ii++)
    {
        if ( strcmp(argv[ii], "-v" ) == 0 )
            debug_lvl = AGGREGATE;    
        else if ( strcmp(argv[ii], "-vv" ) == 0 )
            debug_lvl = AVGDEBUG;    
        else if ( strcmp(argv[ii], "-vvv" ) == 0 )
            debug_lvl = MAXDEBUG;    
        else if ( (strcmp(argv[ii],"-h" ) == 0 ) || (strcmp(argv[ii],"--help" ) ==  0) )
        {
            printf("usage: PROGNAME \n\t-n number of floors [default %d]\n\t-p number of people [default %d]\n\t-m max capacity [default %d]\n\t-t number of milliseconds per tick [default %d]\n\t-w maximum amount of time a worker will be working on his task (in ticks) [default %d]\n\t-v [prints 100-tick aggregation data]\n\t-vv [prints only who enters and leaves each tick]\n\t-vvv [prints ALL the events that occur per tick] [default]\n\t-h or --help [prints this message]\n", floor_count, clients_num, max_capacity, tick_interval/1000, max_work_time);
            exit(0);
        }
        else if ( strcmp(argv[ii], "-p" ) == 0 )
        {
            if (ii == argc-1)
            {
                printf("please specify the number of people present");
                exit (-1);
            }
            clients_num = atoi(argv[++ii]);
        } 
        else if ( strcmp(argv[ii], "-n" ) == 0 )
        {
            if (ii == argc-1)
            {
                printf("please specify the number of floors in the building");
                exit (-1);
            }
            floor_count = atoi(argv[++ii]);
        } 
        else if ( strcmp(argv[ii], "-m" ) == 0 )
        {
            if (ii == argc-1)
            {
                printf("please specify the maximum capacity of the elevator");
                exit (-1);
            }
            max_capacity = atoi(argv[++ii]);
        } 
        else if ( strcmp(argv[ii], "-w" ) == 0 )
        {
            if (ii == argc-1)
            {
                printf("please specify the maximum amount of time a worker can work before changing floors");
                exit (-1);
            }
            max_work_time = atoi(argv[++ii]);
        } 
        else if ( strcmp(argv[ii], "-t" ) == 0 )
        {
            if (ii == argc-1)
            {
                printf("please specify the number of miliseconds between ticks");
                exit (-1);
            }
            tick_interval = 1000*atoi(argv[++ii]);
        } 
    } 
    
    requests = (int **)malloc(sizeof(int* ) * 2); //main() must allocate the memory - [2][floor_count]
    requests[0] = (int *)malloc(sizeof(int)*floor_count);
    requests[1] = (int *)malloc(sizeof(int)*floor_count);
    targets = (int *) malloc(sizeof(int)*floor_count);   // main() must allocate the memory - [floor_count]
    passengers = (int *) malloc(sizeof(int) * clients_num);
    for (ii=0 ; ii < floor_count ; ii++)
    {
        requests[UP][ii]=0;
        requests[DOWN][ii]=0;
        targets[ii]=0;
//        printf("executed: %d\n",ii);
    }
    
    pthread_t elevator_thread, clock_thread, clients_threads[clients_num];
    //init clock & elevator
    pthread_create(&elevator_thread, NULL, elevator, (void *)max_capacity);
    pthread_create(&clock_thread, NULL, my_clock, (void *)(tick_interval));

    wait_for_tick();

    //init  clients 
    int rc;
    for (ii = 0; ii < clients_num;ii++)
    {
        passengers[ii]=0; //initializing the variable here to save an extra loop
        rc = pthread_create(&clients_threads[ii], NULL, &client, (void*)ii); //or is it just 'client'?
        if (rc){
            printf("ERROR; return code from pthread_create() is %d\n", rc);
            exit(-1);
        }
    }
    
    pthread_exit(NULL);
    return 0;
}
