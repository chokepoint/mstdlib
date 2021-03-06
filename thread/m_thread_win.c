/* The MIT License (MIT)
 * 
 * Copyright (c) 2015 Main Street Softworks, Inc.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "m_config.h"

#include <mstdlib/mstdlib_thread.h>
#include "m_thread_int.h"
#include "base/platform/m_platform.h"
#include "base/time/m_time_int.h"
#include "m_pollemu.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#define SIGNAL    0
#define BROADCAST 1

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
struct M_thread_cond {
	HANDLE           events[2];
	HANDLE           gate;
	CRITICAL_SECTION mutex;
	int              waiters;
	int              event;
};

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static DWORD win32_abstime2msoffset(const M_timeval_t *abstime)
{
	M_timeval_t tv;
	M_int64     ret;

	M_time_gettimeofday(&tv);

	/* Subtract current time from abstime and return result in milliseconds */
	ret = (((abstime->tv_sec - tv.tv_sec) * 1000) + ((abstime->tv_usec / 1000) - (tv.tv_usec / 1000)));

	/* Sanity check to make sure GetSystemTimeAsFileTime() didn't return
	 * something bogus.  Also makes sure too much time hasn't already elapsed.
	 * If this were to return negative, it could hang indefinitely */
	if (ret < 0)
		ret = 1;
	/* check for DWORD overflow, would indicate bogus time */
	if ((ret >> 32) != 0)
		ret = 1;
	return (DWORD)ret;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static M_thread_t *M_thread_win_create(M_threadid_t *id, const M_thread_attr_t *attr, void *(*func)(void *), void *arg)
{
	DWORD dwThreadId;
	HANDLE hThread;

	if (id)
		*id = 0;

	if (func == NULL)
		return NULL;

	hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)func, arg, 0, &dwThreadId);
	if (hThread == NULL)
		return NULL;

	if (id)
		*id = dwThreadId;

	if (attr != NULL && !M_thread_attr_get_create_joinable(attr)) {
		CloseHandle(hThread);
		return (M_thread_t *)1;
	}
	return (M_thread_t *)hThread;
}

static M_bool M_thread_win_join(M_thread_t *thread, void **value_ptr)
{
	if (thread == NULL)
		return M_FALSE;

	if (WaitForSingleObject((HANDLE)thread, INFINITE) != WAIT_OBJECT_0)
		return M_FALSE;
	if (value_ptr)
		GetExitCodeThread((HANDLE)thread, (LPDWORD)value_ptr);
	CloseHandle((HANDLE)thread);
	return M_TRUE;
}

static M_threadid_t M_thread_win_self(void)
{
	return (M_threadid_t)GetCurrentThreadId();
}

static void M_thread_win_yield(M_bool force)
{
	if (!force)
		return;
	SwitchToThread();
}

