#pragma once
#include <stdlib.h>
#ifdef __cplusplus
extern "C"{
#endif

typedef void  chan;

typedef enum _chan_mode
{ CHAN_NONE=0,
  CHAN_READ,
  CHAN_WRITE,
  CHAN_MODE_MAX,
} ChanMode;

chan  *Chan_Alloc              ( size_t buffer_count, size_t buffer_size_bytes);
chan  *Chan_Alloc_And_Open     ( size_t buffer_count, size_t buffer_size_bytes, ChanMode mode);
chan  *Chan_Open               ( chan *self, ChanMode mode);                                    // does ref counting and access type
int    Chan_Close              ( chan *self );                                                  // does ref counting

unsigned Chan_Get_Ref_Count      ( chan* self);
void     Chan_Set_Expand_On_Full ( chan* self, int  expand_on_full);                           // default: no expand

// ----
// Next
// ----
//
// Requires a mode to be set.
// For chan's open in read mode, does a pop.
// For chan's open in write mode, does a push.
//
//   *    Overflow                       Underflow
// =====  ============================   ===================
// -      may overwrite or expand        Waits forever
// Copy   may overwrite or expand        Waits forever
// Try    fails immediatly               Fails immediately
// Timed  waits.  Fails after timeout.   Fails after timeout

unsigned int Chan_Next       ( chan *self, void **pbuf, size_t sz); 
unsigned int Chan_Next_Copy  ( chan *self, void  *buf,  size_t sz); 
unsigned int Chan_Next_Try   ( chan *self, void **pbuf, size_t sz);                     
unsigned int Chan_Next_Timed ( chan *self, void **pbuf, size_t sz, unsigned timeout_ms );

// ----
// Peek
// ----
//
// Does not require a mode to be set.
//
unsigned int Chan_Peek       ( chan *self, void **pbuf, size_t sz );
unsigned int Chan_Peek_Try   ( chan *self, void **pbuf, size_t sz );
unsigned int Chan_Peek_Timed ( chan *self, void **pbuf, size_t sz, unsigned timeout_ms );

// -----------------
// Memory management
// -----------------
//
// Resize: when nbytes is less than current size, does nothing

int         Chan_Is_Full                    ( chan *self );
int         Chan_Is_Empty                   ( chan *self );
inline void Chan_Resize_Buffers             ( chan* self, size_t nbytes);

void*       Chan_Token_Buffer_Alloc         ( chan *self );
void*       Chan_Token_Buffer_Alloc_And_Copy( chan *self, void *src );
void        Chan_Token_Buffer_Free          ( void *buf );
size_t      Chan_Buffer_Size_Bytes          ( chan *self );


#define CHAN_SUCCESS(expr) ((expr)==0)
#define CHAN_FAILURE(expr) (!CHAN_SUCCESS(expr))

#ifdef __cplusplus
}
#endif

