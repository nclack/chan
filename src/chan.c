/** \file 
    \ref Chan implementation.

    \mainpage
    \ref Chan is a cross-platfrom zero-copy multi-producer mutli-consumer
    asynchronous FIFO queue for passing messages between threads.

    Reading and writing to the queue are both performed via Chan_Next().
    Whether a "read" or a "write" happens depends on the \a mode passed to
    Chan_Open(), which returns a reference-counted "reader" or "writer"
    reference to the queue.

    Note that "opening" a channel is different than "allocating" it.  A \ref
    Chan is typically allocated once, but may be opened and closed many times.
    Chan_Alloc() initializes and reserves memory for the queue.  Internally,
    Chan_Open() does little more than some bookkeeping.
    
    \ref Chan was designed so that Chan_Next() operations block the calling
    thread under some circumstances.  See \ref next for details.  With proper
    use, this design garauntees that certain networks of threads (namely,
    directed acyclic graphs) connected by \ref Chan instances will deliver
    every message emited from a producer to a consumer in that network.  Once
    the network is assembled, messages will be recieved in topological order.

    \section ex Example producer/consumer

    An example producer thread might look something like this:
    \code
    void producer(Chan *q)
    { void  *data = Chan_Token_Buffer_Alloc(q);
      size_t nbytes;
      Chan  *output = Chan_Open(q,CHAN_WRITE);
      while( getdata(&data,&nbytes) )
        if( CHAN_FAILURE( Chan_Next(output,&data,&nbytes)))
          break;
      Chan_Close(output);
      Chan_Token_Buffer_Free(data);
    }
    \endcode

    An example consumer might look something like this:
    \code
    void consumer(Chan *q)
    { void *data = Chan_Token_Buffer_Alloc(q);
      size_t nbytes;
      Chan *input = Chan_Open(q,CHAN_READ);
      while( CHAN_SUCCESS( Chan_Next(input,&data,&nbytes)))
        dosomething(data,nbytes);
      Chan_Close(input);
      Chan_Token_Buffer_Free(data);
    }
    \endcode

    Both the \c producer() and \c consumer() take a \ref Chan*, \c q, as input.
    This is the communication channel that will link the two threads.  Both
    functions start by using Chan_Token_Buffer_Alloc() to allocate space for
    the first message.  It isn't neccessary to call this in \c producer(), but
    it is a recommended pattern.  See \ref mem for details.

    The \c producer() opens a \ref CHAN_WRITE reference to \c q, and
    generates data using the mysterious \c getdata() procedure that fills the 
    \c data buffer with something interesting.  It is ok if \c getdata()
    changes or reallocates the \c data pointer.

    The producer() then pushes \c data onto the queue with Chan_Next().  Pushing 
    just swaps the address pointed to by \c data with the address of an unused buffer
    that the queue has been keeping track of, so pushing is fast.  In the example,
    if the push fails, the \c producer() will clean up and terminate.  Techinically,
    the push can only fail here if something goes wrong with the under-the-hood 
    synchronization.

    The consumer() thread will wait at the first Chan_Next() call till a message is
    available.  The order in which the produce() or consumer() threads are started
    does not matter.  Popping just swaps the address pointed to by \c data with the
    last message on the queue, so popping is fast.  This behavior is the reason behind 
    allocating the \c data buffer with Chan_Token_Buffer_Alloc(); the queue
    will hold on to that buffer and recycle it later.
    
    Now suppose we have a different scenario.  The \c consumer thread starts, 
    calls Chan_Next() and begins waiting for data, but \c producer() never 
    produces any data; \c getdata() returns false.
    When the \c producer() calls Chan_Close(), this releases the last writer
    reference to the queue and notifies waiting threads.  The \c consumer()'s
    call to Chan_Next() will return false, since no data was available, and
    the consumer can get on with it's life.

    \section next Next Functions
  
    These require a mode to be set via Chan_Open().
    For \ref Chan's open in read mode, does a pop.  Reads will fail if there
        are no writers for the \ref Chan and the \ref Chan is empty and
        a writer was at some point opened for the \ref Chan.
        Otherwise, when the \ref Chan is empty, reads will block.
    For \ref Chan's open in write mode, does a push.  Writes will block when
        the \ref Chan is full.

    \verbatim
      *    Overflow                       Underflow
    =====  ============================   ===================
    -      Waits or expands.              Fails if no sources, otherwise waits.
    Copy   Waits or expands.              Fails if no sources, otherwise waits.
    Try    Fails immediately              Fails immediately.
    Timed  Waits.  Fails after timeout.   Fails immediately if no sources, otherwise waits till timeout.
    \endverbatim

    \section peek Peek Functions

    The Chan_Peek() functions behave very similarly to the Chan_Next() family.
    However, Chan_Peek() does not alter the queue.  The message at the end
    of the queue is copied into supplied buffer.  The buffer may be resized
    to fit the message.

    The Chan_Peek() functions do not require a \ref Chan to be opened in any
    particular mode.  However, the \ref Chan must be opened with Chan_Open().
    The \ref CHAN_PEEK mode is recommended for readability.

    \section mem Memory management

    \ref Chan is a zero-copy queue.  Instead of copying data, the queue 
    operates on pointers.  The pointers address heap allocated blocks of 
    data that are, ideally, big enough for your messages.  The block size 
    is set by the Chan_Alloc() call. Chan_Resize() can be used to change 
    the size of the blocks, though it is only possible to \a increase the size.

    When a buffer is swapped on to a queue via Chan_Next(), it may be resized
    so it is large enough to hold a block.  It is recommended that  
    Chan_Token_Buffer_Alloc() is used to pre-allocate buffers that 
    are to be swapped on to the queue.  If nothing else, it might help you
    remember to free any blocks returned by Chan_Next().

    That said, it's definitely possible to do:
    \code
    { void *data    = NULL;
      size_t nbytes = 0;
      Chan_Next(q,&data,&nbytes);             // q can have CHAN_READ or CHAN_WRITE mode
      // data now points to a buffer of size nbytes.
      // if Chan_Next failed, data might still be NULL.
      if(data) Chan_Token_Buffer_Free(data);  // remember to free the data!
    }
    \endcode
  */
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-value"
#pragma clang diagnostic ignored "-Wparentheses"

