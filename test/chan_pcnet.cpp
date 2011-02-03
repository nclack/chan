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
#include <gtest/gtest.h>
#include <stdio.h>
#include "thread.h"
#include "chan.h"


#if 0
#define report(...) printf(__VA_ARGS__)
#else
#define report(...)
#endif

inline int max(int* pa, int b)
{ *pa = (b>*pa)?b:*pa;
}


class ChanPCNetTest: public ::testing::Test
{ public:     
    int stop;
    int item;

    int pmax;
    int cmax;
    Chan* chan;
  protected:
    virtual void SetUp()
    { stop=item=pmax=cmax=0;
      chan = Chan_Alloc(16,sizeof(int));
    }

    virtual void TearDown()
    {
      Chan_Close(chan);
    }

    void execnet(ThreadProc *procs,int n);

};

typedef struct _input
{ int   id;
  Chan *chan;
  ChanPCNetTest *test;
} input_t;
#define GETCHAN(e) (((input_t*)(e))->chan)
#define GETID(e)   (((input_t*)(e))->id)
#define GETTEST(e) (((input_t*)(e))->test) 

#define N 9 // storage space - max number of thread procs required for tests
void ChanPCNetTest::execnet(ThreadProc *procs,int n)
{ 
  Thread*  threads[N];
  input_t  inputs[N];

  stop=0;
  { size_t i;
    for(i=0;i<n;++i)
    { inputs[i].id   = i;
      inputs[i].chan = chan;
      inputs[i].test = this;
    }
    for(i=0;i<n;++i)
      threads[i] = Thread_Alloc(procs[i],(void*)(inputs+i));
  }

  //usleep(100);
  Chan_Wait_For_Ref_Count(chan,n+1);
  stop=1;
  { int i=0;
    for(i=0;i<n;++i)
      Thread_Join(threads[i]);
  }
}

void* producer(void* arg)
{ Chan* writer;
  int*  buf;
  int id;
  ChanPCNetTest *test = GETTEST(arg);
  
  id = GETID(arg);
  writer = Chan_Open(GETCHAN(arg),CHAN_WRITE);
  report("Producer %d START"ENDL,id);
  buf = (int*)Chan_Token_Buffer_Alloc(writer);
  // Leave this as a do{}while(); loop to be sensitive
  // to early exit of consumer threads.
  //
  // In normal use you'd probably want to test for stop at the 
  // top of the loop.  Here, we want to gaurantee each producer
  // instanced generates at least one item.
  do
  { buf[0] = InterlockedIncrement(&test->item);
    usleep(1);
    usleep(1);
#pragma omp critical
    {
      max(&test->pmax,buf[0]);
    }
    if(CHAN_FAILURE(Chan_Next(writer,(void**)&buf,sizeof(int))))
      report("Producer %d *** push failed for %d"ENDL,id,buf[0]);
     
  } while(!test->stop);
  Chan_Token_Buffer_Free(buf);
  report("Producer %d exiting"ENDL,id);
  Chan_Close(writer);
  return NULL;
}

void* consumer(void* arg)
{ Chan* reader;
  int*  buf,i,id;
  ChanPCNetTest *test = GETTEST(arg);
  
  id = GETID(arg);
  reader = Chan_Open(GETCHAN(arg),CHAN_READ);
  report("Consumer %d START"ENDL,id);
  buf = (int*)Chan_Token_Buffer_Alloc(reader);
  while(CHAN_SUCCESS(Chan_Next(reader,(void**)&buf,sizeof(int))))
  { 
    usleep(1);
    usleep(1);
#pragma omp critical
    {
      max(&test->cmax,buf[0]);
    }
  } 
  Chan_Token_Buffer_Free(buf);
  report("Consumer %d exiting"ENDL,id);
  Chan_Close(reader);
}

TEST_F(ChanPCNetTest,ManyToMany)
{ 
  ThreadProc procs[] = { 
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
  execnet(procs,sizeof(procs)/sizeof(ThreadProc));
  EXPECT_EQ(pmax,cmax);
}

TEST_F(ChanPCNetTest,OneToOne)
{ 
  ThreadProc procs[] = { 
    consumer,
    producer,
  };
  execnet(procs,sizeof(procs)/sizeof(ThreadProc));
  EXPECT_EQ(pmax,cmax);
}
