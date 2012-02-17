#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <errno.h>
#include <pthread.h>

#define WORKING 1
#define WAITING 2
#define INTRANSIT 3
#define UP 0
#define DOWN 1

pthread_mutex_t clock_mutex = PTHREAD_MUTEX_INITIALIZER; 
pthread_mutex_t in_mutex = PTHREAD_MUTEX_INITIALIZER; 
pthread_mutex_t out_mutex = PTHREAD_MUTEX_INITIALIZER; 
pthread_mutex_t in_ding_mutex = PTHREAD_MUTEX_INITIALIZER; 
pthread_mutex_t out_ding_mutex = PTHREAD_MUTEX_INITIALIZER; 
pthread_mutex_t elevator_floor_mutex = PTHREAD_MUTEX_INITIALIZER; 


int time_elapsed; //main() initializes to 0. everyone can read. clock can write.
int floor_count; //main() reads this from command-line args
int **requests; //main() must allocate the memory - [2][floor_count]
int *targets;   // main() must allocate the memory - [floor_count]
int passenger_count=0;
int elevator_floor;

int my_random(int mod); //gets a random number between 0 and mod
int read_time(); //protected call to know the time elapsed since start of simulation
int my_clock(int interval); //main ticker
void wait_for_tick(); //waits until the next clock tick.
int client(int work_len); //client thread. one for each client
int elevator(int max_cap); //elevator thread
void send_out_ding(); //lets one person out. blockingg call.
void stop_out_ding(); //lets one person out. blockingg call.
void stop_in_ding(); //lets one person out. blockingg call.
void send_in_ding(); //let one person in. blocks until mutex is released by other thread.
void wait_for_out_ding(); //blocking call
void wait_for_in_ding();  //blocking call

int get_elevator_floor()
{
    int tmp;
    pthread_mutex_lock( &elevator_floor_mutex );
    tmp = elevator_floor;
    pthread_mutex_unlock( &elevator_floor_mutex );
    return time_elapsed;
}

int my_random(int mod)
{
    srand (time(NULL)); //should be done once... to be fixed eventually
    return rand() % mod;
}

//TIME/clock - related
int read_time()
{
    int tmp;
    pthread_mutex_lock( &clock_mutex );
    tmp = time_elapsed;
    pthread_mutex_unlock( &clock_mutex );
    return time_elapsed;
}
int my_clock(int interval)
{
    while(1)
    {
        //increment time_elapsed
        pthread_mutex_lock( &clock_mutex );
        time_elapsed++;
        pthread_mutex_unlock( &clock_mutex );

        //send tick.
        
        //wait 1 clock cycle
        usleep(interval);
    }
}
void wait_for_tick() //waits until the next clock tick.
{
    int start_tick = read_time();
    while(1)
    {
        if ( read_time() > start_tick)
            return;
    }
}

//CLIENT - related

int client(int work_len)
{
    //init
    int state = WORKING;
    int worked_time = my_random(work_len); 
    int target_floor = 0;
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
                press_button(target_floor, curr_floor);
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
                state=WORKING;
            }
        }
        else
        { // dafuq? how did we get here. how about we terminate this faulty person?
            break;
        }
    }

}

int press_button(curr_floor, target_floor)
{
    if (curr_floor < target_floor) //FIXME: protected code
    {
        requests[UP][curr_floor]++;
    }
    else
    {
        requests[DOWN][curr_floor]++;
    }
    //release semaphore
}

int get_inside(curr_floor, target_floor) 
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
}


int get_outside(int floor) //allows one person to leave the elevator
{
    pthread_mutex_lock( &out_mutex );
    passenger_count--;
    targets[floor]--;
    pthread_mutex_unlock( &out_mutex );
    return 0;
}

int elevator(int max_cap)
{
    //init?
    int dir = UP;    
    int curr_floor = 0;
    int max_capacity=max_cap;

    while(1)
    {
        wait_for_tick(); //everything starts with a tick from the clock
        dir = compute_direction(dir, curr_floor); //where we need to go, given curr dir & floor
        if ( dir == UP)
        {
            //we move up
            curr_floor++;
        }
        else if (dir == DOWN)
        {   //am I forgetting something here?
            curr_floor--;
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
            //wait until we know someone got the ding...
            stop_out_ding();
        }

        //let passengers in
        while ( requests[dir/*0=up*/][curr_floor] > 0 && passenger_count < max_capacity )
        {
            send_in_ding();            
        }

    }

}

int out_ding = 0;
int out_ding_ack = 0;

void send_out_ding() //lets one person out. blocking call.
{
    out_ding=1;
    out_ding_ack=0;
    while( out_ding_ack == 0)
        ; //we wait
    return;
}
void stop_out_ding()
{
    out_ding=0;
}
void wait_for_out_ding() //blocking call
{
    while ( out_ding != 1 && out_ding_ack != 0 )
    {
        ;// we wait for an out_ding to be available, and nobody else has taken it
    }
    // a ding has occurred
    
    //FIXME: synchronize 
    int waiting=0;
    pthread_mutex_lock( &out_ding_mutex );
    if (out_ding_ack != 1 ) //if free, we take it
        out_ding_ack=1;
    else 
    {   waiting =1;
    }
    pthread_mutex_unlock( &out_ding_mutex );
    if ( waiting)
        wait_for_out_ding(); //otherwise, we wait some more
    return;
}

int in_ding = 0;
int in_ding_ack = 0;

void send_in_ding() //lets one person in. blocking call.
{
    in_ding=1;
    in_ding_ack=0;
    while( in_ding_ack == 0)
        ; //we wait
    return;
}
void stop_in_ding()
{
    in_ding=0;
}
void wait_for_in_ding() //blocking call
{
    while ( in_ding != 1 && in_ding_ack != 0 )
    {
        ;// we wait for an out_ding to be available, and nobody else has taken it
    }
    // a ding has occurred
    
    //FIXME: synchronize 
    int waiting=0;
    pthread_mutex_lock( &in_ding_mutex );
    if (in_ding_ack != 1 ) //if free, we take it
        in_ding_ack=1;
    else 
    {   waiting =1;
    }
    pthread_mutex_unlock( &in_ding_mutex );
    if ( waiting)
        wait_for_in_ding(); //otherwise, we wait some more
    return;
}


int main()
{
    //init
    time_elapsed=0;
}