#include <stdio.h>   // for printf used in reporting errors, etc...
#include <string.h>
#include "config.h"
#include "thread.h"
#include "chan.h"
#include "fifo.h"

#define SUCCESS (0) 
#define FAILURE (1)

#define DEBUG_CHAN

//////////////////////////////////////////////////////////////////////
//  Logging    ///////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////// 
void chan_breakme() {}
#define chan_warning(...) printf(__VA_ARGS__);
#define chan_error(...)   do{fprintf(stderr,__VA_ARGS__);chan_breakme(); exit(-1);}while(0)

#ifdef DEBUG_CHAN
#define chan_debug(...)   printf(__VA_ARGS__)
#else
#define chan_debug(...)
#endif

// debug
#ifdef DEBUG_CHAN
#define Chan_Assert(expression) \
  if(!(expression))\
    chan_error("Assertion failed: %s\n\tIn %s (line: %u)\n", #expression, __FILE__ , __LINE__ )
#else
#define Chan_Assert(expression) (expression)  
#define DEBUG_REQUEST_STORAGE
#endif

#if 0
#define DEBUG_SHOW_REFS chan_debug("%s(%d) - Refs: %d"ENDL,__FILE__,__LINE__,c->q->ref_count)
#else
#define DEBUG_SHOW_REFS
#endif

#define CHAN_ERR__INVALID_MODE chan_error("Error: At %s(%d)"ENDL \
                                          "\tChannel has invalid read/write mode."ENDL, \
                                          __FILE__,__LINE__)

#define CHAN_WRN__NULL_ARG(e)  chan_warning("Warning: At %s(%d)"ENDL \
                                            "\tArgument <%s> was NULL."ENDL, \
                                            __FILE__,__LINE__,#e)

#define HERE printf("HERE: Line % 5d File: %s\n",__LINE__,__FILE__)

//////////////////////////////////////////////////////////////////////
//  Utilities  ///////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
#define return_if_fail(cond)          { if(!(cond)) return; }
#define return_val_if(cond,val)       { if( (cond)) return (val); }
#define goto_if_not(e,lbl)            { if(!(e)) goto lbl; }
#define goto_if(e,lbl)                { if(e) goto lbl; }

