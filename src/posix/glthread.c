#include "hammer/error.h"
#include "hammer/glthread.h"
#include <errno.h>
#include <stdatomic.h>
#include <pthread.h>
#include <sched.h>  /* sched_yield */

glthread_callback glthread_fn;
void             *glthread_arg;
pthread_t         glthread_handle;
pthread_cond_t    glthread_cv;
pthread_mutex_t   glthread_mtx;
atomic_bool       glthread_terminate;

static void *
glthread_entry(void *arg)
{
	(void) arg;
	while (!glthread_terminate) {
		pthread_mutex_lock(&glthread_mtx);
		pthread_cond_wait(&glthread_cv, &glthread_mtx);

		glthread_callback fn = glthread_fn;
		void *arg = glthread_arg;
		if (fn) {
			glthread_fn = NULL;
			glthread_arg = NULL;
			fn(arg);
		}

		pthread_mutex_unlock(&glthread_mtx);
	}
	return NULL;
}

int
glthread_create(void)
{
	glthread_terminate = 0;

	int r;
	if ((r = pthread_cond_init(&glthread_cv, NULL)) ||
	    (r = pthread_mutex_init(&glthread_mtx, NULL)) ||
	    (r = pthread_create(&glthread_handle, NULL, glthread_entry, NULL)))
	{
		errno = r;
		xperror("Error creating OpenGL thread");
	}
	return r;
}

void
glthread_destroy(void)
{
	pthread_mutex_lock(&glthread_mtx);
	glthread_terminate = 1;
	pthread_cond_signal(&glthread_cv);
	pthread_mutex_unlock(&glthread_mtx);

	pthread_join(glthread_handle, NULL);
	pthread_mutex_destroy(&glthread_mtx);
	pthread_cond_destroy(&glthread_cv);
}

void
glthread_execute(glthread_callback fn, void *arg)
{
	glthread_fn = fn;
	glthread_arg = arg;
	/*
	 * Wake up the OpenGL thread then wait for it to begin working before
	 * attempting to acquire its mutex, then sleep until the OpenGL thread
	 * is complete.
	 */
	pthread_cond_signal(&glthread_cv);
	while (glthread_fn)
		sched_yield();
	pthread_mutex_lock(&glthread_mtx);
	pthread_mutex_unlock(&glthread_mtx);
}
