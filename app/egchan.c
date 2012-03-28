/** \file
 *  A simple example of using \ref Chan used to bang out problems from time to
 *  time.
 */

#include <stdio.h>
#include "thread.h"
#include "chan.h"
#include "config.h"

#define NITEMS 10

//inline int mymax(int* pa, int b);

static inline 
void mymax(int* pa, int b)
{ *pa = (b>(*pa))?b:(*pa);
}

int stop;
int item;

int pmax;
int cmax;

typedef struct _input
{ int   id;
  Chan *chan;
} input_t;

#define GETCHAN(e) (((input_t*)(e))->chan)
#define GETID(e)   (((input_t*)(e))->id)


void* producer(void* arg)
{ Chan* writer;
  int*  buf;
  int id;
  
  id = GETID(arg);
  printf("Producer %d starting"ENDL,id);
  writer = Chan_Open(GETCHAN(arg),CHAN_WRITE);
  buf = (int*)Chan_Token_Buffer_Alloc(writer);
  do
  { buf[0] = InterlockedIncrement(&item);
    usleep(1);
    usleep(1);
    printf("Producer: item %4d [   + ] %d"ENDL, buf[0], id);
#pragma omp critical
    {
      mymax(&pmax,*buf);
    }
    Chan_Next(writer,(void**)&buf,sizeof(int));
  } while(!stop);
  Chan_Token_Buffer_Free(buf);
  Chan_Close(writer);
  printf("Producer %d exiting"ENDL,id);
  return NULL;
}

void* consumer(void* arg)
{ Chan* reader;
  int*  buf,id;
  
  id = GETID(arg);
  printf("Consumer %d starting\n",id);
  reader = Chan_Open(GETCHAN(arg),CHAN_READ);
  buf = (int*)Chan_Token_Buffer_Alloc(reader);
  while(CHAN_SUCCESS(Chan_Next(reader,(void**)&buf,sizeof(int))))
  { //i=InterlockedDecrement(&item);
    printf("Consumer: item %4d [ -   ] %d"ENDL, buf[0], id);
#pragma omp critical
    {
      mymax(&cmax,buf[0]);
    }
  } 
  Chan_Token_Buffer_Free(buf);
  Chan_Close(reader);
  printf("Consumer %d exiting"ENDL,id);
  return NULL;
}

#define N 9
int main(int argc,char* argv[])
{ Chan *chan;
  Thread*  threads[N];
  input_t  inputs[N];

  stop=0;
  item=0;

  chan = Chan_Alloc(16,sizeof(int));

  stop=0;
  { size_t i;
    ThreadProc procs[N] = { 
      consumer,
      consumer,
      consumer,
      consumer,
      producer,
      producer,
      producer,
      producer,
      producer,
    };
    for(i=0;i<N;++i)
    { inputs[i].id   = i;
      inputs[i].chan = chan;
    }
    for(i=0;i<N;++i)
    { threads[i] = Thread_Alloc(procs[i],(void*)(inputs+i));
      usleep(10);
    }
  }

  //usleep(100);
  Chan_Wait_For_Ref_Count(chan,N+1);
  stop=1;
  printf("*** Main: triggered stop ***"ENDL);
  { int i=0;
    for(i=0;i<N;++i)
      Thread_Join(threads[i]);
  }
  Chan_Close(chan);
  printf("Balance: (%d,%d) %s"ENDL,pmax,cmax,(pmax==cmax)?"Ok!":"Mismatch *****");
  return 0;
}
