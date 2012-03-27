/** \file
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

  */
#pragma once
#include <stdlib.h>

#ifdef __cplusplus
extern "C"{
#endif

typedef void  Chan;

typedef enum _chan_mode
{ CHAN_NONE=0,
  CHAN_PEEK=0,
  CHAN_READ,
  CHAN_WRITE,
  CHAN_MODE_MAX,
} ChanMode;

       Chan  *Chan_Alloc      ( size_t buffer_count, size_t buffer_size_bytes);
extern Chan  *Chan_Alloc_Copy ( Chan *chan);
       Chan  *Chan_Open       ( Chan *self, ChanMode mode);                                    // does ref counting and access type
       int    Chan_Close      ( Chan *self);                                                  // does ref counting

extern Chan  *Chan_Id         ( Chan *self);

unsigned Chan_Get_Ref_Count         ( Chan* self);
void     Chan_Wait_For_Ref_Count    ( Chan* self, size_t n);
void     Chan_Wait_For_Writer_Count ( Chan* self,size_t n);
void     Chan_Wait_For_Have_Reader  ( Chan* self);
void     Chan_Set_Expand_On_Full    ( Chan* self, int  expand_on_full);                           // default: no expand

/** \file
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
*/

unsigned int Chan_Next         ( Chan *self,  void **pbuf, size_t sz); ///< Push or pop next item.  May block the calling thread.
unsigned int Chan_Next_Copy    ( Chan *self,  void  *buf,  size_t sz); ///< Push or pop a copy.  May block the calling thread.  
unsigned int Chan_Next_Try     ( Chan *self,  void **pbuf, size_t sz); ///< Push (or pop), but never block.  If the queue is full (or empty) immediately return failure.
unsigned int Chan_Next_Copy_Try( Chan *self_, void  *buf,  size_t sz); ///< Same as Chan_Next_Try(), but pushes or pops a copy.  Will not block.
unsigned int Chan_Next_Timed   ( Chan *self,  void **pbuf, size_t sz,   unsigned timeout_ms); ///< Just like Chan_Next(), but any waiting is limited by the timeout.

/** \file
    \section peek Peek Functions

    The Chan_Peek() functions behave very similarly to the Chan_Next() family.
    However, Chan_Peek() does not alter the queue.  The message at the end
    of the queue is copied into supplied buffer.  The buffer may be resized
    to fit the message.

    The Chan_Peek() functions do not require a \ref Chan to be opened in any
    particular mode.  However, the \ref Chan must be opened with Chan_Open().
    The \ref CHAN_PEEK mode is recommended for readability.
*/ 
unsigned int Chan_Peek       ( Chan *self, void **pbuf, size_t sz);
unsigned int Chan_Peek_Try   ( Chan *self, void **pbuf, size_t sz);
unsigned int Chan_Peek_Timed ( Chan *self, void **pbuf, size_t sz, unsigned timeout_ms);

/** \file
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

int         Chan_Is_Full                    ( Chan *self);
int         Chan_Is_Empty                   ( Chan *self);
extern void Chan_Resize                     ( Chan *self, size_t nbytes);

void*       Chan_Token_Buffer_Alloc         ( Chan *self);
void*       Chan_Token_Buffer_Alloc_And_Copy( Chan *self, void *src);
void        Chan_Token_Buffer_Free          ( void *buf );
extern size_t Chan_Buffer_Size_Bytes        ( Chan *self);
extern size_t Chan_Buffer_Count             ( Chan *self);


#define CHAN_SUCCESS(expr) ((expr)==0)
#define CHAN_FAILURE(expr) (!CHAN_SUCCESS(expr))

#ifdef __cplusplus
}
#endif
/** \file
    \author Nathan Clack <clackn@janelia.hhmi.org>
    \date   Feb. 2011
*/
