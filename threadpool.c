/** Author: Saar Weitzman **/
/** I.D: 204175137 **/
/** Date: 31.12.18 **/
/** Ex2- HTTP Server Implementation **/

#include "threadpool.h"
#include <stdio.h>
#include <stdlib.h>

//#define DEBUG
#define MAX_PORT_NUM 65535
#define MIN_PORT_NUM 1

void free_threadpool(threadpool* server_threadpool);
void free_list_rec(work_t* work_ptr);


/**
 * create_threadpool creates a fixed-sized thread
 * pool.  If the function succeeds, it returns a (non-NULL)
 * "threadpool", else it returns NULL.
 * this function should:
 * 1. input sanity check
 * 2. initialize the threadpool structure
 * 3. initialized mutex and conditional variables
 * 4. create the threads, the thread init function is do_work and its argument is the initialized threadpool.
 */
threadpool* create_threadpool(int num_threads_in_pool)
{
    if (num_threads_in_pool <= 0 || num_threads_in_pool > MAXT_IN_POOL)
        return NULL;


    threadpool* server_threadpool= (threadpool*)malloc(sizeof(threadpool));   //initialize the threadpool structure
    if (server_threadpool == NULL)
    {
        printf("Error: threadpool malloc has failed\r\n");
        return NULL;
    }
    server_threadpool->num_threads = num_threads_in_pool;
    server_threadpool->qsize = 0;
    server_threadpool->shutdown = 0;
    server_threadpool->dont_accept = 0;
    server_threadpool->qhead = NULL;
    server_threadpool->qtail = NULL;

    pthread_mutex_init(&server_threadpool->qlock, NULL);  //initialize the pthread_mutex
    pthread_cond_init(&server_threadpool->q_empty, NULL);  //initialize the pthread_cond
    pthread_cond_init(&server_threadpool->q_not_empty, NULL);  //initialize the pthread_cond

    server_threadpool->threads = (pthread_t*)malloc(num_threads_in_pool * sizeof(pthread_t));  //create array of threads
    if (server_threadpool->threads == NULL)
    {
        printf("Error: pthread_t malloc has failed\r\n");
        free(server_threadpool);
        return NULL;
    }

    int i, rc = 0;
    for (i = 0; i < num_threads_in_pool ; i++)
    {
        rc = pthread_create(&server_threadpool->threads[i], NULL, do_work, (void *)server_threadpool);  //create the threads
        if (rc)
        {
            perror("ERROR with pthread_create\r\n");
            destroy_threadpool(server_threadpool);
            return NULL;
        }
    }
    return server_threadpool;
}


/**
 * The work function of the thread
 * this function should:
 * 1. lock mutex
 * 2. if the queue is empty, wait
 * 3. take the first element from the queue (work_t)
 * 4. unlock mutex
 * 5. call the thread routine
 *
 */
void* do_work(void* p)
{
    if ((threadpool*)p == NULL)
        return NULL;

    threadpool* server_threadpool = (threadpool*)p;

    while(1)
    {
        pthread_mutex_lock(&server_threadpool->qlock);   //lock the critical section of taking a job from the queue

        if (server_threadpool->shutdown == 1)   //destroy function works
        {
            pthread_mutex_unlock(&server_threadpool->qlock); //release the mutex beofre the thread dies
            return NULL;
        }
        if (server_threadpool->qsize == 0 && server_threadpool->dont_accept == 0)
            pthread_cond_wait(&server_threadpool->q_not_empty, &server_threadpool->qlock); //thread wait for a new job

        if (server_threadpool->shutdown == 1) //the destroy_threadpool function waked the thread up, so need to kill the thread
        {
            pthread_mutex_unlock(&server_threadpool->qlock); //release the mutex beofre the thread dies
            return NULL;
        }

        /*if we got here, shutdown = 0 , so the dispatch function waked up the thread so it will take a job from the queue*/

        work_t* job_to_do = server_threadpool->qhead;

        if (server_threadpool->qhead)
        {
            server_threadpool->qhead = server_threadpool->qhead->next; //dequeue the done job from the queue
            server_threadpool->qsize--;
        }
        if (server_threadpool->qsize == 0)
            server_threadpool->qtail = NULL;

        if (server_threadpool->qsize == 0 && server_threadpool->dont_accept == 1)  //let destroy function work
        {
            pthread_cond_signal(&server_threadpool->q_empty);  //raise the flag to say there is no jobs in the queue
        }
        pthread_mutex_unlock(&server_threadpool->qlock);   //unlock the critical section of taking a job from queue, so other threads will be able to get in

        if (job_to_do != NULL)  //the job is still available in the queue, so the waked up thread needs to take it
        {
            job_to_do->routine(job_to_do->arg);  //do the job
            free(job_to_do);  //the thread done the job, so it can be freed
        }
    }
}


