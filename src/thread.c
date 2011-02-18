#include "thread.h"
#include "stdio.h"
#include "config.h"

#define thread_error(...)    do{fprintf(stderr,__VA_ARGS__);exit(-1);}while(0)
#define thread_assert(e)     if(!(e)) thread_error("Assert failed in thread module" ENDL \
																									 "\tFailed: %s" ENDL \
                                                   "\tAt %s:%d" ENDL,#e,__FILE__,__LINE__ );

typedef struct _closure_t 
{ ThreadProc    proc;
  ThreadProcArg arg;
  ThreadProcRet ret;
} closure_t;

#ifdef USE_PTHREAD
#include <pthread.h>
#define _MUTEX_INITIALIZER     {PTHREAD_MUTEX_INITIALIZER,PTHREAD_MUTEX_INITIALIZER,0}
#define _CONDITION_INITIALIZER PTHREAD_COND_INITIALIZER
#endif //USE_PTHREAD

#ifdef USE_WIN32_THREADS
#ifndef RTL_CONDITION_VARIABLE_INIT
#define RTL_CONDITION_VARIABLE_INIT {0}
#endif
#define _MUTEX_INITIALIZER     {0,0,0}
#define _CONDITION_INITIALIZER RTL_CONDITION_VARIABLE_INIT
#endif //USE_WIN32_THREADS
extern const Mutex     MUTEX_INITIALIZER     = _MUTEX_INITIALIZER;
extern const Condition CONDITION_INITIALIZER = _CONDITION_INITIALIZER;

#ifdef USE_WIN32_THREADS
#include <strsafe.h>
#define return_val_if(cond,val)    { if( (cond)) return (val); }
#define thread_assert_win32(e)     if(!(e)) {ReportLastWindowsError(); thread_error("Assert failed in thread module" ENDL \
                                                                                    "\tAt %s:%d" ENDL,#e,__FILE__,__LINE__ );}

static void ReportLastWindowsError(void) 
{ //EnterCriticalSection( _get_reporting_critical_section() );
  { // Retrieve the system error message for the last-error code

    LPVOID lpMsgBuf;
    LPVOID lpDisplayBuf;
    DWORD dw = GetLastError(); 

    FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | 
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        dw,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPTSTR) &lpMsgBuf,
        0, NULL );

    // Display the error message and exit the process

    lpDisplayBuf = (LPVOID)LocalAlloc(LMEM_ZEROINIT, 
        (lstrlen((LPCTSTR)lpMsgBuf) + 40) * sizeof(TCHAR)); 
    StringCchPrintf((LPTSTR)lpDisplayBuf, 
        LocalSize(lpDisplayBuf) / sizeof(TCHAR),
        TEXT("Failed with error %d: %s"), 
        dw, lpMsgBuf); 
    
    // spam formated string to listeners
    fprintf(stderr,"%s",lpDisplayBuf);

    LocalFree(lpMsgBuf);
    LocalFree(lpDisplayBuf);
  }
  //LeaveCriticalSection( _get_reporting_critical_section() );
}

/* Returns 1 on success, 0 otherwise.
 * Possibly generates a panic shutdown.
 * Lack of success could indicate the lock was
 *   abandoned or a timeout elapsed.
 * A warning will be generated if the lock was
 *   abandoned.
 * Return of 0 indicates a timeout or an 
 *   abandoned wait.
 */
static inline unsigned
handle_wait_for_result(DWORD result, const char* msg)
{ return_val_if( result == WAIT_OBJECT_0, 1 );
  thread_assert_win32( result != WAIT_FAILED );
#ifdef DEBUG_HANDLE_WAIT_FOR_RESULT  
  if( result == WAIT_ABANDONED )
    thread_warning("Thread(win32): Wait abandoned\r\n\t%s\r\n",msg);
  if( result == WAIT_TIMEOUT )
    thread_warning("Thread(win32): Wait timeout\r\n\t%s\r\n",msg);  
#endif
  return 0;
}

typedef HANDLE    native_thread_t;
typedef struct _thread_t
{ native_thread_t handle;
  DWORD           id;
  closure_t       closure;
} thread_t;
DWORD WINAPI win32call(LPVOID lpParam)
{ closure_t* c = (closure_t*)lpParam;
  c->ret = c->proc(c->arg);
  return 0;
}

Thread* Thread_Alloc(ThreadProc function, ThreadProcArg arg)
{ thread_t*  t;
  closure_t* c; 
  thread_assert(t = (thread_t*)calloc(1,sizeof(thread_t)));
  c=&t->closure;
  c->proc=function;
  c->arg=arg;
  c->ret=NULL;
  thread_assert_win32(SUCCEEDED(t->handle=CreateThread(
          NULL,                       // attr
          0,                          // stack size (0=use default)
          win32call,                  // function
          (LPVOID)c,                  // argument
          0,                          // run immediately
          &t->id)));                  // output: the thread handle
  return (Thread*)t;        
}

void Thread_Free(Thread* self_)
{ thread_t *self = (thread_t*)self_;
  if(self)
  { Thread_Join(self_);
    thread_assert_win32(CloseHandle(self->handle));
    free(self);
  }
}

