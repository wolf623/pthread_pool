#ifndef __PTHREAD_POOL_C__
#define __PTHREAD_POOL_C__

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>

#include "pthread_pool.h"

pthread_pool_t *pthread_pool_create(int max_pths, int curr_pths);
void pthread_pool_destroy();
void pool_add_worker(pthread_pool_t *pool, void *(*func)(void *arg), void *arg);
void *pthread_routine(void *arg);

#ifdef THREAD_DEFINE
static pthread_cond_t *_thread_get(pthread_pool_t *pool, pthread_t tid)
{
    thread_t *head = pool->threads;
    while(head != NULL)
    {
        if(tid == head->tid)
            break; //get it
        else
            head = head->next;
    }

    return &head->cond;
}

static void _thread_add(pthread_pool_t *pool, pthread_t tid)
{
    thread_t *tmp = (thread_t *)malloc(sizeof(thread_t));
    if(tmp == NULL)
    {
        printf("%s(): malloc for thread failed!\n", __FUNCTION__);
        return;
    }

    tmp->tid = tid;
    pthread_cond_init(&tmp->cond, NULL);
    tmp->next = pool->threads;
    pool->threads = tmp;
}

static void _thread_del(pthread_pool_t *pool, pthread_cond_t *cond)
{
    thread_t *head = pool->threads;
    if(memcmp(cond, &head->cond, sizeof(pthread_cond_t)) == 0)
    {
        pool->threads = head->next;
        pthread_cond_destroy(&head->cond);
        free(head);
        return;
    }
    else
    {
        while(head->next != NULL)
        {
            thread_t *tmp = head->next;
            if(memcmp(cond, &tmp->cond, sizeof(pthread_cond_t)) == 0)
            {
                head->next = tmp->next;
                pthread_cond_destroy(&tmp->cond);
                free(tmp);
                break;
            }

            head = tmp;
        }
    }
}
#endif

pthread_pool_t *pthread_pool_create(int max_pths, int curr_pths)
{
    pthread_pool_t *pool = (pthread_pool_t *)malloc(sizeof(pthread_pool_t));
    if(pool == NULL)
    {
        printf("Create pthread pool failed!\n");
        return NULL;
    }

    memset(pool, 0, sizeof(pthread_pool_t));
    
    pool->max_pthread_num = max_pths;
    pool->curr_pthread_num = 0;
    pool->idls_pthread_num = curr_pths;
    pool->head = pool->tail = NULL;

    pthread_mutex_init(&(pool->mutex), NULL);

    pthread_cond_init(&(pool->cond), NULL);

    pool->threadIds = (pthread_t *)malloc(curr_pths * sizeof(pthread_t));
    if(pool->threadIds == NULL)
    {
        printf("Malloc memory for pool->threads failed!\n");
        return NULL;
    }
    
    int i = 0;
    for(i = 0; i < curr_pths; i++)
    {
        pthread_create(&(pool->threadIds[i]), NULL, pthread_routine, pool);
      
    }

    return pool;
}

void pool_add_worker(pthread_pool_t *pool, void *(*func)(void *arg), void *arg)
{
    assert(pool != NULL);
    pthread_job_t *job = (pthread_job_t *)malloc(sizeof(pthread_job_t));
    if(job == NULL)
    {
        printf("Malloc memory for pthread job failed!\n");
        exit(1);
    }

    job->func = func;
    job->arg = arg;
    job->next = NULL;

    pthread_mutex_lock(&pool->mutex);
    if(pool->head == NULL)
    {
        pool->head = job;
    }
    else
    {
        pool->tail->next = job;
    }
    
    pool->tail = job;

    pool->curr_pthread_num++;
    
    if(pool->idls_pthread_num > 0)
    {
        pthread_mutex_unlock(&pool->mutex);

        pthread_cond_signal(&pool->cond);
    }
    else if(pool->curr_pthread_num < pool->max_pthread_num)
    {
        //create a new thread
        pthread_t tid;
        pthread_attr_t attr;

        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

        if(pthread_create(&tid, &attr, (void*(*)(void*))pthread_routine, (void *)pool) == 0)
            pool->curr_pthread_num++;

        pthread_mutex_unlock(&pool->mutex);
        pthread_attr_destroy(&attr);
    }
    else
    {
        pthread_mutex_unlock(&pool->mutex);
    }
}


void *pthread_routine(void *arg)
{
    assert(arg != NULL);
    pthread_pool_t *pool = (pthread_pool_t *)arg;
    pthread_job_t *job = NULL;
    int status;

    pthread_mutex_lock(&pool->mutex);

   while(1)
    {
        if(pool->head != NULL)
        {
            job = pool->head;
            pool->head = job->next;
            if(pool->tail == job)
                pool->tail = NULL;

            pool->curr_pthread_num--;
            pool->idls_pthread_num++;
            pthread_mutex_unlock(&pool->mutex);

            //call callback function
            job->func(job->arg);

            free(job);

            //lock again?
            pthread_mutex_lock(&pool->mutex);
         
        }
        else
        {
            pool->idls_pthread_num++;

            status = pthread_cond_wait(&pool->cond, &pool->mutex);

            pool->idls_pthread_num--;

            if(status == 0)
            {
                //get the signal, success, go to deal with it
                continue;
            }

            pool->curr_pthread_num--;

            pthread_mutex_unlock(&pool->mutex);

            return;
        }
    }
}

void pthread_pool_destroy(pthread_pool_t *pool)
{
    pthread_mutex_lock(&pool->mutex);

    free(pool->threadIds);
    pthread_job_t *job = pool->head;

    while(job != NULL)
    {
        pthread_job_t *tmp = job->next;
        free(job);
        job = tmp;
    }
    
    pthread_mutex_unlock(&pool->mutex);

    pthread_mutex_destroy(&pool->mutex);

    pool = NULL;
}

static void *thread_callback(void *arg)
{
    int index = *(int *)arg;
    printf(">>>>Worker %d callback function.\n", index);
    sleep(1);
    return;
}

#define TEST_MAX_THREAD_NUM 256
#define TEST_THREAD_NUM 200
int main(int argc, char *argv[])
{
    pthread_pool_t *pool = pthread_pool_create(TEST_MAX_THREAD_NUM, 50);

    int i = 0;
    int num[TEST_THREAD_NUM];
    for(i=0; i<TEST_THREAD_NUM; i++)
    {
        num[i] = i;
        pool_add_worker(pool, (void*(*)(void*))thread_callback, &num[i]);
    }

    sleep(3);

    pthread_pool_destroy(pool);
    
    return 0;
}

#endif
