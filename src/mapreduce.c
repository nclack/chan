/** \file
    Map-reduce framework implemented using \ref Chan.
    \section secMapEx map() example

    Take an array of 8-bit RGB tuples from a color image and map to 
    convert to grayscale, outputing an array of doubles.

    The function to be applied is defined:
    \code
    int grayscale(void *_dst, void *_src)
    { double        *dst = (double*) _dst;
      unsigned char *src = (unsigned char*) _src;
      *dst = 0.630*src[0] + 0.310*src[1] + 0.155*src[2];
      return 0;
    }
    \endcode

    Perform the grayscale conversion in parallel over a 512x512 RGB image.
    The output image will be dynamically allocated.
    \code
    MRData result = 
      map(MREmpty(sizeof(double)),
          MRPackage(image,3,512*512*3),
          grayscale);
    // use the result
    MRRelease(&result);
    \endcode

    If the output image has already been allocated:
    \code
    double out[512*512];
    map(MRPackage(out,8,512*512)),
        MRPackage(image,3,512*512*3),
        grayscale);
    // use "out"    
    \endcode
  */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "mapreduce.h"
#include "thread.h"
#include "chan.h"

#define LOG(...) fprintf(stderr,__VA_ARGS__)
#define ENDL "\n"
#define TRY(e,lbl) do { if(!(e)) \
    {LOG("*** Error [%s]: %s(%d)"ENDL \
         "Expression evaluated as false."ENDL \
         "%s"ENDL,#lbl,__FILE__,__LINE__,#e); goto lbl;} } while(0)

struct _map_reduce_data
{ void   *data;
  size_t  bytesof_elem;
  size_t  bytesof_data;
};

static int   _nthreads = 8;
static Mutex _lock     = {0}; // right now, mostly protects access to _nthreads

MRData MRPackage(void *buf, size_t bytesof_elem, size_t bytesof_data)
{ MRData out = {buf,bytesof_elem,bytesof_data};
  return out;
}

MRData MREmpty(size_t bytesof_elem)
{ MRData out = {NULL,bytesof_elem,0};
  return out;
}

void MRData_Release(MRData *mrdata)
{ if(mrdata->data) free(mrdata->data);
  memset(mrdata,0,sizeof(MRData));
}

void MRSetWorkerThreadCount(int nthreads)
{ 
  Mutex_Lock(&_lock);
  _nthreads = nthreads;
  Mutex_Unlock(&_lock);
}

static Thread **alloc_threads(ThreadProc function,void *arg, int *nthreads)
{ 
  Thread **ts;
  int i;
  *nthreads = 0;
  Mutex_Lock(&_lock);
  TRY(ts = (Thread**)malloc(sizeof(Thread*)*_nthreads),MemoryError);
  for(i=0;i<_nthreads;++i)
    ts[i] = Thread_Alloc(function,arg);
  *nthreads = _nthreads;
MemoryError:
  Mutex_Unlock(&_lock);
  return ts;
}

static void free_threads(Thread** ts, int nthreads)
{ int i;
  if(ts)
  { for(i=0;i<nthreads;++i)
      Thread_Free(ts[i]);
    free(ts);
  }
}

typedef struct _map_consumer_args
{ Chan       *q;
  MRFunction  f;
} MapConsumerArgs;

typedef struct _map_payload
{ void *d;
  void *s;
} Payload;

static void* map_consumer(void *args_)
{ Chan    *reader;
  Payload *data   = NULL;
  size_t   nbytes = 0;
  MapConsumerArgs *args = (MapConsumerArgs*) args_;
  reader = Chan_Open(args->q,CHAN_READ);
  TRY( Chan_Next(reader,(void**)&data,nbytes) ,ErrorFailedRead);
  TRY( args->f(data->d,data->s)       ,ErrorFailedApply);
ErrorFailedApply:
  Chan_Token_Buffer_Free(data);
ErrorFailedRead:
  Chan_Close(reader);
  /// \todo How to handle errors/pass them back to map()
  return NULL;
}

/** Allocates \a dst data if necessary, otherwise shrinks \a src to fit.

    There are four cases:
    \li \a dst is ill defined (\c bytesof_elem is 0)
    \li \a dst describes a buffer bigger than *src
    \li \a dst describes a buffer smaller than *src
    \li \a dst is "empty"

    The first case is treated as an error, so the function returns 0.

    The second case requires no work.  Everything is ok, so the function returns 1.

    The third case is treated as an error, so the  function returns 0.

    The last case requires the appropriate space is allocated.  The number of 
    output elements is computed from \a src.  This is then multiplied by the \a dst
    \c bytesof_elem to determine the final buffer size to request from the heap.

    \param src Description of the source buffer.
    \param dst Description of the destination buffer.
    \return 1 on success, otherwise 0
 */
static int maybe_alloc_dst(MRData *dst, MRData *src)
{ size_t bytes_required;
  TRY(dst->bytesof_elem>0,Error);
  bytes_required = dst->bytesof_data * src->bytesof_data/src->bytesof_elem;
  if(dst->data)
    TRY(dst->bytesof_data>=bytes_required,Error);
  else  
    TRY(dst->data=malloc(bytes_required),Error);
  return 1;
Error:
  return 0;
}

MRData map(MRData dst, MRData src, MRFunction f)
{ Thread **ts;
  int nthreads;
  MapConsumerArgs args;
  MRData result = {NULL,0,0};

  TRY( maybe_alloc_dst(&dst,&src), ErrorMemory );

  TRY(args.q = Chan_Alloc(_nthreads,sizeof(Payload)),ErrorMemory);
  args.f = f;
  TRY(ts = alloc_threads(map_consumer,&args,&nthreads),ErrorAllocThreads);

  { void *s,*d;
    Payload *data;
    Chan    *writer;
    TRY( data  =(Payload*)Chan_Token_Buffer_Alloc(args.q), ErrorChanData);
    TRY( writer=Chan_Open(args.q,CHAN_WRITE),ErrorChanOpen);
    for(s=src.data,d=dst.data;s<src.data+src.bytesof_data;s+=src.bytesof_elem,d+=dst.bytesof_elem)
    { data->s = s;
      data->d = d;
      TRY(CHAN_SUCCESS(Chan_Next(writer,(void**)&data,sizeof(void*))),ErrorChanWrite);
    }
ErrorChanWrite:
    Chan_Close(writer);
ErrorChanOpen:
    Chan_Token_Buffer_Free(data);
  }
ErrorChanData:
  free_threads(ts,nthreads);
ErrorAllocThreads:
  Chan_Close(args.q);
ErrorMemory:
  return result;
}