//////////////////////////////////////////////////////////////////////
//  Chan       ///////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////

typedef uint32_t u32;

typedef struct
{ Fifo *fifo;  

  u32 ref_count;
  u32 nreaders;
  u32 nwriters;
  u32 expand_on_full;
  u32 flush;
  
  Mutex              lock;
  Condition          notfull;  //predicate: not full  || expand_on_full
  Condition          notempty; //predicate: not empty || no writers 
  Condition          changedRefCount; //predicate: refcount != n
  Condition          haveWriter; //predicate: nwriters>0
  Condition          haveReader; //predicate: nreaders>0

  void              *workspace;  // Token buffer used for copy operations.
} __chan_t;

typedef struct _chan
{ 
  __chan_t *q;
  ChanMode  mode;
} chan_t;

__chan_t* chan_alloc(size_t buffer_count, size_t buffer_size_bytes)
{ __chan_t *c=0;
  Fifo *fifo;
  if(fifo=Fifo_Alloc(buffer_count,buffer_size_bytes))
  { Chan_Assert(c=(__chan_t*)calloc(1,sizeof(__chan_t)));
    c->fifo = fifo;
    c->lock = MUTEX_INITIALIZER;
    Condition_Initialize(&c->notfull);
    Condition_Initialize(&c->notempty);
    Condition_Initialize(&c->changedRefCount);
    Condition_Initialize(&c->haveWriter);
    Condition_Initialize(&c->haveReader);
    c->workspace = Fifo_Alloc_Token_Buffer(c->fifo);
    c->ref_count=1;
  }
  return c;
}

void chan_destroy(__chan_t* c)
{ //precondition - called when the last reference is released
  //             - nobody should be waiting
  Fifo_Free_Token_Buffer(c->workspace);
  Fifo_Free(c->fifo);
  free(c);
}

Chan* Chan_Alloc( size_t buffer_count, size_t buffer_size_bytes)
{ chan_t   *c=0;
  __chan_t *q=0;
  if(q=chan_alloc(buffer_count,buffer_size_bytes))
  { Chan_Assert(c=(chan_t*)malloc(sizeof(chan_t)));
    c->q = q;
    c->mode = CHAN_NONE;
  }
  return (Chan*)c;
}

inline
Chan *Chan_Alloc_Copy( Chan *chan)
{ return Chan_Alloc(Chan_Buffer_Count(chan),Chan_Buffer_Size_Bytes(chan));
}

// must be called from inside a lock
chan_t* incref(chan_t *c)
{ chan_t *n;
  ++(c->q->ref_count);
  goto_if_not(n=malloc(sizeof(chan_t)),ErrorAlloc);
  memcpy(n,c,sizeof(chan_t));
  DEBUG_SHOW_REFS;
  Condition_Notify_All(&c->q->changedRefCount);
  return n;
ErrorAlloc:
  --(c->q->ref_count);
  //Mutex_Unlock(&c->q->lock);
  chan_error("malloc() call failed."ENDL);
  return 0;
}

void decref(chan_t **pc)
{ u32 remaining;
  chan_t *c;
  Chan_Assert(pc);
  c=*pc; *pc=0;
  Chan_Assert(c);
  Mutex_Lock(&c->q->lock);
  remaining = --(c->q->ref_count);
  DEBUG_SHOW_REFS;
  Mutex_Unlock(&c->q->lock);
  Chan_Assert(remaining>=0);
  if(remaining==0)
    chan_destroy(c->q);
  else
    Condition_Notify_All(&c->q->changedRefCount);
    
  free(c);
}

Chan* Chan_Open( Chan *self, ChanMode mode)
{ chan_t *n,*c;
  c = (chan_t*)self;
  Chan_Assert( Chan_Get_Ref_Count(self)>0 );
  Mutex_Lock(&c->q->lock);
  goto_if_not(n = incref(c),ErrorIncref);
  n->mode = mode;
  switch(mode)
  { case CHAN_READ:
      ++(n->q->nreaders);
      if(Fifo_Is_Empty(n->q->fifo))
        n->q->flush=0;
      Condition_Notify_All(&n->q->haveReader);
      break;
    case CHAN_WRITE: 
      ++(n->q->nwriters);
      n->q->flush=0;
      Condition_Notify_All(&n->q->haveWriter);
      break;
    case CHAN_NONE:
      break;
    default:
      CHAN_ERR__INVALID_MODE;
      break;
  }
ErrorIncref: 
  //n will be null if there's an error.
  //The error should already be reported.
  //This is here just in case the error doesn't cause a panic.
  Mutex_Unlock(&c->q->lock);
  return (Chan*)n;
}

