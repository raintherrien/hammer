#include "hammer/client/glthread.h"
#include "hammer/client/window.h"
#include "hammer/error.h"
#include <errno.h>
#include <stdatomic.h>
#include <Windows.h>

static struct {
	_Atomic(glthread_callback) fn;
	void              *arg;
	int                ret;
	HANDLE             handle;
	CONDITION_VARIABLE cv;
	SRWLOCK            srwlock;
	atomic_bool        terminate;
} glthread;

static void
win32_panic(void)
{
	size_t bufsz = 256;
	char buf[bufsz];
	memset(buf, 0, bufsz);
	FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, GetLastError(), 0, buf, bufsz, NULL);
	xpanicva("Error creating OpenGL thread: %.*s", bufsz - 1, buf);
}

static DWORD
glthread_entry(LPVOID _)
{
	(void) _;

	window_create();

	AcquireSRWLockShared(&glthread.srwlock);
	while (!glthread.terminate) {
		void *arg = glthread.arg;
		glthread.arg = NULL;
		glthread_callback fn = atomic_exchange_explicit(
		                         &glthread.fn, NULL,
		                         memory_order_relaxed);
		if (fn)
			glthread.ret = fn(arg);

		SleepConditionVariableSRW(&glthread.cv, &glthread.srwlock, INFINITE, CONDITION_VARIABLE_LOCKMODE_SHARED);
	}
	ReleaseSRWLockShared(&glthread.srwlock);

	window_destroy();

	return 1;
}

void
glthread_create(void)
{
	glthread.terminate = 0;

	glthread.handle = CreateThread(NULL, 0, glthread_entry, NULL, 0, NULL);
	if (glthread.handle == NULL) {
		win32_panic();
	}
	InitializeSRWLock(&glthread.srwlock);
	InitializeConditionVariable(&glthread.cv);
}

void
glthread_destroy(void)
{
	AcquireSRWLockShared(&glthread.srwlock);
	glthread.terminate = 1;
	WakeConditionVariable(&glthread.cv);
	ReleaseSRWLockShared(&glthread.srwlock);

	DWORD waitrc = WaitForSingleObject(glthread.handle, INFINITE);
	BOOL closed = CloseHandle(glthread.handle);
	if (!closed) {
		win32_panic();
	}
	if (waitrc != WAIT_OBJECT_0) {
		win32_panic();
	}
	/* Win32 SRW and CV are stateless :) */
}

int
glthread_execute(glthread_callback fn, void *arg)
{
	AcquireSRWLockShared(&glthread.srwlock);
	atomic_store_explicit(&glthread.fn, fn, memory_order_relaxed);
	glthread.arg = arg;
	ReleaseSRWLockShared(&glthread.srwlock);

	/*
	 * Wake up the OpenGL thread then wait for it to begin working before
	 * attempting to acquire its mutex, then sleep until the OpenGL thread
	 * is complete.
	 */
	while (atomic_load_explicit(&glthread.fn, memory_order_relaxed))
		WakeConditionVariable(&glthread.cv);

	AcquireSRWLockShared(&glthread.srwlock);
	ReleaseSRWLockShared(&glthread.srwlock);

	return glthread.ret;
}
