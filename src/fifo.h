#pragma once

#include "config.h"

/*
 Ring FIFO
 ---------

 This is a "zero-copy" FIFO queue implimented on a circular store of pointers
 that each address distinct, pre-allocated buffers.  Data is pushed and popped
 with a swap operation that exchanges the pointer to the input buffer with the
 pointer to the head/tail. On a push, the token buffer has been filled with
 data to enqueue.  On a pop, the token buffer is just a token; the contained
 data may be deleted or overwritten
 
 When pushing to a full queue, there are three behaviors defined.  The queue
 is optionally expanded (involves a block copy and a number of mallocs), or
 both the read and write point are advanced around the circular queue 
 overwriting the oldest enqueued data.  A push-try operation will fail
 when the queue is full, resulting in a no-op.  All push functions return
 1 when the queue is full and 0 othewise.

 Popping from an empty queue results in a no op.  The pop will return 1 to 
 indicate the queue was empty.

 The circular store is always sized as a power of two (non-zero).  Expand
 will cause the store to grow exponentially.

 Implimentation is reentrant but not threadsafe.  That is, the interface may be
 used by multiple threads, but synchronization is required for proper use of a
 given fifo instance.

 Interface Notes
 ---------------
 Alloc
   <buffer_count>       The requested number of buffers.  Must be a power of 2.
   <buffer_size_bytes>  The requested size of each buffer.

 Expand
   Add's more buffers to the queue.  Does not resize the buffers.  Sizes the
   queue to the next power of two.

 Resize
   Changes the size of enqueued buffers.  Operates by realloc'ing buffers in
   the "dead" part of the queue and changing the <buffer_size_bytes> property.
   Pop operations police the buffers that get swapped on to the queue to ensure
   under-sized buffers are resized.

 Pop
 Push
 Push_Try
   Return 0 on success.
   Operate by swapping a token buffer passed as an argument.

 Peek
 Peek_At
   Operate by copying data out of the read point into a passed buffer.

*/

#ifdef __cplusplus
extern "C"{
#endif

typedef void Fifo;

Fifo*   Fifo_Alloc   ( size_t buffer_count, size_t buffer_size_bytes );
void    Fifo_Expand  ( Fifo *self );
void    Fifo_Resize  ( Fifo *self, size_t buffer_size_bytes );
void    Fifo_Free    ( Fifo *self );

inline unsigned int Fifo_Pop       ( Fifo *self, void **pbuf, size_t sz);                    //                             *pbuf==NULL ok (allocs)
inline unsigned int Fifo_Peek      ( Fifo *self, void **pbuf, size_t sz);                    // copies, might resize *pbuf, *pbuf==NULL ok (allocs)
inline unsigned int Fifo_Peek_At   ( Fifo *self, void **pbuf, size_t sz, size_t index);      // copies, might resize *pbuf
       unsigned int Fifo_Push      ( Fifo *self, void **pbuf, size_t sz, int expand_on_full);// might resize queue's bufs,  *pbuf==NULL ok (allocs)
inline unsigned int Fifo_Push_Try  ( Fifo *self, void **pbuf, size_t sz);                    // might resize queue's bufs,  *pbuf==NULL ok (allocs)

void*       Fifo_Alloc_Token_Buffer( Fifo *self );

inline unsigned char Fifo_Is_Empty(Fifo *self);
inline unsigned char Fifo_Is_Full (Fifo *self);

#define     FIFO_SUCCESS(expr) ((expr)==0)
#define     FIFO_FAILURE(expr) (!FIFO_SUCCESS(expr))



#ifdef __cplusplus
}
#endif
