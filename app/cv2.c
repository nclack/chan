/** \file
 * Condition variable test
 *
 * Based on the "Using Conditional Variables" example on msdn
 * http://msdn.microsoft.com/en-us/library/ms686903(v=vs.85).aspx
 *
 * Seems to be a better pattern than the llnl/pthreads example
 * for conditional variables.
 */
#include "config.h"
#include "thread.h"
#include <stdio.h>
#include <stdlib.h>

#define ITER 1000   //max number of items to produce
#define SZ 10
#define PSLEEP  90  //ms
#define CSLEEP 100  //ms
 
#define HERE printf("HERE: Line % 5d File: %s\n",__LINE__,__FILE__)

long buffer[SZ];
long last;
unsigned long qsz;
unsigned long qoff;

unsigned long pitems;
unsigned long citems;

Condition* bufferNotEmpty;
Condition* bufferNotFull;
Mutex*     bufferLock;

int stop;

void* producer(void* p)
{ long id =(long)p;
  //while(1)
  int i;
  for(i=0;i<ITER;++i)
  { long item;
    usleep( drand48()*PSLEEP*1000.0 );
    item = InterlockedIncrement(&last);
    Mutex_Lock(bufferLock);
    while(qsz==SZ && stop==0)
      Condition_Wait(bufferNotFull,bufferLock);

    if(stop)
    { Mutex_Unlock(bufferLock);
      break;
    }

    buffer[(qoff+qsz)%SZ]=item;
    ++qsz;
    ++pitems;

    printf ("Producer %ld: item %2ld, queue size %2lu\r\n", id, item, qsz);
    Mutex_Unlock(bufferLock);
    // Wake waiting consumers
    Condition_Notify(bufferNotEmpty);
  }
  printf("Producer %ld exiting\n",id);
  return NULL;
}

void* consumer(void *p)
{ long id=(long)p;
  while(1)
  { long item;
    Mutex_Lock(bufferLock);
    while( qsz==0 && stop==0 )
      Condition_Wait(bufferNotEmpty,bufferLock);
    if(stop && qsz==0)
    { Mutex_Unlock(bufferLock);
      break;
    }
    // Consumer first available
    item=buffer[qoff];
    --qsz;
    ++qoff;
    ++citems;
    if(qoff==SZ)
      qoff=0;
    printf ("Consumer %ld: item %2ld, queue size %2lu\r\n", id, item, qsz);
    Mutex_Unlock(bufferLock);
    // Wake waiting producers
    Condition_Notify(bufferNotFull);
    // Simulate processing time
    usleep( drand48()*CSLEEP*1000.0 );
  }
  printf("Consumer %ld exiting.\n",id);
  return NULL;
}

#define N 6
int main(int argc,char* argv[])
{ Thread* threads[N];
  Condition c1,c2;
  c1 = CONDITION_INITIALIZER;
  c2 = CONDITION_INITIALIZER;
  bufferNotFull  = &c1; //Condition_Alloc();
  bufferNotEmpty = &c2; //Condition_Alloc();
  bufferLock     = Mutex_Alloc();

  stop=0;
  { size_t i;
    ThreadProc procs[N] = { 
      consumer,
      consumer,
      producer,
      producer,
      consumer,
      producer,
    };
    for(i=0;i<N;++i)
      threads[i] = Thread_Alloc(procs[i],(void*)i);
  }

  sleep(2);

  Mutex_Lock(bufferLock);
  stop=1;
  Mutex_Unlock(bufferLock);

  Condition_Notify_All(bufferNotFull);
  Condition_Notify_All(bufferNotEmpty);

  { int i=0;
    for(i=0;i<N;++i)
      Thread_Join(threads[i]);
  }
  return 0;
}
