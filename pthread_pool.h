#ifndef __PTHREAD_POOL_H__
#define __PTHREAD_POOL_H__

#include <pthread.h>

struct _pthread_job_t
{
	void *(*func)(void *arg); 			//callback function
	void *arg;							//parameter	
	struct _pthread_job_t *next; 		//next job
};

typedef struct _pthread_job_t pthread_job_t;

#ifdef THREAD_DEFINE
struct _thread_t
{
	pthread_t tid;				//pthread id
	pthread_cond_t cond;		//current pthread cond
	struct _thread_t *next; 	//next thread
};

typedef struct _thread_t thread_t;
#endif

struct _pthread_pool_t
{
	int max_pthread_num;				    //max pool size
	int curr_pthread_num; 				    //current pthread numbers
	int idls_pthread_num;				    //current idle pthread numbers
	pthread_mutex_t mutex; 				    //pthread mutex
	
#ifdef THREAD_DEFINE
	struct _thread_t *threads;			    //all pthread conds
#else
    pthread_t *threadIds;
    pthread_cond_t cond;
#endif

	struct _pthread_job_t *head;		    //job list head
	struct _pthread_job_t *tail;		    //job list tail
};

typedef struct _pthread_pool_t pthread_pool_t;

#endif