int Chan_Close( Chan *self_ )
{ chan_t *self = (chan_t*)self_;
  int notify=0;
  if(!self)
  {
    //CHAN_WRN__NULL_ARG(self);
    return SUCCESS;
  }
  Mutex_Lock(&self->q->lock);
  { __chan_t *q = self->q;
    switch(self->mode)
    { case CHAN_READ:  
        Chan_Assert( (--(q->nreaders))>=0 );
        if(q->nreaders==0)
          q->flush=0;
        break;
      case CHAN_WRITE:
        Chan_Assert( (--(q->nwriters))>=0 );
        Condition_Notify_All(&q->haveWriter);
        notify = (q->nwriters==0);
        if(notify)
          q->flush=1;
        break;
      default:
        ;
    }
  }
  if(notify)
    Condition_Notify_All(&self->q->notempty);
  Mutex_Unlock(&self->q->lock);
  decref(&self);
  return SUCCESS;
}

unsigned Chan_Get_Ref_Count(Chan* self_)
{ chan_t *self = (chan_t*)self_; 
  return self->q->ref_count;
}

void Chan_Wait_For_Ref_Count(Chan* self_,size_t n)
{ chan_t *self = (chan_t*)self_; 
  __chan_t *q = self->q;
  Mutex_Lock(&q->lock);
  while(q->ref_count!=n)
    Condition_Wait(&q->changedRefCount,&q->lock);
  chan_debug("WaitForRefCount: target: %zu        now: %u"ENDL,n,q->ref_count);
  Mutex_Unlock(&q->lock);
}

void Chan_Wait_For_Writer_Count(Chan* self_,size_t n)
{ chan_t *self = (chan_t*)self_; 
  __chan_t *q = self->q;
  Mutex_Lock(&q->lock);
  while(q->nwriters!=n)
    Condition_Wait(&q->haveWriter,&q->lock);
  chan_debug("WaitForWriter - writer count: %d"ENDL,q->nwriters);
  Mutex_Unlock(&q->lock);
}

void Chan_Wait_For_Have_Reader(Chan* self_)
{ chan_t *self = (chan_t*)self_; 
  __chan_t *q = self->q;
  Mutex_Lock(&q->lock);
  while(q->nreaders==0)
    Condition_Wait(&q->haveReader,&q->lock);
  chan_debug("WaitForHaveReader - reader count: %d"ENDL,q->nreaders);
  Mutex_Unlock(&q->lock);
}

void Chan_Set_Expand_On_Full( Chan* self_, int expand_on_full)
{ chan_t *self = (chan_t*)self_;  
  self->q->expand_on_full=expand_on_full;
  if(expand_on_full)
    Condition_Notify_All(&self->q->notfull);
}

// ----
// Next
// ----

unsigned int chan_push__locked(__chan_t *q, void **pbuf, size_t sz, unsigned timeout_ms)
{ 
  while(Fifo_Is_Full(q->fifo) && q->expand_on_full==0)
    Condition_Wait(&q->notfull,&q->lock); // TODO: use timed wait?
  if(FIFO_SUCCESS(Fifo_Push(q->fifo,pbuf,sz,q->expand_on_full)))
    return SUCCESS;
  return FAILURE;
}

static inline int _pop_bypass_wait(__chan_t *q)
{ 
  return q->nwriters==0 && q->flush;  
}

unsigned int chan_pop__locked(__chan_t *q, void **pbuf, size_t sz, unsigned timeout_ms)
{ //int starved;
  while(Fifo_Is_Empty(q->fifo) && !_pop_bypass_wait(q))
    Condition_Wait(&q->notempty,&q->lock); // TODO: use timed wait?
  //starved = Fifo_Is_Empty(q->fifo) && q->nwriters==0;
  if(FIFO_SUCCESS(Fifo_Pop(q->fifo,pbuf,sz)))
    return SUCCESS;
  return FAILURE;
}

