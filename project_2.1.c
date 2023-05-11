#include "queue.c"
#include <pthread.h>
#include <sys/time.h>
#include <string.h>
#include <unistd.h>

int simulationTime = 120;    // simulation time
int seed = 10;               // seed for randomness
int emergencyFrequency = 30; // frequency of emergency gift requests from New Zealand

void *ElfA(void *arg); // the one that can paint
void *ElfB(void *arg); // the one that can assemble
void *Santa(void *arg);
void *ControlThread(void *arg); // handles printing and queues (up to you)

// pthread sleeper function
int pthread_sleep(int seconds)
{
    pthread_mutex_t mutex;
    pthread_cond_t conditionvar;
    struct timespec timetoexpire;
    if (pthread_mutex_init(&mutex, NULL))
    {
        return -1;
    }
    if (pthread_cond_init(&conditionvar, NULL))
    {
        return -1;
    }
    struct timeval tp;
    // When to expire is an absolute time, so get the current time and add it to our delay time
    gettimeofday(&tp, NULL);
    timetoexpire.tv_sec = tp.tv_sec + seconds;
    timetoexpire.tv_nsec = tp.tv_usec * 1000;

    pthread_mutex_lock(&mutex);
    int res = pthread_cond_timedwait(&conditionvar, &mutex, &timetoexpire);
    pthread_mutex_unlock(&mutex);
    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&conditionvar);

    // Upon successful completion, a value of zero shall be returned
    return res;
}
struct thread_data
{
    int thread_id;
    int sum;
    char *message;
};

int taskID = 1;

// Queues
Queue *packageQueue;
Queue *deliveryQueue;
Queue *paintQueue;
Queue *assambleQueue;
Queue *qAQueue;

//condition variable
pthread_mutex_t qAQueueSize_mutex;
pthread_cond_t qAQueueSize_cv;

// mutexes
pthread_mutex_t taskIDMutex;
pthread_mutex_t packageMutex;
pthread_mutex_t deliveryMutex;
pthread_mutex_t paintMutex;
pthread_mutex_t assambleMutex;
pthread_mutex_t qAMutex;
pthread_mutex_t jobsMutex;

int keepGoing = 1;

//to check QA and paint/assabmle tasks are completed
int jobs[1000];

struct timespec finishTime;
struct timespec startTime;

FILE *simulationResult;

int main(int argc, char **argv)
{
    // -t (int) => simulation time in seconds
    // -s (int) => change the random seed
    for (int i = 1; i < argc; i++)
    {
        if (!strcmp(argv[i], "-t"))
        {
            simulationTime = atoi(argv[++i]);
        }
        else if (!strcmp(argv[i], "-s"))
        {
            seed = atoi(argv[++i]);
        }
    }

    if (pthread_mutex_init(&packageMutex, NULL) 
    && pthread_mutex_init(&deliveryMutex, NULL) 
    && pthread_mutex_init(&taskIDMutex, NULL)
    && pthread_mutex_init(&paintMutex, NULL)
    && pthread_mutex_init(&assambleMutex, NULL)
    && pthread_mutex_init(&qAMutex, NULL)
    && pthread_mutex_init(&jobsMutex, NULL))
    {
        printf("\nInitiliazing mutex error.\n");
        return 1;
    }

    // keep track of task ID
    //  int taskID = 1;

    packageQueue = ConstructQueue(1000);
    deliveryQueue = ConstructQueue(1000);
    paintQueue = ConstructQueue(1000);
    assambleQueue = ConstructQueue(1000);
    qAQueue = ConstructQueue(1000);

    struct thread_data data;
    data.sum = 5;

    pthread_t threads[4];
    int rc;
    // long t;
    rc = pthread_create(&threads[0], NULL, ControlThread, NULL);
    rc = pthread_create(&threads[1], NULL, ElfA, NULL);
    rc = pthread_create(&threads[2], NULL, ElfB, NULL);
    rc = pthread_create(&threads[3], NULL, Santa, NULL);
    

    for (int i = 0; i < 4; i++)
    {
        int *r;
        if (pthread_join(threads[i], (void **)&r) != 0)
        {
            perror("Failed to join thread");
        }

        // free(r);
    }
    pthread_mutex_destroy(&packageMutex);
    pthread_mutex_destroy(&deliveryMutex);
    pthread_mutex_destroy(&taskIDMutex);
    pthread_mutex_destroy(&paintMutex);
    pthread_mutex_destroy(&assambleMutex);
    pthread_mutex_destroy(&qAMutex);
    pthread_mutex_destroy(&qAQueueSize_mutex);
    pthread_mutex_destroy(&jobsMutex);
    pthread_cond_destroy(&qAQueueSize_cv);
    

    return 0;
}

