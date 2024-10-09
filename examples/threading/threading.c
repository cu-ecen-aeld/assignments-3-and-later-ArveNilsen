#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

// Optional: use these functions to add debug or error prints to your application
#define DEBUG_LOG(msg,...)
//#define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)

void* threadfunc(void* thread_param)
{

    // TODO: wait, obtain mutex, wait, release mutex as described by thread_data structure
    // hint: use a cast like the one below to obtain thread arguments from your parameter
    //struct thread_data* thread_func_args = (struct thread_data *) thread_param;
    
    struct thread_data *thread_func_args = (struct thread_data*) thread_param;

    if (usleep(thread_func_args->wait_pre_lock * 1000)) {
        perror("usleep failed");
        thread_func_args->thread_complete_success = false;
        return thread_param;
    }

    if (pthread_mutex_lock(thread_func_args->tdmutex) != 0) {
        perror("mutex lock failed");
        thread_func_args->thread_complete_success = false;
        return thread_param;
    }

    if (usleep(thread_func_args->wait_post_lock * 1000)) {
        perror("usleep failed");
        thread_func_args->thread_complete_success = false;
        pthread_mutex_unlock(thread_func_args->tdmutex);
        return thread_param;
    }

    if (pthread_mutex_unlock(thread_func_args->tdmutex) != 0 ) {
        perror("mutex unlock failed");
        thread_func_args->thread_complete_success = false;
        return thread_param;
    }

    thread_func_args->thread_complete_success = true;
    return thread_param;
}


bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex,int wait_to_obtain_ms, int wait_to_release_ms)
{
    /**
     * TODO: allocate memory for thread_data, setup mutex and wait arguments, pass thread_data to created thread
     * using threadfunc() as entry point.
     *
     * return true if successful.
     *
     * See implementation details in threading.h file comment block
     */
    struct thread_data *td = malloc(sizeof *td);
    if (!td)
        return false;

    td->tdmutex = mutex;
    td->wait_pre_lock = wait_to_obtain_ms;
    td->wait_post_lock = wait_to_release_ms;
    td->thread_complete_success = true;

    if (pthread_create(thread, NULL, threadfunc, (void *) td) != 0) {
        free(td);
        return false;
    }

    td->tdthread = *thread;

    bool success = td->thread_complete_success;
    return success;
}

