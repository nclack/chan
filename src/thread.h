#pragma once
#include <stdlib.h> //for NULL

/* Questions
 * - Is a condition variable always used with the same mutex?
 *   - Should the condition variable just be constructed with a reference to
 *     the mutex?  It doesn't look like any common thread api's do it; why not?
 */

#ifdef __cplusplus
extern "C"{
#endif

typedef void Thread;
typedef void Mutex;
typedef void Condition;
typedef void Future;

//Prefer pthread-style thread proc specification
typedef void* (*ThreadProc)(void*);
typedef void* ThreadProcArg;
typedef void* ThreadProcRet;

Thread* Thread_Alloc ( ThreadProc function, ThreadProcArg arg);
void    Thread_Free  ( Thread *self);
void*   Thread_Join  ( Thread *self);

/*  Don't really need futures.
    In boost, they provide lazy eval with callbacks, but otherwise they're
    synonymous with Join

Future* Thread_Result( Thread *self);

void*   Future_Get   ( Future *self); //wait till result is available and return result
void    Future_Free  ( Future *self);
*/

Mutex*  Mutex_Alloc ( );
void    Mutex_Free  ( Mutex* self);
void    Mutex_Lock  ( Mutex* self);
void    Mutex_Unlock( Mutex* self);

Condition* Condition_Alloc     ( );
void       Condition_Free      ( Condition* self);
void       Condition_Wait      ( Condition* self,   Mutex* lock);
void       Condition_Notify    ( Condition* self);
void       Condition_Notify_All( Condition* self);

#ifdef __cplusplus
}
//////////////////////////////////////////////////////////////////////
// C++ interface
//////////////////////////////////////////////////////////////////////
namespace thread{
  class Th
  { Thread *thread_;
    public:
      Th(ThreadProc function, ThreadProcArg arg) {thread_=Thread_Alloc(function,arg);}
      virtual ~Th()                              {Thread_Free(thread_);}
      inline void join()                         {Thread_Join(thread_);}
  };

  class AutoThread
  { Th t_;
    public:
      AutoThread(ThreadProc function, ThreadProcArg arg) : t_(function,arg) {}
      virtual ~AutoThread()                                                 {t_.join();}
  };

  class Mtx
  { Mutex *mutex_;
    public:
      Mtx()                                      {mutex_=Mutex_Alloc();}
      virtual ~Mtx()                             {Mutex_Free(mutex_);}
      void lock()                                {Mutex_Lock(mutex_);}
      inline void unlock()                       {Mutex_Unlock(mutex_);}
    friend class CV;
  };

  class AutoMtx
  { Mtx *m_;
    Mtx  own_;
    public:
      AutoMtx():m_(NULL)                         {m_=&own_; m_->lock();}
      AutoMtx(Mtx *mutex):m_(mutex)              {m_->lock();}
      virtual ~AutoMtx()                         {m_->unlock();}
    friend class CV;
  };

  class CV
  { Condition* c_;
    public:
      CV()                                       {c_=Condition_Alloc();}
      virtual ~CV()                              {Condition_Free(c_);}
      inline void wait(Mtx *m)                   {Condition_Wait(c_,m->mutex_);}
      inline void wait(AutoMtx *m)               {Condition_Wait(c_,m->m_->mutex_);}
      inline void wait(Mutex *m)                 {Condition_Wait(c_,m);} 
      inline void notify()                       {Condition_Notify(c_);}
		inline void notify_all()                   {Condition_Notify_All(c_);}
  };
  
} //end namespace thread
#endif
