//TODO: different networks
//FIXME: if consumer starts first, then loop dies
//       Could think of something like fall through happens when a flag is set,
//             otherwise waits ... the "flush" flag
//       - did this:     
//         0. flush flag is false
//         1. when last producer closes, flush flags gets set to true
//         2. when last consumer closes, flush flag is reset to false.
//
//         Still have a problem in this situation
//
//           1. consumer starts
//           2. producer starts
//           3. producer closes
//           4. consumer closes - it emptied the queue, saw it was ok to not
//                                wait, and exited
//           5. producer starts
//           6. producer closes
//
//           It didn't wait for _all_ the producers to start.  The trouble is 
//           that there's an indefinte wait between thread start and the
//           Chan_Open(_,CHAN_WRITE) call.  Pre-opening would solve this 
//           problem, but seems a bit awkward.
//
//           There needs to be sufficient time for all threads to open
//           the channels or there needs to be a wait mechanism to signal
//           that all the expected channels have been opened.
//
//           How would that look:
//
//             wait for reader count (q,n)
//             wait for writer count (q,n)
//
//                impl by waiting on a condition triggered each time the count
//                changes.
//

/* Notes on testing
 *
 *
 * o  test for early consumer shutdown
 *
 *    many-to-many connection
 *    setup consumers first
 *    small delay between thread starts on main thread so consumers have a
 *          chance to hit their Chan_Next call
 *
 * o  failure modes
 *
 *    all produced items are not consumed
 *        items are ints
 *        keep track of max produced item
 *        keep track of max consumed item
 *        increment item on each production cycle ensuring synchronized inc so
 *            every cycle is counted
 *        compare max's at end to ensure they're equal
 *
 *    deadlock
 *        [ ] need a timed thread join
 *        Detect using timeout on thread join. (no timed join in pth)
 *        Normally threads should quit t seconds after stop flag is set,
 *        exceeding a time e>t (e is the wait time) indicates a deadlock.
 *
 */

#include <stdio.h>
#include "thread.h"
#include "chan.h"
#include "config.h"

#define NITEMS 10

//inline int mymax(int* pa, int b);

inline 
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
    CHAN_SUCCESS(Chan_Next(writer,(void**)&buf,sizeof(int)));
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
