#pragma once
#include <stdlib.h>
#include "config.h"
#ifdef __cplusplus
extern "C"{
#endif

typedef void  Chan;

typedef enum _chan_mode
{ CHAN_NONE=0,
  CHAN_READ,
  CHAN_WRITE,
  CHAN_MODE_MAX,
} ChanMode;

       Chan  *Chan_Alloc              ( size_t buffer_count, size_t buffer_size_bytes);
extern Chan  *Chan_Alloc_Copy         ( Chan *chan);
       Chan  *Chan_Open               ( Chan *self, ChanMode mode);                                    // does ref counting and access type
       int    Chan_Close              ( Chan *self );                                                  // does ref counting

unsigned Chan_Get_Ref_Count      ( Chan* self);
void     Chan_Wait_For_Ref_Count ( Chan* self, size_t n);
void     Chan_Wait_For_Writer_Count ( Chan* self,size_t n);
void     Chan_Set_Expand_On_Full ( Chan* self, int  expand_on_full);                           // default: no expand

// ----
// Next
// ----
//
// Requires a mode to be set.
// For Chan's open in read mode, does a pop.  Reads will fail if there
//     are no writers for the channel and the channel is empty.
//     Otherwise, when the channel is empty, reads will block.
// For chan's open in write mode, does a push.  Writes will block when
//     the channel is full.
//
//   *    Overflow                       Underflow
// =====  ============================   ===================
// -      Waits or expands.              Fails if no sources, otherwise waits.
// Copy   Waits or expands.              Fails if no sources, otherwise waits.
// Try    Fails immediately              Fails immediately.
// Timed  Waits.  Fails after timeout.   Fails immediately if no sources, otherwise waits till timeout.

unsigned int Chan_Next         ( Chan *self,  void **pbuf, size_t sz);
unsigned int Chan_Next_Copy    ( Chan *self,  void  *buf,  size_t sz);
unsigned int Chan_Next_Try     ( Chan *self,  void **pbuf, size_t sz);
unsigned int Chan_Next_Copy_Try( Chan *self_, void  *buf,  size_t sz);
unsigned int Chan_Next_Timed   ( Chan *self,  void **pbuf, size_t sz,   unsigned timeout_ms );

// ----
// Peek
// ----
//
// Requires read mode.
//
unsigned int Chan_Peek       ( Chan *self, void **pbuf, size_t sz );
unsigned int Chan_Peek_Try   ( Chan *self, void **pbuf, size_t sz );
unsigned int Chan_Peek_Timed ( Chan *self, void **pbuf, size_t sz, unsigned timeout_ms );

// -----------------
// Memory management
// -----------------
//
// Resize: when nbytes is less than current size, does nothing

int         Chan_Is_Full                    ( Chan *self );
int         Chan_Is_Empty                   ( Chan *self );
void        Chan_Resize                     ( Chan *self, size_t nbytes);

void*         Chan_Token_Buffer_Alloc         ( Chan *self );
void*         Chan_Token_Buffer_Alloc_And_Copy( Chan *self, void *src );
void          Chan_Token_Buffer_Free          ( void *buf );
size_t        Chan_Buffer_Size_Bytes          ( Chan *self );
size_t        Chan_Buffer_Count               ( Chan *self );


#define CHAN_SUCCESS(expr) ((expr)==0)
#define CHAN_FAILURE(expr) (!CHAN_SUCCESS(expr))

#ifdef __cplusplus
}
#endif

