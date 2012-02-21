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

#define MAXDEBUG 0
#define AVGDEBUG 1
#define AGGREGATE 2

pthread_mutex_t in_mutex = PTHREAD_MUTEX_INITIALIZER; 
pthread_mutex_t out_mutex = PTHREAD_MUTEX_INITIALIZER; 
pthread_mutex_t in_ding_mutex = PTHREAD_MUTEX_INITIALIZER; 
pthread_mutex_t out_ding_mutex = PTHREAD_MUTEX_INITIALIZER; 
pthread_mutex_t elevator_floor_mutex = PTHREAD_MUTEX_INITIALIZER; 

pthread_cond_t in_ding_waiter = PTHREAD_COND_INITIALIZER;
pthread_cond_t out_ding_waiter = PTHREAD_COND_INITIALIZER;

//clock
pthread_mutex_t clock_mutex = PTHREAD_MUTEX_INITIALIZER; 
pthread_cond_t tick_waiter = PTHREAD_COND_INITIALIZER;
pthread_mutex_t tick_mutex = PTHREAD_MUTEX_INITIALIZER; 



int time_elapsed; //main() initializes to 0. everyone can read. clock can write.
int floor_count; //main() reads this from command-line args
int **requests; //main() must allocate the memory - [2][floor_count]
int *targets;   // main() must allocate the memory - [floor_count]
int passenger_count=0;
int elevator_floor;
int debugLvl = MAXDEBUG;
int max_work_time;

void *my_clock(void* interval); //main ticker
void *client(void* work_len); //client thread. one for each client
void *elevator(void* max_cap); //elevator thread

int my_random(int mod); //gets a random number between 0 and mod
int read_time(); //protected call to know the time elapsed since start of simulation
void wait_for_tick(); //waits until the next clock tick.
void send_out_ding(); //lets one person out. blockingg call.
void send_in_ding(); //let one person in. blocks until mutex is released by other thread.
void wait_for_out_ding(); //blocking call
void wait_for_in_ding();  //blocking call
void press_button(int curr_floor, int target_floor, int client_id); //press the right button (up/down)
int get_inside(int curr_floor, int target_floor); //allows one person to enter elevator
int get_outside(int floor); //allows one person to leave the elevator

int get_elevator_floor()
{
    int tmp;
    pthread_mutex_lock( &elevator_floor_mutex );
    tmp = elevator_floor;
    pthread_mutex_unlock( &elevator_floor_mutex );
    return tmp;
}

int my_random(int mod)
{
    return rand() % mod;
}

//TIME/clock - related
int read_time()
{
/*    int tmp;
    pthread_mutex_lock( &clock_mutex );
    tmp = time_elapsed;
    pthread_mutex_unlock( &clock_mutex );
*/    return time_elapsed;
}
void * my_clock(void * arg)
{
    int interval = (int)arg;
    while(1)
    {
        //increment time_elapsed
        time_elapsed++;

        printf("\n\nStarting tick %d\n", time_elapsed);
        //send tick.
        pthread_cond_broadcast( &tick_waiter);        
        //wait 1 clock cycle
        usleep(interval);
    }
}
void wait_for_tick() //waits until the next clock tick.
{
    pthread_cond_wait( &tick_waiter, &tick_mutex);
    pthread_mutex_unlock(&tick_mutex);
}

//CLIENT - related