static inline int _peek_bypass_wait(__chan_t *q)
{ 
  return q->nwriters==0 && q->flush;  
}

unsigned int chan_peek__locked(__chan_t *q, void **pbuf, size_t sz, unsigned timeout_ms)
{ //int starved;
  while(Fifo_Is_Empty(q->fifo) && !_peek_bypass_wait(q))
    Condition_Wait(&q->notempty,&q->lock); // TODO:!! use timed wait
  //starved = Fifo_Is_Empty(q->fifo) && q->nwriters==0;
  if(FIFO_SUCCESS(Fifo_Peek(q->fifo,pbuf,sz)))
    return SUCCESS;
  return FAILURE;
}

unsigned int chan_push(chan_t *self, void **pbuf, size_t sz, int copy, unsigned timeout_ms)
{ // TO SELF: use timeout=0 for try 
  // precondition: this should be a "Write" mode channel
  Mutex_Lock(&self->q->lock);
  { __chan_t *q = self->q;
    if(timeout_ms==0)
      goto_if(Fifo_Is_Full(q->fifo),NoPush);
    if(copy)
    { Fifo_Resize(q->fifo,sz);
      Fifo_Resize_Token_Buffer(q->fifo,&q->workspace);
      memcpy(q->workspace,*pbuf,sz);
      goto_if(CHAN_FAILURE(chan_push__locked(q,&q->workspace,sz,timeout_ms)),NoPush);
    } else
    {
      goto_if(CHAN_FAILURE(chan_push__locked(q,pbuf,sz,timeout_ms)),NoPush);
    }
  }
  Mutex_Unlock(&self->q->lock);
  Condition_Notify(&self->q->notempty);
  return SUCCESS;
NoPush:
  Mutex_Unlock(&self->q->lock);
  return FAILURE;
}

unsigned int chan_pop(chan_t *self, void **pbuf, size_t sz, int copy, unsigned timeout_ms)
{ 
  Mutex_Lock(&self->q->lock);
  { __chan_t *q = self->q;
    if(timeout_ms==0)
      goto_if(Fifo_Is_Empty(q->fifo),NoPop);
    if(copy)
    { Fifo_Resize(q->fifo,sz);
      Fifo_Resize_Token_Buffer(q->fifo,&q->workspace);
      goto_if(CHAN_FAILURE(chan_pop__locked(q,&q->workspace,sz,timeout_ms)),NoPop);
      memcpy(*pbuf,q->workspace,sz);
    } else
      goto_if(CHAN_FAILURE(chan_pop__locked(q,pbuf,sz,timeout_ms)),NoPop);
  }            
  Condition_Notify(&self->q->notfull);
  Mutex_Unlock(&self->q->lock);
  return SUCCESS;
NoPop:
  Mutex_Unlock(&self->q->lock);
  return FAILURE;
}

unsigned int chan_peek(chan_t *self, void **pbuf, size_t sz, unsigned timeout_ms)
{ 
  Mutex_Lock(&self->q->lock);
  { __chan_t *q = self->q;
    if(timeout_ms==0)
      goto_if(Fifo_Is_Empty(q->fifo),NoPeek);
    goto_if(Fifo_Is_Empty(q->fifo) && q->nwriters==0,NoPeek); // possibly avoid the resize/copy
    goto_if(CHAN_FAILURE(chan_peek__locked(q,pbuf,sz,timeout_ms)),NoPeek);
  }
  Mutex_Unlock(&self->q->lock);
  // no size change so no notify
  return SUCCESS;
NoPeek:
  Mutex_Unlock(&self->q->lock);
  return FAILURE;
}

unsigned int Chan_Next( Chan *self_, void **pbuf, size_t sz)
{ chan_t *self = (chan_t*)self_;
  switch(self->mode)
  { case CHAN_READ:  return chan_pop (self,pbuf,sz,0,(unsigned)-1); break;
    case CHAN_WRITE: return chan_push(self,pbuf,sz,0,(unsigned)-1); break;
    default:
      CHAN_ERR__INVALID_MODE;
      break;
  }
  return FAILURE;
}