void *ElfA(void *arg)
{

    struct timeval currentTime;

    while (keepGoing)
    {
        while (packageQueue->size != 0)
        {
            Task a;
            pthread_mutex_lock(&packageMutex);
            if (packageQueue->size != 0)
            {
                a = Dequeue(packageQueue);
                // break;
            }
            pthread_mutex_unlock(&packageMutex);
            // pthread_mutex_lock(&packageMutex);
            // printf("id: %d\n", a.ID);
            pthread_sleep(1);
            gettimeofday(&currentTime, NULL);
            a.turnAround = currentTime.tv_sec - a.taskArrival;
            a.type = 'C';

            fprintf(simulationResult,
                    "%d             %d          %d              %c           %d         %d         %d              %c\n", a.ID, a.giftID, a.giftType, a.type, a.requestTime, a.taskArrival, a.turnAround, 'A');

            // deliver to santa
            a.ID = taskID;
            pthread_mutex_lock(&taskIDMutex);
            taskID++;
            pthread_mutex_unlock(&taskIDMutex);
            a.type = 'D';
            gettimeofday(&currentTime, NULL);
            a.taskArrival = currentTime.tv_sec;
            a.responsable = 'S';
            pthread_mutex_lock(&deliveryMutex);
            Enqueue(deliveryQueue, a);
            pthread_mutex_unlock(&deliveryMutex);

            //checking whether the simulation time is over
            if(currentTime.tv_sec >= finishTime.tv_sec) {
                exit(0);
            }
        }
        // pthread_mutex_unlock(&packageMutex);

        while (paintQueue->size != 0)
        {
            if(packageQueue->size != 0){
                ElfA(NULL);
            }
            pthread_mutex_lock(&paintMutex);
            Task a = Dequeue(paintQueue);
            Task *ptr = &a;
            a.type = 'P';
            pthread_mutex_unlock(&paintMutex);
            pthread_sleep(3);

            gettimeofday(&currentTime, NULL);
            a.turnAround = currentTime.tv_sec - a.taskArrival;
            
            pthread_mutex_lock(&jobsMutex);
            jobs[a.giftID]++;
            pthread_mutex_unlock(&jobsMutex);

            fprintf(simulationResult,
                    "%d             %d          %d              %c           %d         %d         %d              %c\n", a.ID, a.giftID, a.giftType, a.type, a.requestTime, a.taskArrival, a.turnAround, 'A');

            // deliver to package
            a.ID = taskID;
            pthread_mutex_lock(&taskIDMutex);
            taskID++;
            pthread_mutex_unlock(&taskIDMutex);

            a.type = 'C';
            gettimeofday(&currentTime, NULL);
            a.taskArrival = currentTime.tv_sec;
            pthread_mutex_lock(&packageMutex);
            if(a.giftType ==2 || jobs[a.giftID] == 2)
            {
                //printf("Gift type: %d\n",ptr->giftType);
                Enqueue(packageQueue, a);
                pthread_mutex_unlock(&packageMutex);
            }
            // else if(ptr->isQA == 1)
            // {
            //     printf("Helloworld2\n");
            //     Enqueue(packageQueue, a);
            //     pthread_mutex_unlock(&packageMutex);
            // }
            pthread_mutex_unlock(&packageMutex);

            //checking whether the simulation time is over
            if(currentTime.tv_sec >= finishTime.tv_sec) {
                exit(0);
            }
        }
    }
    pthread_exit(0);
}