void *client(void * arg )
{
    int work_len = 1+my_random(max_work_time-1);
    int worker_id = (int)arg;
    //init
    int state = WORKING;
    int worked_time = my_random(work_len); 
    int target_floor = my_random(floor_count);
    int curr_floor = my_random(floor_count); // we start at a random floor

    //main loop
    while(1)
    {
        //this line ensures all the operations happen only once per tick.
        wait_for_tick(); //must be a blocking call.

        if (state == WORKING)
        {
            worked_time++;
            if ( worked_time > work_len ) //TODO: verify the necessity of an equality constraint here.
            {  
                while ( target_floor == curr_floor)
                    target_floor = my_random(floor_count); //avoids going to current floor
                press_button(curr_floor, target_floor,worker_id);
                state = WAITING;                
            }
        }
        else if ( state == WAITING)
        {
            wait_for_in_ding(); // a single in ding happens at each clock cycle
            
            if ( get_elevator_floor() == curr_floor )
            {
                //get inside...quick!
                //or at least try
                if (get_inside(curr_floor, target_floor) == 0 ) //operation succeeded. and that is a blocking operation through mutex
                {
                    printf("Client %d enetered the elevator at floor %d heading toward floor %d\n",worker_id,curr_floor,target_floor);
                    state = INTRANSIT;
                }
            }
        }
        else if ( state == INTRANSIT)
        {
            wait_for_out_ding(); // a single out ding happens at each clock cycle
            curr_floor=get_elevator_floor(); //so we know where everyone is at all time. BIG BROTHER IS WATCHING YOU
            if (curr_floor == target_floor)
            {
                get_outside(target_floor);
                printf("Client %d left the elevator at floor %d and will now word for %d ticks.\n",worker_id,curr_floor,work_len);
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

void press_button(int curr_floor, int target_floor, int client_id)
{
    if (curr_floor < target_floor) //FIXME: protected code
    {
        requests[UP][curr_floor]++;
        printf("Client %d pressed button UP at floor %d, hoping to get to floor %d.\n",client_id,curr_floor,target_floor);
    }
    else
    {
        requests[DOWN][curr_floor]++;
        printf("Client %d pressed button DOWN at floor %d, hoping to get to floor %d.\n",client_id,curr_floor,target_floor);
    }
    //release semaphore
}

int get_inside(int curr_floor, int target_floor) 
{
    if (curr_floor < target_floor) //FIXME: protected code
    {
        requests[UP][curr_floor]--;
    }
    else
    {
        requests[DOWN][curr_floor]--;
    }
    targets[target_floor]++;
    passenger_count++;
    //FIXME: release semaphore
    return 0;
}


int get_outside(int floor) //allows one person to leave the elevator
{
    pthread_mutex_lock( &out_mutex );
    passenger_count--;
    targets[floor]--;
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
        {//semaphore here?
            if (requests[UP][ii] > 0 )
                return UP;
            else if (requests[DOWN][ii] > 0)
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
        {//semaphore here?
            if (requests[UP][ii] > 0 )
                return DOWN;
            else if (requests[DOWN][ii] > 0)
                return DOWN;
        }
        return UP;
        //option to add the idea of not moving if there are no requests...
    }
    return -1;
}

void *elevator(void * arg)
{
    int max_capacity = (int)arg;
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
            printf("elevator moved UP to floor %d\n", ++curr_floor);
        }
        else if (dir == DOWN)
        {   //am I forgetting something here?
            printf("elevator moved DOWN to floor %d\n", --curr_floor);
        }
        else 
        {
            //no need to move
            continue;
        }

        pthread_mutex_lock( &elevator_floor_mutex );
        elevator_floor=curr_floor;
        pthread_mutex_unlock( &elevator_floor_mutex );
        
        //let passengers out
        while (targets[curr_floor] > 0 )
        {
            send_out_ding();
        }

        //let passengers in
        while ( ((requests[dir/*0=up*/][curr_floor] > 0) || ((requests[1-dir][0] > 0) && curr_floor==0) || ((requests[1-dir][floor_count] > 0) && curr_floor==floor_count) ) && passenger_count < max_capacity )
        {
            send_in_ding();            
        }
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
void wait_for_in_ding() //blocking call
{
    pthread_mutex_lock(&in_ding_mutex);    
    pthread_cond_wait(&in_ding_waiter, &in_ding_mutex);
    pthread_mutex_unlock(&in_ding_mutex);
}

int main(int argc, char * argv[])
{
    //init
    srand (time(NULL)); //should be done once... to be fixed eventually
    time_elapsed=0;
    int clients_num=50;
    int max_capacity = 2147483647;
    int tick_interval = 100000;
    max_work_time=30;
    int ii; 
    floor_count=50;
    elevator_floor=0;

    //parse input
    //
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
            debugLvl = AGGREGATE;    
        else if ( strcmp(argv[ii], "-vv" ) == 0 )
            debugLvl = AVGDEBUG;    
        else if ( strcmp(argv[ii], "-vvv" ) == 0 )
            debugLvl = MAXDEBUG;    
        else if ( (strcmp(argv[ii],"-h" ) == 0 ) || (strcmp(argv[ii],"--help" )) )
        {
            printf("usage: PROGNAME \n\t-n number of floors [default 50]\n\t-p number of people [default 5]\n\t-m max capacity [default 2147483647]\n\t-t number of milliseconds per tick [default 1000]\n\t-w maximum amount of time a worker will be working on his task (in ticks) [default 50]\n\t-v [prints 100-tick aggregation data]\n\t-vv [prints only who enters and leaves each tick]\n\t-vvv [prints ALL the events that occur per tick] [default]\n\t-h or --help [prints this message]\n");
            exit(0);
        }
        else if ( strcmp(argv[ii], "-p" ) == 0 )
        {
            if (ii == argc-1)
            {
                printf("please specify the number of people present");
                exit (-1);
            }
            max_capacity = atoi(argv[++ii]);
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
            max_work_time = 1000*atoi(argv[++ii]);
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
        rc = pthread_create(&clients_threads[ii], NULL, &client, (void*)ii); //or is it just 'client'?
        if (rc){
            printf("ERROR; return code from pthread_create() is %d\n", rc);
            exit(-1);
        }
    }
    
    pthread_exit(NULL);
    return 0;
}
