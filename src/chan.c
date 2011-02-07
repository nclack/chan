#include <stdio.h>   // for printf used in reporting errors, etc...
#include <string.h>
#include "config.h"
#include "thread.h"
#include "chan.h"
#include "fifo.h"

#define SUCCESS (0) 
#define FAILURE (1)

//#define DEBUG_CHAN

//////////////////////////////////////////////////////////////////////
//  Logging    ///////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
#define chan_warning(...) printf(__VA_ARGS__);
#define chan_error(...)   do{fprintf(stderr,__VA_ARGS__);exit(-1);}while(0)

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

#define CHAN_ERR__INVALID_MODE chan_error("Error: At %s(%d)"ENDL \
                                          "Channel has invalid read/write mode."ENDL, \
                                          __FILE__,__LINE__)

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
  u32 abort_wait;
  u32 flush;
  
  Mutex              lock;
  Condition          notfull;  //predicate: not full  || abort || expand_on_full
  Condition          notempty; //predicate: not empty || abort || no writers 
  Condition          changedRefCount; //predicate: refcount != n
  Condition          haveWriter; //predicate: nwriters>0

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
  { Chan_Assert(c=calloc(1,sizeof(__chan_t)));
    c->fifo = fifo;
    c->lock = MUTEX_INITIALIZER;
    Condition_Initialize(&c->notfull);
    Condition_Initialize(&c->notempty);
    Condition_Initialize(&c->changedRefCount);
    Condition_Initialize(&c->haveWriter);
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
  { Chan_Assert(c=malloc(sizeof(chan_t)));
    c->q = q;
    c->mode = CHAN_NONE;
  }
  return (Chan*)c;
}

// must be called from inside a lock
chan_t* incref(chan_t *c)
{ chan_t *n;
  ++(c->q->ref_count);
  goto_if_not(n=malloc(sizeof(chan_t)),ErrorAlloc);
  memcpy(n,c,sizeof(chan_t));
  chan_debug("Refs: %d"ENDL,c->q->ref_count);
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
  chan_debug("Refs: %d"ENDL,c->q->ref_count);
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
  { case CHAN_READ:  ++(n->q->nreaders); break;
    case CHAN_WRITE: 
      ++(n->q->nwriters);
      n->q->flush=0;
      Condition_Notify_All(&n->q->haveWriter);
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
  Mutex_Lock(&self->q->lock);
  { __chan_t *q = self->q;
    switch(self->mode)
    { case CHAN_READ:  
        Chan_Assert( (--(q->nreaders))>=0 ); break;
        if(q->nreaders==0)
          q->flush=0;
      case CHAN_WRITE:
        Chan_Assert( (--(q->nwriters))>=0 );
        Condition_Notify_All(&q->haveWriter);
        notify = (q->nwriters==0);
        if(notify)
          q->flush=1;
        break;
    }
  }
  Mutex_Unlock(&self->q->lock);
  if(notify)
    Condition_Notify_All(&self->q->notempty);
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
  while(Fifo_Is_Full(q->fifo) && q->abort_wait==0 && q->expand_on_full==0)
    Condition_Wait(&q->notfull,&q->lock); // TODO: use timed wait?
  if(q->abort_wait)
    return FAILURE;
  if(FIFO_SUCCESS(Fifo_Push(q->fifo,pbuf,sz,q->expand_on_full)))
    return SUCCESS;
  return FAILURE;
}

inline int _pop_bypass_wait(__chan_t *q)
{ 
  return q->nwriters==0 && q->flush;  
}

unsigned int chan_pop__locked(__chan_t *q, void **pbuf, size_t sz, unsigned timeout_ms)
{ int starved;
  while(Fifo_Is_Empty(q->fifo) && q->abort_wait==0 && !_pop_bypass_wait(q))
    Condition_Wait(&q->notempty,&q->lock); // TODO: use timed wait?
  starved = Fifo_Is_Empty(q->fifo) && q->nwriters==0;
  if(q->abort_wait || starved)
    return FAILURE;
  if(FIFO_SUCCESS(Fifo_Pop(q->fifo,pbuf,sz)))
    return SUCCESS;
  return FAILURE;
}

unsigned int chan_peek__locked(__chan_t *q, void **pbuf, size_t sz, unsigned timeout_ms)
{ int starved;
  while(Fifo_Is_Empty(q->fifo) && q->abort_wait==0 && q->nwriters>0)
    Condition_Wait(&q->notempty,&q->lock); // TODO:!! use timed wait
  starved = Fifo_Is_Empty(q->fifo) && q->nwriters==0;
  if(q->abort_wait || starved)
    return FAILURE;
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
  Mutex_Unlock(&self->q->lock);
  Condition_Notify(&self->q->notfull);
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
  Condition_Notify(&self->q->notfull);
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
  Chan_Assert(self->mode==CHAN_READ);
  return chan_peek(self,pbuf,sz,(unsigned)-1);
}

unsigned int Chan_Peek_Try( Chan *self_, void **pbuf, size_t sz )
{ chan_t *self = (chan_t*)self_; 
  Chan_Assert(self->mode==CHAN_READ);
  return chan_peek(self,pbuf,sz,0);
}

unsigned int Chan_Peek_Timed ( Chan *self_, void **pbuf, size_t sz, unsigned timeout_ms )
{ chan_t *self = (chan_t*)self_; 
  Chan_Assert(self->mode==CHAN_READ);
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

inline void Chan_Resize_Buffers( Chan* self, size_t nbytes)
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

size_t Chan_Buffer_Size_Bytes( Chan *self )
{ return Fifo_Buffer_Size_Bytes(FIFO(self));
} 

