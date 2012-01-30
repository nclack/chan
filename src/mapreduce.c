#include "mapreduce.h"
#include "thread.h"
#include "chan.h"

#define LOG(...) fprintf(stderr,__VA_ARGS__)
#define ENDL "\n"
#define TRY(e,lbl) if(!(e)) {LOG("*** Error [%s]: %s(%d)"ENDL "Expression evaluated as false."ENDL "%s"ENDL),#lbl,__FILE__,__LINE__,#e); goto lbl;}

struct _map_reduce_data
{ void   *data;
  size_t  bytesof_elem;
  size_t  bytesof_data;
};

static int   _nthreads = 8;
static Mutex _lock     = MUTEX_INITIALIZER; // right now, mostly protects access to _nthreads

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
  Mutex_Lock(_lock);
  _nthreads = nthreads;
  Mutex_Unlock(_lock);
}

static Thread **alloc_threads(ThreadProc function,ThreadProc arg, int *nthreads)
{ 
  Thread **ts;
  int i;
  *nthreads = 0;
  Mutex_Lock(_lock);
  TRY(ts = (Thread**)malloc(sizeof(Thread*)*_nthreads),MemoryError);
  for(i=0;i<_nthreads;++i)
    ts[i] = Thread_Alloc(function,arg);
  *nthreads = _nthreads;
MemoryError:
  Mutex_Unlock(_unlock);
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

typedef _map_consumer_args
{ Chan       *q;
  MRFunction *f;
} MapConsumerArgs;

typedef _map_payload
{ void *d;
  void *s;
} Payload;

static void* map_consumer(MapConsumerArgs *args)
{ Chan    *reader;
  Payload *data   = NULL;
  size_t   nbytes = 0;
  reader = Chan_Open(args->q,CHAN_READ);
  TRY( Chan_Next(reader,&data,&nbytes) ,ErrorFailedRead);
  TRY( args->f(data->d,data->s)        ,ErrorFailedApply);
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

  { void *s;
    Payload *data;
    Chan    *writer;
    TRY( data  =(Payload*)Chan_Token_Buffer_Alloc(q), ErrorChanData);
    TRY( writer=Chan_Open(q,CHAN_WRITE),ErrorChanOpen);
    for(s=src.data;s<src.data+bytesof_data;s+=src.bytesof_elem)
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
  Chan_Close(q);
ErrorMemory:
  return result;
}
