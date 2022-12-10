#ifndef __SCHEDULER__C__
#define __SCHEDULER__C__

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include "scheduler.h"

// Some global variables
static float global_time = 0;
static float time_slice = 2; // Change this variable to run the variations of round robin
static const int MAX_LINE_LENGTH = 255;
static bool longTermRunning = false;
FILE *completionTimes;

void init_sched_queue(sched_queue_t *queue, int queue_size)
{
	sem_init(&queue->sched_queue_sem,0,queue_size);
	sem_init(&queue->ready_sem,0,0);
	sem_init(&queue->cpu_sem,0,1);
	pthread_mutex_init(&queue->lock, NULL);
	list_init(&queue->lst);
	// TODO initialize semaphores and mutex
}

void destroy_sched_queue(sched_queue_t *queue)
{
	sem_destroy(&queue->sched_queue_sem);
	sem_destroy(&queue->ready_sem);
	sem_destroy(&queue->cpu_sem);
	pthread_mutex_destroy(&queue->lock);

	list_elem_t *lelt;
	for(lelt = list_get_head(&queue->lst); lelt != NULL; lelt = lelt->next)
		list_remove_elem(&queue->lst,lelt);


    // TODO destroy semaphores and mutex
}

void signal_process(process_t *info)
{
	sem_post(&info->cpu_sem);	

    // TODO signal the process that the CPU is free
}

void wait_for_process(sched_queue_t *queue)
{
	sem_wait(&queue->cpu_sem);
    // TODO make the dispatcher wait until CPU is available
}

void wait_for_queue(sched_queue_t *queue)
{
	sem_wait(&queue->ready_sem);
	sem_post(&queue->ready_sem);
    // TODO make the queue wait until there are ready processes in the queue
}

process_t *next_process_fifo(sched_queue_t *queue)
{
	process_t *info = NULL;
	list_elem_t *elt = NULL;

    // TODO access queue with mutual exclusion
	pthread_mutex_lock(&queue->lock);
    // TODO get the front element of the queue
	elt = list_get_head(&queue->lst);
    // TODO if the element is not NULL remove the element and retrieve the process data
	if (elt != NULL) {
		list_remove_elem(&queue->lst, elt);
		info = (process_t*)elt->datum;		

		time_slice = info->serviceTime;
	}
	pthread_mutex_unlock(&queue->lock);


    // return the process info
	return info;
}

process_t *next_process_rr(sched_queue_t *queue)
{
	process_t *info = NULL;
	list_elem_t *elt = NULL;

    // TODO access queue with mutual exclusion
	pthread_mutex_lock(&queue->lock);
    // TODO get the front element of the queue
	elt = list_get_head(&queue->lst);
    // TODO if the element is not NULL remove the element and retrieve the process data
	if (elt != NULL) {
		list_remove_elem(&queue->lst, elt);
		info = (process_t*)elt->datum;
	}
	pthread_mutex_unlock(&queue->lock);

    // return the process info
	return info;
}

void *process_function(void *arg){
    // get the element and process info from the arg
    list_elem_t *elt = (list_elem_t*) arg;
    process_t *info = (process_t *) elt->datum;
    
    // do not modify the service time in the process image, you will need that information
    float serviceTime = info->serviceTime; 
     

	while(serviceTime > 0){
	// TODO request access to CPU using process semaphore	
		sem_wait(&info->cpu_sem); 
        // TODO increment global time equal to time slice or remaining of service time
	// TODO decrease process service time by time slice
	if(serviceTime > time_slice){
            // Do some useless work
            sleep(time_slice/1000.0);
            //////////////////////

	    global_time += time_slice;
	    serviceTime = serviceTime - time_slice;
        }
        else{
            // Do some useless work
            sleep(serviceTime/1000.0);
            //////////////////////
	    global_time += serviceTime;
	    serviceTime = 0;

        }
	
		fprintf(stdout, "Finished time slice of process %d, time remaining = %f\n", info->pid, serviceTime);
		// TODO if serviceTime is not 0 insert the process to the back of the list
		if(serviceTime != 0){
		pthread_mutex_lock(&info->queue->lock);
        	list_insert_tail(&info->queue->lst, elt);
		pthread_mutex_unlock(&info->queue->lock);
		fprintf(stdout, "Inserting process %d to back of the queue\n", info->pid);
        }
        // TODO else record the info of the process in the completionTimes file, and signal a new empty slot in the queue
		// record the info as: processID arrivalTime serviceTime completionTime
        else{
		fprintf(completionTimes,"pid: %d Arrival Time: %d Service Time: %f Global Time: %f\n", info->pid, info->arrivalTime, info->serviceTime, global_time);
            	fprintf(stdout, "Teminating process %d\n", info->pid);
	    	sem_post(&info->queue->sched_queue_sem);
	    	sem_wait(&info->queue->ready_sem);

        }
		// TODO signal queue the time slice is complete
		sem_post(&info->queue->cpu_sem);
	}
	pthread_exit(0);
}