void *ElfB(void *arg)
{
    // printf("Inside of ElfB\n");

    struct timeval currentTime;

    while (keepGoing)
    {
        while (packageQueue->size != 0)

        {
            Task a;
            pthread_mutex_lock(&packageMutex);
            if (packageQueue->size != 0)
            {
                a = Dequeue(packageQueue);
                // break;
            }
            pthread_mutex_unlock(&packageMutex);
            // printf("id: %d\n", a.ID);
            pthread_sleep(1);
            gettimeofday(&currentTime, NULL);
            a.turnAround = currentTime.tv_sec - a.taskArrival;
            a.type = 'C';

            fprintf(simulationResult,
                    "%d             %d          %d              %c           %d         %d         %d              %c\n", a.ID, a.giftID, a.giftType, a.type, a.requestTime, a.taskArrival, a.turnAround, 'B');

            // deliver to santa
            a.ID = taskID;
            pthread_mutex_lock(&taskIDMutex);
            taskID++;
            pthread_mutex_unlock(&taskIDMutex);
            a.type = 'D';
            gettimeofday(&currentTime, NULL);
            a.taskArrival = currentTime.tv_sec;
            a.responsable = 'S';
            pthread_mutex_lock(&deliveryMutex);
            Enqueue(deliveryQueue, a);
            pthread_mutex_unlock(&deliveryMutex);

            //checking whether the simulation time is over
            if(currentTime.tv_sec >= finishTime.tv_sec) {
                exit(0);
            }
        }

        // pthread_mutex_unlock(&packageMutex);

        while (assambleQueue->size != 0)
        {
            if(packageQueue->size != 0){
                ElfB(NULL);
            }
            pthread_mutex_lock(&assambleMutex);
            Task a = Dequeue(assambleQueue);
            pthread_mutex_unlock(&assambleMutex);
            pthread_sleep(2);

            // assemble is done
            pthread_mutex_lock(&jobsMutex);
            jobs[a.giftID]++;
            pthread_mutex_unlock(&jobsMutex);

            gettimeofday(&currentTime, NULL);
            a.turnAround = currentTime.tv_sec - a.taskArrival;

            fprintf(simulationResult,
                    "%d             %d          %d              %c           %d         %d         %d              %c\n", a.ID, a.giftID, a.giftType, a.type, a.requestTime, a.taskArrival, a.turnAround, 'B');

            // deliver to package
            a.ID = taskID;
            pthread_mutex_lock(&taskIDMutex);
            taskID++;
            pthread_mutex_unlock(&taskIDMutex);

            a.type = 'C';
            gettimeofday(&currentTime, NULL);
            a.taskArrival = currentTime.tv_sec;
            if (a.giftType == 3 || jobs[a.giftID] == 2) 
            {
                // printf("Gift type: %d\n", a.giftType);
                pthread_mutex_lock(&packageMutex);
                Enqueue(packageQueue, a);
                pthread_mutex_unlock(&packageMutex);
            }

            if(currentTime.tv_sec >= finishTime.tv_sec) {
                exit(0);
            }
            
        }
    }
    pthread_exit(0);
}

// manages Santa's tasks
void *Santa(void *arg)
{

    struct timeval currentTime;

    while (keepGoing)
    {
        while (deliveryQueue->size >= 1)
        {
            
            pthread_mutex_lock(&deliveryMutex);
            Task a = Dequeue(deliveryQueue);
            pthread_mutex_unlock(&deliveryMutex);
            // printf("santa id: %d\n", a.ID);
            pthread_sleep(1);
            gettimeofday(&currentTime, NULL);
            a.turnAround = currentTime.tv_sec - a.taskArrival;
            a.type = 'D';
            fprintf(simulationResult,
                    "%d             %d          %d              %c           %d         %d         %d              %c\n", a.ID, a.giftID, a.giftType, a.type, a.requestTime, a.taskArrival, a.turnAround, 'S');
            if(currentTime.tv_sec >= finishTime.tv_sec) {
                exit(0);
            }
        }

        while(qAQueue->size >= 1) {
            
            //handle QA
            pthread_mutex_lock(&qAMutex);
            
            Task a = Dequeue(qAQueue);
     
            //Task *ptr = &a;
            a.type = 'Q';
            pthread_mutex_unlock(&qAMutex);
            pthread_sleep(1);
            gettimeofday(&currentTime, NULL);
            a.turnAround = currentTime.tv_sec - a.taskArrival;

            pthread_mutex_lock(&jobsMutex);
            jobs[a.giftID]++;
            pthread_mutex_unlock(&jobsMutex);

            fprintf(simulationResult,
                    "%d             %d          %d              %c           %d         %d         %d              %c\n", a.ID, a.giftID, a.giftType, a.type, a.requestTime, a.taskArrival, a.turnAround, 'S');
            //printf("isPaint: %d\n",ptr->isPainted);

            if (jobs[a.giftID] == 2) //check whether painting/assemble is done
            {
                pthread_mutex_lock(&packageMutex);
                a.taskArrival = currentTime.tv_sec;
                Enqueue(packageQueue,a);
                pthread_mutex_unlock(&packageMutex);
            }
            
            if(deliveryQueue->size != 0){
              Santa(NULL);
            }

            if(currentTime.tv_sec >= finishTime.tv_sec) {
                exit(0);
            }    
        }
    }
    pthread_exit(0);
}

