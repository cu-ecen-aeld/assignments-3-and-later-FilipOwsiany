#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>


// Optional: use these functions to add debug or error prints to your application
#define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)

void* threadfunc(void* thread_param)
{
    struct thread_data* thread_func_args = (struct thread_data *) thread_param;
    DEBUG_LOG("Thread started with wait_to_obtain_ms: %d, wait_to_release_ms: %d",
              thread_func_args->wait_to_obtain_ms, thread_func_args->wait_to_release_ms);

    usleep(thread_func_args->wait_to_obtain_ms * 1000); // Convert ms to us
    if (pthread_mutex_lock(thread_func_args->mutex) != 0) 
    {
        ERROR_LOG("Failed to lock mutex");
        thread_func_args->thread_complete_success = false;
        return thread_param; // Return the thread data structure
    }
    DEBUG_LOG("Mutex obtained, sleeping for %d ms", thread_func_args->wait_to_release_ms);
    usleep(thread_func_args->wait_to_release_ms * 1000); // Convert ms to us
    if (pthread_mutex_unlock(thread_func_args->mutex) != 0) 
    {
        ERROR_LOG("Failed to unlock mutex");
        thread_func_args->thread_complete_success = false;
        return thread_param; // Return the thread data structure
    }
    DEBUG_LOG("Mutex released, thread will exit now");
    thread_func_args->thread_complete_success = true;

    return thread_param;
}


bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex,int wait_to_obtain_ms, int wait_to_release_ms)
{
    if (thread == NULL || mutex == NULL) 
    {
        ERROR_LOG("Invalid thread or mutex pointer");
        return false;
    }
    struct thread_data* thread_data = malloc(sizeof(struct thread_data));
    if (thread_data == NULL) 
    {
        ERROR_LOG("Failed to allocate memory for thread_data");
        return false;
    }

    thread_data->mutex = mutex;
    thread_data->wait_to_obtain_ms = wait_to_obtain_ms;
    thread_data->wait_to_release_ms = wait_to_release_ms;
    thread_data->thread_complete_success = false;

    int result = pthread_create(thread, NULL, threadfunc, (void*)thread_data);
    if (result != 0) 
    {
        ERROR_LOG("Failed to create thread");
        free(thread_data);
        return false;
    }


    
    return true;
}

