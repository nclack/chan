#ifndef _H_THREAD
#define _H_THREAD

#include <stdlib.h> //for NULL

//////////////////////////////////////////////////////////////////////
// NOTES
//
// Windows Vista and later has almost one-to-one compatibility with pthreads.
// Here I provide a wrapper to give the functionality provided by both
// libraries a common interface.  I try to follow the pthread symantics when
// possible.  Below are some exceptions:
//
// Thread_Alloc() and Thread_Free()
//
//   - need alloc/free symantics because of how windows allocates 
//     threads.
//
// Thread_Exit(unsigned)
//
//   - windows threads return a DWORD that is usually interpreted as an exit
//     code.  Pthreads return a void* that may point to the result of some
//     computation.  The pthread convention is maintained by Thread_Join().
//     That is, threads return a (void*).
//
//     Thread_Exit causes a thread to immediately abort.  There's no real
//     isomorphism between pthread and windows here.  The return type
//     of a windows thread isn't wide enough to use as a pointer on 64-bit
//     systems.  As a result, it doesn't get used for Thread_Join(). 
//
//////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////
// CONFIG
//////////////////////////////////////////////////////////////////////
#ifdef _WIN32
#define USE_WIN32_THREADS
#else
#define USE_PTHREAD
#endif

#ifdef __cplusplus
extern "C"{
#endif

#ifdef USE_PTHREAD
#include <pthread.h>
typedef pthread_t       native_thread_t;
typedef pthread_t       native_thread_id_t;
typedef pthread_mutex_t native_mutex_t;
typedef pthread_cond_t  native_cond_t;
#endif //USE_PTHREAD

#ifdef USE_WIN32_THREADS
//#include <windows.h> // can't do this bc windows defines UINT8 etc... these collide with mylib names
                       // so we fake it
typedef void*               _HANDLE;
typedef unsigned            _DWORD;
typedef void*               _SRWLOCK;
typedef struct{void* Ptr;}  _CONDITION_VARIABLE;

typedef _HANDLE             native_thread_t;
typedef _DWORD              native_thread_id_t;
typedef _SRWLOCK            native_mutex_t;
typedef _CONDITION_VARIABLE native_cond_t;
#endif //USE_WIN32_THREADS


typedef struct _mutex_t
{ native_mutex_t  lock; 
  native_mutex_t  self_lock;  
  native_thread_id_t owner;
} Mutex;
       
typedef void          Thread;
typedef native_cond_t Condition;
            
extern const Mutex     MUTEX_INITIALIZER;
extern const Condition CONDITION_INITIALIZER;

//Prefer pthread-style thread proc specification
typedef void* (*ThreadProc)(void*);
typedef void* ThreadProcArg;
typedef void* ThreadProcRet;

Thread* Thread_Alloc ( ThreadProc function, ThreadProcArg arg);
void    Thread_Free  ( Thread* self);
void*   Thread_Join  ( Thread* self);
void    Thread_Exit  ( unsigned exitcode);
extern native_thread_id_t Thread_SelfID( );
void    Thread_Self  ( Thread* out );

Mutex*  Mutex_Alloc ( );
void    Mutex_Free  ( Mutex* self);
void    Mutex_Lock  ( Mutex* self);
// TODO:? int     Mutex_Try_Lock(Mutex* self);
void    Mutex_Unlock( Mutex* self);

Condition* Condition_Alloc     ( );
void       Condition_Initialize( Condition* self);
void       Condition_Free      ( Condition* self);
void       Condition_Wait      ( Condition* self, Mutex* lock);
// TODO: int        Condition_Timed_Wait( Condition* self, Mutex* lock, unsigned timeout_ms);
void       Condition_Notify    ( Condition* self);
void       Condition_Notify_All( Condition* self);

#ifdef __cplusplus
}
#endif

#endif //#ifndef _H_THREAD