unsigned int Chan_Next_Copy( Chan *self_, void  *buf,  size_t sz) 
{ chan_t *self = (chan_t*)self_;
  switch(self->mode)
  { case CHAN_READ:  return chan_pop (self,&buf,sz,1,(unsigned)-1); break;
    case CHAN_WRITE: return chan_push(self,&buf,sz,1,(unsigned)-1); break;
    default:
      CHAN_ERR__INVALID_MODE;
      break;
  }
  return FAILURE;
}

unsigned int Chan_Next_Try( Chan *self_, void **pbuf, size_t sz)                     
{ chan_t *self = (chan_t*)self_;
  switch(self->mode)
  { case CHAN_READ:  return chan_pop (self,pbuf,sz,0,0); break;
    case CHAN_WRITE: return chan_push(self,pbuf,sz,0,0); break;
    default:
      CHAN_ERR__INVALID_MODE;
      break;
  }
  return FAILURE;
}

unsigned int Chan_Next_Copy_Try( Chan *self_, void  *buf,  size_t sz) 
{ chan_t *self = (chan_t*)self_;
  switch(self->mode)
  { case CHAN_READ:  return chan_pop (self,&buf,sz,1,0); break;
    case CHAN_WRITE: return chan_push(self,&buf,sz,1,0); break;
    default:
      CHAN_ERR__INVALID_MODE;
      break;
  }
  return FAILURE;
}

unsigned int Chan_Next_Timed( Chan *self_, void **pbuf, size_t sz, unsigned timeout_ms )
{ chan_t *self = (chan_t*)self_;
  switch(self->mode)
  { case CHAN_READ:  return chan_pop (self,pbuf,sz,0,timeout_ms); break;
    case CHAN_WRITE: return chan_push(self,pbuf,sz,0,timeout_ms); break;
    default:
      CHAN_ERR__INVALID_MODE;
      break;
  }
  return FAILURE;
}


// ----
// Peek
// ----
//
// Requires a read mode channel.
//
unsigned int Chan_Peek( Chan *self_, void **pbuf, size_t sz )
{ chan_t *self = (chan_t*)self_; 
  return chan_peek(self,pbuf,sz,(unsigned)-1);
}

unsigned int Chan_Peek_Try( Chan *self_, void **pbuf, size_t sz )
{ chan_t *self = (chan_t*)self_; 
  return chan_peek(self,pbuf,sz,0);
}

unsigned int Chan_Peek_Timed ( Chan *self_, void **pbuf, size_t sz, unsigned timeout_ms )
{ chan_t *self = (chan_t*)self_;   
  return chan_peek(self,pbuf,sz,timeout_ms);
}


// -----------------
// Memory management
// -----------------

#define FIFO(e) (((chan_t*)(e))->q->fifo)
int Chan_Is_Full( Chan *self )
{ return Fifo_Is_Full( FIFO(self) );
}

int Chan_Is_Empty( Chan *self )
{ return Fifo_Is_Empty( FIFO(self) );
}

inline void Chan_Resize( Chan* self, size_t nbytes)
{ Fifo_Resize( FIFO(self),nbytes );
}


void* Chan_Token_Buffer_Alloc( Chan *self )
{ return Fifo_Alloc_Token_Buffer(FIFO(self));
}

void* Chan_Token_Buffer_Alloc_And_Copy( Chan *self, void *src )
{ Fifo *fifo = FIFO(self);
  size_t sz  = Fifo_Buffer_Size_Bytes(fifo);
  void *buf  = Fifo_Alloc_Token_Buffer(fifo);
  memcpy(buf,src,sz);
  return buf;
}

void Chan_Token_Buffer_Free( void *buf )
{ Fifo_Free_Token_Buffer(buf);
}

inline
size_t Chan_Buffer_Size_Bytes( Chan *self )
{ return Fifo_Buffer_Size_Bytes(FIFO(self));
} 

inline
size_t Chan_Buffer_Count( Chan *self )
{ return Fifo_Buffer_Count(FIFO(self));
} 

inline Chan* Chan_Id( Chan *self )
{ return ((chan_t*)self)->q;
}