void* Thread_Join(Thread *self_)
{ thread_t *self = (thread_t*)self_;
  handle_wait_for_result(WaitForSingleObject(self->handle,INFINITE),"Thread_Join");
  return self->closure.ret;
}

inline void Thread_Exit(unsigned exitcode)
{ ExitThread((DWORD)exitcode);
}

inline native_thread_id_t Thread_SelfID()
{ return GetCurrentThreadId();
}

void Thread_Self(Thread* out)
{ thread_t *self = (thread_t*) out;
  memset(out,0,sizeof(thread_t));
  self->handle = GetCurrentThread();
  self->id     = GetThreadId(self->handle);
}

//////////////////////////////////////////////////////////////////////
//  Mutex  ///////////////////////////////////////////////////////////
//
//  Prefer the newer Slim Read Write Lock (SRWLock) (Vista and newer).
//  SRWLocks may not be recursively acquired.  If you need recursive
//  acquisition you'll have to (a) change the recursive acquisition 
//  checks and (b) use CRITICAL_SECTION.
//  TODO
//  [ ] Use CRITICAL_SECTION if the the SRWLock isn't available
//      - Although, I think windows' Condition Variables would be missing too
//////////////////////////////////////////////////////////////////////
#define M_NATIVE(x)  ((PSRWLOCK)&((x)->lock))
#define M_SELF(x)    ((PSRWLOCK)&((x)->self_lock))
#define M_OWNER(x)   ((x)->owner)
#define M_SELF_ID(x) (GetThreadId((x)->self_lock))

//const Mutex MUTEX_INITIALIZER = {0,0,0};

Mutex* Mutex_Alloc()
{ Mutex *m;
  thread_assert(m=(Mutex*)calloc(1,sizeof(Mutex)));
  InitializeSRWLock(M_NATIVE(m));
  InitializeSRWLock(M_SELF(m));
  return (Mutex*)m;
}

void Mutex_Free(Mutex* self)
{ 
  Mutex_Lock(self);
  if(self) free(self);
}

void Mutex_Lock(Mutex* self)
{ 
  DWORD current = GetCurrentThreadId();
  AcquireSRWLockExclusive(M_SELF(self));
  if(M_OWNER(self) && M_OWNER(self)==current)
    goto ErrorAttemptedRecursiveLock;
  AcquireSRWLockExclusive(M_NATIVE(self));
  M_OWNER(self)=current;  
  ReleaseSRWLockExclusive(M_SELF(self));
  return;
ErrorAttemptedRecursiveLock:
  thread_error("Detected an attempt to recursively acquire a mutex.  This isn't allowed."ENDL);
}

void Mutex_Unlock(Mutex* self)
{ 
  DWORD current = GetCurrentThreadId();
  if(!M_OWNER(self))
    goto ErrorUnownedUnlock;
  if(current!=M_OWNER(self))
    goto ErrorStolenUnlock;
  self->owner = 0;
  ReleaseSRWLockExclusive(M_NATIVE(self));
  return;
ErrorUnownedUnlock:
  thread_error("Detected an attempt to unlock a mutex that hasn't been locked.  This isn't allowed."ENDL);
ErrorStolenUnlock:
  thread_error("Detected an attempt to unlock a mutex by a thread that's not the owner.  This isn't allowed."ENDL); 
}

//////////////////////////////////////////////////////////////////////
//  Condition Variables //////////////////////////////////////////////
//  
//  - Requires Vista or better
//  - As far as I can tell, InitializeConditionVariable does nothing.
//    I suppose it's there so that one day, it might.
//////////////////////////////////////////////////////////////////////

//const Condition CONDITION_INITIALIZER = RTL_CONDITION_VARIABLE_INIT;

#define PCONDCAST(e) ((PCONDITION_VARIABLE)(e))

Condition* Condition_Alloc()
{ Condition *c;
  thread_assert(c = (Condition*)malloc(sizeof(Condition)));
  InitializeConditionVariable(PCONDCAST(c));
  return c;
}

void Condition_Initialize(Condition* c)
{ 
  InitializeConditionVariable(PCONDCAST(c));
}

void Condition_Free(Condition* self)
{ if(self) 
  {
    //Condition_Notify_All(self); // proper use shouldn't require this?
    free(self);
  }
}

void Condition_Wait(Condition* self, Mutex* lock)
{ 
  thread_assert_win32(
    SleepConditionVariableSRW(PCONDCAST(self),M_NATIVE(lock),INFINITE,0));
  M_OWNER(lock)=GetCurrentThreadId();
}

void Condition_Notify(Condition* self)
{ 
  WakeConditionVariable(PCONDCAST(self));
}

void Condition_Notify_All(Condition* self)
{ 
  WakeAllConditionVariable(PCONDCAST(self));
}

#endif // win32