/**
 * dispatch enter a "job" of type work_t into the queue.
 * when an available thread takes a job from the queue, it will
 * call the function "dispatch_to_here" with argument "arg".
 * this function should:
 * 1. create and init work_t element
 * 2. lock the mutex
 * 3. add the work_t element to the queue
 * 4. unlock mutex
 *
 */
void dispatch(threadpool* from_me, dispatch_fn dispatch_to_here, void *arg)
{
    if (from_me == NULL || dispatch_to_here == NULL)
        return;

    pthread_mutex_lock(&from_me->qlock);  //lock the critical section of adding job to the queue (enqueue)

    if (from_me->dont_accept == 1)
    {
        //dont accept new item to the queue
        pthread_mutex_unlock(&from_me->qlock);
        return;
    }

    work_t* work_ptr = (work_t*)malloc(sizeof(work_t));
    if (work_ptr == NULL)
    {
        printf("Error: work_t malloc has failed\r\n");
        free_threadpool(from_me);
        return;
    }
    work_ptr->routine = dispatch_to_here;
    work_ptr->next = NULL;
    work_ptr->arg = arg;


    if (from_me->qhead == NULL)  //the first job in the queue
    {
        from_me->qtail = work_ptr;
        from_me->qhead = work_ptr;
    }
    else     //enter the new job as the last node in the list
    {
        from_me->qtail->next = work_ptr;
        from_me->qtail = work_ptr;
    }
    from_me->qsize++;

    pthread_cond_signal(&from_me->q_not_empty);  //to sign the threads there is a new work that needs to be done
    pthread_mutex_unlock(&from_me->qlock);  //unlock the critical section of adding job to the queue
}


void free_threadpool(threadpool* server_threadpool)
{
    if (server_threadpool == NULL)
        return;

    if (server_threadpool->qhead)
        free_list_rec(server_threadpool->qhead);  //free what was left in the queue
    if (server_threadpool->threads)
        free(server_threadpool->threads);

    pthread_mutex_destroy(&server_threadpool->qlock);
    pthread_cond_destroy(&server_threadpool->q_empty);
    pthread_cond_destroy(&server_threadpool->q_not_empty);

    free(server_threadpool);
}


/**recursive method which gets to the last object in each list of the hash table indexes, and frees from the last object
 * until the first object in the list. function for case a malloc does not work or there is problem that cause us return from
 * the threadpool library while there are still jobs (work_t) in the queue**/
void free_list_rec(work_t* work_ptr)
{
    if (work_ptr == NULL)
        return;
    if (work_ptr->next != NULL)  //in case we still did not reach the last job in the queue
        free_list_rec(work_ptr->next);

    free(work_ptr);    //work_ptr is the last job in the queue if we got here
}


/**
 * destroy_threadpool kills the threadpool, causing
 * all threads in it to commit suicide, and then
 * frees all the memory associated with the threadpool.
 */
void destroy_threadpool(threadpool* destroyme)
{
    if (destroyme == NULL)
        return;

    pthread_mutex_lock(&destroyme->qlock);
    destroyme->dont_accept = 1;

    if (destroyme->qsize > 0)
    {
        pthread_cond_wait(&destroyme->q_empty, &destroyme->qlock); //wait for the queue to be empty before destroying the threadpool
    }

    destroyme->shutdown = 1;
    pthread_cond_broadcast(&destroyme->q_not_empty);  //wake up all the threads that sleep on the condition variable q_empty and wait for a job
    pthread_mutex_unlock(&destroyme->qlock);

    int i;
    for (i = 0; i < destroyme->num_threads; i++)
    {
        pthread_join(destroyme->threads[i], NULL);
    }
    free_threadpool(destroyme);  //free all the memory allocations
}