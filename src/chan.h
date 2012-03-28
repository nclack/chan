/** \file
    \ref Chan interface
    \author Nathan Clack <clackn@janelia.hhmi.org>
    \date   Feb. 2011
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
       Chan  *Chan_Open       ( Chan *self, ChanMode mode);             ///< does ref counting and access type
       int    Chan_Close      ( Chan *self);                            ///< does ref counting

extern Chan  *Chan_Id         ( Chan *self);

unsigned Chan_Get_Ref_Count         ( Chan* self);
void     Chan_Wait_For_Ref_Count    ( Chan* self, size_t n);
void     Chan_Wait_For_Writer_Count ( Chan* self,size_t n);
void     Chan_Wait_For_Have_Reader  ( Chan* self);
void     Chan_Set_Expand_On_Full    ( Chan* self, int  expand_on_full); ///< default: no expand


unsigned int Chan_Next         ( Chan *self,  void **pbuf, size_t sz); ///< Push or pop next item.  May block the calling thread.
unsigned int Chan_Next_Copy    ( Chan *self,  void  *buf,  size_t sz); ///< Push or pop a copy.  May block the calling thread.  
unsigned int Chan_Next_Try     ( Chan *self,  void **pbuf, size_t sz); ///< Push (or pop), but never block.  If the queue is full (or empty) immediately return failure.
unsigned int Chan_Next_Copy_Try( Chan *self_, void  *buf,  size_t sz); ///< Same as Chan_Next_Try(), but pushes or pops a copy.  Will not block.
unsigned int Chan_Next_Timed   ( Chan *self,  void **pbuf, size_t sz,   unsigned timeout_ms); ///< Just like Chan_Next(), but any waiting is limited by the timeout.

unsigned int Chan_Peek       ( Chan *self, void **pbuf, size_t sz);
unsigned int Chan_Peek_Try   ( Chan *self, void **pbuf, size_t sz);
unsigned int Chan_Peek_Timed ( Chan *self, void **pbuf, size_t sz, unsigned timeout_ms);


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
