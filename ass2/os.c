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


int time_elapsed; //main() initializes to 0. everyone can read. clock can write.
int floor_count; //main() reads this from command-line args
int **requests; //main() must allocate the memory - [2][floor_count]
int *targets;   // main() must allocate the memory - [floor_count]

int random(int mod); //gets a random number between 0 and mod
int read_time(); //protected call to know the time elapsed since start of simulation
int clock(int interval); //main ticker
void wait_for_tick(); //waits until the next clock tick.
int client(int work_len); //client thread. one for each client
int elevator(int max_cap); //elevator thread
int send_out_ding(); //lets one person out. blockingg call.
int send_in_ding(); //let one person in. blocks until mutex is released by other thread.
int wait_for_out_ding(); //blocking call
int wait_for_in_ding();  //blocking call


int random(int mod)
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
int clock(int interval)
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
    int worked_time = random(work_len); 
    int target_floor = 0;
    int curr_floor = random(floor_count); // we start at a random floor

    //main loop
    while(1)
    {
        if (state == WORKING)
        {
            wait_for_tick(); //must be a blocking call.
            worked_time++;
            if ( worked_time > work_len ) //TODO: verify the necessity of an equality constraint here.
            {  
                while ( target_floor == curr_floor)
                    target_floor = random(floor_count); //avoids going to current floor
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
                if (get_inside(target_floor) == 1 ) //operation succeeded. and that is a blocking operation through mutex
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


int elevator(int max_cap)
{
    //init?
    int dir = UP;    
    int curr_floor = 0;
    int passenger_count=0;
    int max_capacity=max_cap;

    while(1)
    {
        wait_for_tick(); //everything starts with a tick from the clock
        dir = compute_direction(dir, floor); //where we need to go, given curr dir & floor
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
        
        //let passengers out
        while (targets[curr_floor] > 0 )
        {
            send_out_ding();
        }

        //let passengers in
        while ( requests[dir/*0=up*/][curr_floor] > 0 && passenger_count < max_capacity )
        {
            send_in_ding();            
        }

    }

}

int send_out_ding(); //lets one person out. blockingg call.
int send_in_ding(); //let one person in. blocks until mutex is released by other thread.
int wait_for_out_ding(); //blocking call
int wait_for_in_ding();  //blocking call

int main()
{
    //init
    time_elapsed=0;
}