#ifdef USE_PTHREAD
#include <pthread.h>
#define thread_assert_pthread(e) if(!(e)) {perror("Thread(pthread)"); \
                                           thread_error("Assert failed in thread module" ENDL \
																									      "\tFailed: %s " ENDL \
																												"\tAt %s:%d" ENDL,#e,__FILE__,__LINE__ );} 
#define pthread_success(e) ((e)==0)
#define pth_asrt_success(e) thread_assert_pthread(pthread_success(e))
typedef struct _thread_t
{ native_thread_t handle;
} thread_t;

Thread *Thread_Alloc(ThreadProc function, ThreadProcArg arg)
{ thread_t* t;
  thread_assert(t = (thread_t*)calloc(1,sizeof(thread_t)));
  pth_asrt_success(pthread_create(
        &t->handle,                   // output: the thread handle 
        NULL,                         // attr
        function,                     // function
        arg));                        // argument
  return (Thread*)t;        
}

void Thread_Free(Thread* self_)
{ thread_t *self = (thread_t*)self_;
	if(self)
    free(self);
}

void* Thread_Join(Thread *self_)
{ thread_t *self = (thread_t*)self_;
	void* ret;
  pth_asrt_success(pthread_join(self->handle,&ret));
  return ret;
}

inline void Thread_Exit(unsigned exitcode)
{ pthread_exit((void*)exitcode);
}

inline native_thread_id_t Thread_SelfID()
{ return pthread_self();
}

void Thread_Self(Thread* out_)
{ thread_t *out= (thread_t*)out_; 
  out->handle = pthread_self();
}
//////////////////////////////////////////////////////////////////////
//  Mutex  ///////////////////////////////////////////////////////////
//
//  Disallow recursive locks.  They're not compatible with the SRWLocks used
//  here to implement Mutex on windows.  Also, I suspect it's bad design
//  (that is, in my experience, it's usually a bug).
//////////////////////////////////////////////////////////////////////
#define M_NATIVE(x) (&(x)->lock)
#define M_SELF(x)   (&(x)->self_lock)
/*
const Mutex MUTEX_INITIALIZER = {
  PTHREAD_MUTEX_INITIALIZER,
  PTHREAD_MUTEX_INITIALIZER,
  0
};
*/

Mutex* Mutex_Alloc()
{ Mutex *m;
  thread_assert(m=(Mutex*)calloc(1,sizeof(Mutex)));
  pth_asrt_success(pthread_mutex_init(M_NATIVE(m),NULL));
  pth_asrt_success(pthread_mutex_init(M_SELF(m)  ,NULL));
  return (Mutex*)m;
}

void Mutex_Free(Mutex* self)
{ 
  pth_asrt_success(pthread_mutex_destroy(M_NATIVE(self)));
  pth_asrt_success(pthread_mutex_destroy(M_SELF  (self)));
  if(self) free(self);
}

void Mutex_Lock(Mutex* self)
{ 
  pthread_t caller = pthread_self();
  pth_asrt_success(pthread_mutex_lock(M_SELF(self)));
  if(self->owner && pthread_equal(caller,self->owner))
    goto ErrorAttemptedRecursiveLock;
  pth_asrt_success(pthread_mutex_lock(M_NATIVE(self)));
  self->owner=caller;
  pth_asrt_success(pthread_mutex_unlock(M_SELF(self)));
  return;
ErrorAttemptedRecursiveLock:
  thread_error("Detected an attempt to recursively acquire a mutex.  This isn't allowed."ENDL);
}

void Mutex_Unlock(Mutex* self)
{ 
  pthread_t caller = pthread_self();
  if(!self->owner)
    goto ErrorUnownedUnlock;
  if(!pthread_equal(caller,self->owner))
    goto ErrorStolenUnlock;
  self->owner = 0;
	pth_asrt_success(pthread_mutex_unlock(M_NATIVE(self)));
  return;
ErrorUnownedUnlock:
  thread_error("Detected an attempt to unlock a mutex that hasn't been locked.  This isn't allowed."ENDL);
ErrorStolenUnlock:
  thread_error("Detected an attempt to unlock a mutex by a thread that's not the owner.  This isn't allowed."ENDL);
}

//////////////////////////////////////////////////////////////////////
//  Condition Variables //////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////

//const Condition CONDITION_INITIALIZER = PTHREAD_COND_INITIALIZER;

Condition* Condition_Alloc()
{ Condition *c;
  thread_assert(c = (Condition*)malloc(sizeof(Condition)));
  pth_asrt_success(pthread_cond_init(c,NULL));
  return c;
}

void Condition_Initialize(Condition* c)
{ 
  pth_asrt_success(pthread_cond_init(c,NULL));
}

void Condition_Free(Condition* self)
{ 
  if(self) 
  {
    pth_asrt_success(pthread_cond_destroy(self));
    free(self);
  }
}

void Condition_Wait(Condition* self, Mutex* lock)
{ 
  pth_asrt_success(pthread_cond_wait(self,M_NATIVE(lock)));
  lock->owner = pthread_self();
}

void Condition_Notify(Condition* self)
{ 
  pth_asrt_success(pthread_cond_signal(self));
}

void Condition_Notify_All(Condition* self)
{ 
  pth_asrt_success(pthread_cond_broadcast(self));
}
#endif // pthread