void *short_term_scheduler(void *arg){
    sched_queue_t *queue  = (sched_queue_t*) arg;
    dispatcher_t *sched_ops = &queue->sched_ops;

    // wait for queue to have processes
    sched_ops->wait_for_queue(queue);

    // start scheduling processes let the dispatcher run until the long-term exits and the queue is empty
    while(longTermRunning || list_size(&queue->lst)){   
        // TODO wait for cpu to be available
        
	sem_wait(&queue->cpu_sem);
        // choose one process from the queue list
        process_t *p = sched_ops->next_process(queue);

        // TODO If there is at least one process in the queue, execute it for time_slice amount
        if (p != NULL ){
            fprintf(stdout, "Start execution of process %d\n", p->pid);
            // TODO activate the process
         	sched_ops->signal_process(p);   
            
            // TODO wait for process to finish using CPU
         	sched_ops->wait_for_process(queue);   
        }
        //TODO else wait for 1 to arrive to the queue, don't forget to release the cpu
        else{
	sem_post(&queue->cpu_sem);
        sched_ops->wait_for_queue(queue);
	sem_wait(&queue->cpu_sem);
        }
        // TODO release cpu
	sem_post(&queue->cpu_sem);
               
    }

    // exit thread
    pthread_exit(0);
}

void *long_term_scheduler(void *arg){
    sched_queue_t *queue  = (sched_queue_t*) arg;

    // variables to read the processes from the file
    int arrival_time;
    float service_time;
    char *process_info = (char *)malloc(MAX_LINE_LENGTH);
    int pid = 1000;
    FILE *process_list = fopen("processes.txt", "r");
    
    // check if the file opened successfullym if not terminate the thread
    if(!process_list){
        fprintf(stderr, "File not found!");
        // TODO terminate thread
	pthread_exit(0);
    }
    else {
    	// TODO use longTermRunning variable to let the dispatcher know the long term scheduler is running
	longTermRunning = true;
    }

    // open the completionTimes file for writting so processes can write their information there
    completionTimes = fopen("completionTimes.txt", "w");

    while(fgets(process_info, MAX_LINE_LENGTH, process_list)){
        // TODO request access to the cpu
        sem_wait(&queue->cpu_sem);
	    // TODO read process information from file and parse arrival time, service time, and priority
        sscanf(process_info, "%d %f", &arrival_time, &service_time);
        
        
        while(arrival_time > global_time){
	        fprintf(stdout, "Waiting for process to arrive at %d, currently time %f\n",arrival_time, global_time);
            // TODO yield until the process arrives, don't forget to release the cpu before you yield
		sem_post(&queue->cpu_sem);
		sched_yield();
		sem_wait(&queue->cpu_sem);

            // pass some time to allow process to arrive
            global_time += 1;
        }
	    fprintf(stdout, "New process %d arrived at %d, currently time %f\n", pid, arrival_time, global_time);        
        // when process arrives create the process image
	pthread_t process_thread;
        process_t *p = (process_t*)malloc(sizeof(process_t));
        // TODO initialize the variables of the process
	p->pid = pid;
	pid++;
	p->arrivalTime = arrival_time;
	p->serviceTime = service_time;
	p->completionTime = 0;
	p->queue = queue;
	sem_init(&p->cpu_sem,0,0);

        /* TODO: add the process to scheduler queue
         make sure you keep track of the processes inside the queue
         make sure to always maintain mutual exclusion while modifying the queue
         use the queue semaphores to keep track of the amount of processes in the list */
        p->context = malloc(sizeof(list_elem_t));
        p->context->datum = p;
         
	while(sem_trywait(&queue->sched_queue_sem) == -1){
		sem_post(&queue->cpu_sem);
		sem_wait(&queue->cpu_sem);
	}

	pthread_mutex_lock(&queue->lock);
	list_insert_tail(&queue->lst, p->context);
	pthread_create(&process_thread, NULL, process_function, (void *)p->context);
	pthread_mutex_unlock(&queue->lock);
	sem_post(&queue->ready_sem);
        // TODO release cpu
	sem_post(&queue->cpu_sem);
        // get ready to read next line
	    process_info = (char *)malloc(MAX_LINE_LENGTH);
    }

    // TODO let the dispatcher know the list of processes ended
	longTermRunning = false;
    // exit thread
    pthread_exit(0);
}

// Dispatcher operations
dispatcher_t dispatch_fifo = {init_sched_queue, destroy_sched_queue, signal_process, wait_for_process, next_process_fifo, wait_for_queue };
dispatcher_t dispatch_rr = {init_sched_queue, destroy_sched_queue, signal_process, wait_for_process, next_process_rr, wait_for_queue };

#endif 