static void M_thread_win_sleep(M_uint64 usec)
{
	DWORD    msec; 
	M_uint64 r;

	r = usec/1000;
	if (r > M_UINT32_MAX) {
		msec = M_UINT32_MAX;
	} else{
		msec = (DWORD)r;
	}
	Sleep(msec);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
static int M_thread_win_poll(struct pollfd fds[], nfds_t nfds, int timeout)
{
	return M_pollemu(fds, nfds, timeout);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static M_thread_mutex_t *M_thread_win_mutex_create(M_uint32 attr)
{
	M_thread_mutex_t *mutex;

	(void)attr;
	/* NOTE: we never define "struct M_thread_mutex", as we're aliasing it to a 
	 *       different type.  Bad style, but keeps our type safety */
	mutex = M_malloc_zero(sizeof(CRITICAL_SECTION));
	InitializeCriticalSection((LPCRITICAL_SECTION)mutex);

	return mutex;
}

static void M_thread_win_mutex_destroy(M_thread_mutex_t *mutex)
{
	if (mutex == NULL)
		return;

	DeleteCriticalSection((LPCRITICAL_SECTION)mutex);
	M_free(mutex);
}

static M_bool M_thread_win_mutex_lock(M_thread_mutex_t *mutex)
{
	if (mutex == NULL)
		return M_FALSE;
	EnterCriticalSection((LPCRITICAL_SECTION)mutex);
	return M_TRUE;
}

static M_bool M_thread_win_mutex_trylock(M_thread_mutex_t *mutex)
{
	if (mutex == NULL)
		return M_FALSE;

	if (TryEnterCriticalSection((LPCRITICAL_SECTION)mutex) != 0)
		return M_TRUE;
	return M_FALSE;
}

static M_bool M_thread_win_mutex_unlock(M_thread_mutex_t *mutex)
{
	if (mutex == NULL)
		return M_FALSE;
	LeaveCriticalSection((LPCRITICAL_SECTION)mutex);
	return M_TRUE;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static M_thread_cond_t *M_thread_win_cond_create(M_uint32 attr)
{
	M_thread_cond_t *cond;

	(void)attr;

	cond = M_malloc_zero(sizeof(*cond));
	cond->events[SIGNAL]    = CreateEvent(NULL, FALSE, FALSE, NULL);
	cond->events[BROADCAST] = CreateEvent(NULL, TRUE, FALSE, NULL);
	/* Use a semaphore as a gate so we don't lose signals */
	cond->gate              = CreateSemaphore(NULL, 1, 1, NULL);
	InitializeCriticalSection(&cond->mutex);
	cond->waiters           = 0;
	cond->event             = -1;

	return cond;
}

static void M_thread_win_cond_destroy(M_thread_cond_t *cond)
{
	if (cond == NULL)
		return;

	CloseHandle(cond->events[SIGNAL]);
	CloseHandle(cond->events[BROADCAST]);
	CloseHandle(cond->gate);
	DeleteCriticalSection(&cond->mutex);

	M_free(cond);
}

static M_bool M_thread_win_cond_timedwait(M_thread_cond_t *cond, M_thread_mutex_t *mutex, const M_timeval_t *abstime)
{
	DWORD                retval;
	DWORD                dwMilliseconds;

	if (cond == NULL || mutex == NULL)
		return M_FALSE;

	/* We may only enter when no wakeups active
	 * this will prevent the lost wakeup */
	WaitForSingleObject(cond->gate, INFINITE);

	EnterCriticalSection(&cond->mutex);
	/* count waiters passing through */
	cond->waiters++;
	LeaveCriticalSection(&cond->mutex);

	ReleaseSemaphore(cond->gate, 1, NULL);

	LeaveCriticalSection((LPCRITICAL_SECTION)mutex);

	if (abstime == NULL) {
		dwMilliseconds = INFINITE;
	} else {
		dwMilliseconds = win32_abstime2msoffset(abstime);
	}
	retval = WaitForMultipleObjects(2, cond->events, FALSE, dwMilliseconds);

	/* We go into a critical section to make sure wcond->waiters
	 * isn't checked while we decrement.  This is especially
	 * important for a timeout since the gate may not be closed.
	 * We need to check to see if a broadcast/signal was pending as
	 * this thread could have been preempted prior to EnterCriticalSection
	 * but after WaitForMultipleObjects() so we may be responsible
	 * for reseting the event and closing the gate */
	EnterCriticalSection(&cond->mutex);
	cond->waiters--;
	if (cond->event != -1 && cond->waiters == 0) {
		/* Last waiter needs to reset the event on(as a
		 * broadcast event is not automatic) and also
		 * re-open the gate */
		if (cond->event == BROADCAST)
			ResetEvent(cond->events[BROADCAST]);

		ReleaseSemaphore(cond->gate, 1, NULL);
		cond->event = -1;
	} else if (retval == WAIT_OBJECT_0+SIGNAL) {
		/* If specifically, this thread was signalled and there
		 * are more waiting, re-open the gate and reset the event */
		ReleaseSemaphore(cond->gate, 1, NULL);
		cond->event = -1;
	} else {
		/* This could be a standard timeout with more
		 * waiters, don't do anything */
	}
	LeaveCriticalSection(&cond->mutex);

	EnterCriticalSection((LPCRITICAL_SECTION)mutex);
	if (retval == WAIT_TIMEOUT)
		return M_FALSE;

	return M_TRUE;
}

static M_bool M_thread_win_cond_wait(M_thread_cond_t *cond, M_thread_mutex_t *mutex)
{
	if (cond == NULL || mutex == NULL)
		return M_FALSE;
	return M_thread_win_cond_timedwait(cond, mutex, NULL);
}

static void M_thread_win_cond_broadcast(M_thread_cond_t *cond)
{
	if (cond == NULL)
		return;

	/* close gate to prevent more waiters while broadcasting */
	WaitForSingleObject(cond->gate, INFINITE);

	/* If there are waiters, send a broadcast event,
	 * otherwise, just reopen the gate */
	EnterCriticalSection(&cond->mutex);
	cond->event = BROADCAST;
	/* if no waiters just reopen gate */
	if (cond->waiters) {
		/* wake all */
		SetEvent(cond->events[BROADCAST]);
	} else {
		ReleaseSemaphore(cond->gate, 1, NULL);
	}
	LeaveCriticalSection(&cond->mutex);
}

static void M_thread_win_cond_signal(M_thread_cond_t *cond)
{
	if (cond == NULL)
		return;

	/* close gate to prevent more waiters while signalling */
	WaitForSingleObject(cond->gate, INFINITE);

	/* If there are waiters, wake one, otherwise, just 
	 * reopen the gate */
	EnterCriticalSection(&cond->mutex);
	cond->event = SIGNAL;
	if (cond->waiters) {
		/* wake one */
		SetEvent(cond->events[SIGNAL]);
	} else {
		ReleaseSemaphore(cond->gate, 1, NULL);
	}
	LeaveCriticalSection(&cond->mutex);
}


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

void M_thread_win_register(M_thread_model_callbacks_t *cbs)
{
	if (cbs == NULL)
		return;

	M_mem_set(cbs, 0, sizeof(*cbs));

	cbs->init   = NULL;
	cbs->deinit = NULL;
	/* Thread */
	cbs->thread_create  = M_thread_win_create;
	cbs->thread_join    = M_thread_win_join;
	cbs->thread_self    = M_thread_win_self;
	cbs->thread_yield   = M_thread_win_yield;
	cbs->thread_sleep   = M_thread_win_sleep;
	/* System */
	cbs->thread_poll    = M_thread_win_poll;
	/* Mutex */
	cbs->mutex_create   = M_thread_win_mutex_create;
	cbs->mutex_destroy  = M_thread_win_mutex_destroy;
	cbs->mutex_lock     = M_thread_win_mutex_lock;
	cbs->mutex_trylock  = M_thread_win_mutex_trylock;
	cbs->mutex_unlock   = M_thread_win_mutex_unlock;
	/* Cond */
	cbs->cond_create    = M_thread_win_cond_create;
	cbs->cond_destroy   = M_thread_win_cond_destroy;
	cbs->cond_timedwait = M_thread_win_cond_timedwait;
	cbs->cond_wait      = M_thread_win_cond_wait;
	cbs->cond_broadcast = M_thread_win_cond_broadcast;
	cbs->cond_signal    = M_thread_win_cond_signal;
	/* Read Write Lock */
	cbs->rwlock_create  = M_thread_rwlock_emu_create;
	cbs->rwlock_destroy = M_thread_rwlock_emu_destroy;
	cbs->rwlock_lock    = M_thread_rwlock_emu_lock;
	cbs->rwlock_unlock  = M_thread_rwlock_emu_unlock;
}
