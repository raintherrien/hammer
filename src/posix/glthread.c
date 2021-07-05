#include "hammer/client/glthread.h"
#include "hammer/client/window.h"
#include "hammer/error.h"
#include "hammer/time.h"
#include <errno.h>
#include <stdatomic.h>
#include <pthread.h>

static struct {
	_Atomic(glthread_callback) fn;
	void             *arg;
	int               ret;
	pthread_t         handle;
	pthread_cond_t    cv;
	pthread_mutex_t   mtx;
	atomic_bool       terminate;
} glthread;

static void *
glthread_entry(void *_)
{
	(void) _;

	window_create();

	pthread_mutex_lock(&glthread.mtx);
	while (!glthread.terminate) {
		void *arg = glthread.arg;
		glthread.arg = NULL;
		glthread_callback fn = atomic_exchange_explicit(
		                         &glthread.fn, NULL,
		                         memory_order_relaxed);
		if (fn)
			glthread.ret = fn(arg);

		pthread_cond_wait(&glthread.cv, &glthread.mtx);
	}
	pthread_mutex_unlock(&glthread.mtx);

	window_destroy();

	return NULL;
}

void
glthread_create(void)
{
	glthread.terminate = 0;

	int r;
	if ((r = pthread_cond_init(&glthread.cv, NULL)) ||
	    (r = pthread_mutex_init(&glthread.mtx, NULL)) ||
	    (r = pthread_create(&glthread.handle, NULL, glthread_entry, NULL)))
	{
		errno = r;
		xpanic("Error creating OpenGL thread");
	}
}

void
glthread_destroy(void)
{
	pthread_mutex_lock(&glthread.mtx);
	glthread.terminate = 1;
	pthread_cond_signal(&glthread.cv);
	pthread_mutex_unlock(&glthread.mtx);

	pthread_join(glthread.handle, NULL);
	pthread_mutex_destroy(&glthread.mtx);
	pthread_cond_destroy(&glthread.cv);
}

int
glthread_execute(glthread_callback fn, void *arg)
{
	window_gl_timer_begin();

	pthread_mutex_lock(&glthread.mtx);
	atomic_store_explicit(&glthread.fn, fn, memory_order_relaxed);
	glthread.arg = arg;
	pthread_mutex_unlock(&glthread.mtx);

	/*
	 * Wake up the OpenGL thread then wait for it to begin working before
	 * attempting to acquire its mutex, then sleep until the OpenGL thread
	 * is complete.
	 */
	while (atomic_load_explicit(&glthread.fn, memory_order_relaxed))
		pthread_cond_signal(&glthread.cv);

	pthread_mutex_lock(&glthread.mtx);
	pthread_mutex_unlock(&glthread.mtx);

	window_gl_timer_end();

	return glthread.ret;
}