// the function that controls queues and output
void *ControlThread(void *arg)
{
    // printf("HEREt\n");

    // FILE *simulationResult;
    simulationResult = fopen("./simulationResultPart1.log", "w");
    fprintf(simulationResult,
            "TaskID     GiftID     GiftType      TaskType      RequestTime        TaskArrival     TT      Responsable\n");
    fprintf(simulationResult, "____________________________________________________________________________________________________________\n");

    struct timeval currentTimeReal;
    gettimeofday(&currentTimeReal, NULL);
    
    startTime.tv_sec = currentTimeReal.tv_sec;
    finishTime.tv_sec = currentTimeReal.tv_sec + simulationTime;

    // random variable to decide gift types
    int ran1;
    int ran2 = 0;

    int giftID = 1;

    keepGoing = 1;
    int count = 0;
    // pthread_sleep(1);
    // until reach the execution time:
    while (finishTime.tv_sec != currentTimeReal.tv_sec)
    {

        srand(seed + ran2); // feed the seed
        ran1 = rand() % 20;
        // // printf("Rand: %d\n",ran1);

        if (ran1 == 0 || ran1 == 1) //no gift:(
        {
            printf("Bad kid! :-()\n");
        }
         if (ran1 >= 2 && ran1 <= 9) //gift type 1
        {
            // printf("Okay kid\n");
            Task t;
            t.ID = taskID;
            pthread_mutex_lock(&taskIDMutex);
            taskID++;
            pthread_mutex_unlock(&taskIDMutex);
            t.type = 'C';
            t.giftID = giftID;
            giftID++;
            t.giftType = 1;
            gettimeofday(&currentTimeReal, NULL);
            t.requestTime = currentTimeReal.tv_sec;
            t.taskArrival = currentTimeReal.tv_sec;

            pthread_mutex_lock(&packageMutex);
            Enqueue(packageQueue, t);
            pthread_mutex_unlock(&packageMutex);
            // pthread_sleep(1);

            // printf("Size: %d\n", packageQueue->size);
        }
        else if (ran1 > 9 && ran1 <= 13) //gift type 2
        {
            Task t;
            t.ID = taskID;
            pthread_mutex_lock(&taskIDMutex);
            taskID++;
            pthread_mutex_unlock(&taskIDMutex);
            t.type = 'P';
            t.giftID = giftID;
            giftID++;
            t.giftType = 2;
            gettimeofday(&currentTimeReal, NULL);
            t.requestTime = currentTimeReal.tv_sec;
            t.taskArrival = currentTimeReal.tv_sec;
            pthread_mutex_lock(&paintMutex);
            Enqueue(paintQueue, t);
            pthread_mutex_unlock(&paintMutex);
        }
        else if (ran1 > 13 && ran1 <= 17) //gift type 3
        {
            Task t;
            t.ID = taskID;
            pthread_mutex_lock(&taskIDMutex);
            taskID++;
            pthread_mutex_unlock(&taskIDMutex);
            t.type = 'A';
            t.giftID = giftID;
            giftID++;
            t.giftType = 3;
            gettimeofday(&currentTimeReal, NULL);
            t.requestTime = currentTimeReal.tv_sec;
            t.taskArrival = currentTimeReal.tv_sec;
            pthread_mutex_lock(&assambleMutex);
            Enqueue(assambleQueue, t);
            pthread_mutex_unlock(&assambleMutex);
        }
        else if (ran1 == 18) //gift type 4
        {
            // printf("Gift type 4:\n");
            Task t;
            t.ID = taskID;
            pthread_mutex_lock(&taskIDMutex);
            taskID++;
            pthread_mutex_unlock(&taskIDMutex);
            t.giftID = giftID;
            giftID++;
            t.giftType = 4;
            gettimeofday(&currentTimeReal, NULL);
            t.requestTime = currentTimeReal.tv_sec;
            t.taskArrival = currentTimeReal.tv_sec;

            pthread_mutex_lock(&paintMutex);
            Enqueue(paintQueue, t);
            pthread_mutex_unlock(&paintMutex);
            
            //increment the taskID
            t.ID = taskID;
            pthread_mutex_lock(&taskIDMutex);
            taskID++;
            pthread_mutex_unlock(&taskIDMutex);
            
            pthread_mutex_lock(&qAMutex);
            Enqueue(qAQueue, t);
            pthread_mutex_unlock(&qAMutex);

        }
        else if (ran1 == 19) //gift type 5
        {
            Task t;
            t.ID = taskID;
            pthread_mutex_lock(&taskIDMutex);
            taskID++;
            pthread_mutex_unlock(&taskIDMutex);
            t.giftID = giftID;
            giftID++;
            t.giftType = 5;
            gettimeofday(&currentTimeReal, NULL);
            t.requestTime = currentTimeReal.tv_sec;
            t.taskArrival = currentTimeReal.tv_sec;

            pthread_mutex_lock(&assambleMutex);
            Enqueue(assambleQueue, t);
            pthread_mutex_unlock(&assambleMutex);

            //increment the taskID
            t.ID = taskID;
            pthread_mutex_lock(&taskIDMutex);
            taskID++;
            pthread_mutex_unlock(&taskIDMutex);

            pthread_mutex_lock(&qAMutex);
            Enqueue(qAQueue, t);
            pthread_mutex_unlock(&qAMutex);
           
        }

        // // // your code goes here
        // // // you can simulate gift request creation in here,
        // // // but make sure to launch the threads first

        ran2++;
        pthread_sleep(1);
        gettimeofday(&currentTimeReal, NULL);
        count++;
        printf("count %d\n", count);
        // printf("CurrentTime : %ld\n", currentTime.tv_sec);
    }
    keepGoing = 0;
    pthread_exit(0);
}