//NOTES:
// - looks like there's a slow memory leak (windows only?) in repeated tests.
//   Don't really know why.

//TODO: different networks
//TODO: topologically ordered shutdown test [HARD]
//      [ ] generate random dags
//      [ ] assemble random dags?  this is a little wierd bc chan's are
//          hyperedges - directed, but there's some degeneracy depending on the
//          dag description
//      [ ] record shutdown order
//      [ ] computed expected shutdown order based on dag
//      [ ] ensure topological equivilance

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

inline void mymax(int* pa, int b)
{ *pa = (b>*pa)?b:*pa;
}


#define N 9 // storage space - max number of thread procs required for tests
class ChanPCNetTest: public ::testing::Test
{ public:     
    int stop;
    int item;

    int pmax;
    int ppmax;
    int cmax;
    Chan* chan;
    Chan* chans[N];
  protected:
    virtual void SetUp()
    { stop=item=pmax=ppmax=cmax=0;
      chan = Chan_Alloc(2,sizeof(int));
      for(int i=0;i<N;++i)
        chans[i] = Chan_Alloc(2,sizeof(int));
    }

    virtual void TearDown()
    {
      Chan_Close(chan);
      for(int i=0;i<N;++i)
        Chan_Close(chans[i]);
    }

    void exec_one_chan(ThreadProc *procs,size_t n);
    void execnet(int *graph,int nrows);

};

typedef struct _input
{ int            id;
  Chan          *chan;
  ChanPCNetTest *test;
  struct _input *next;
} input_t;
#define GETCHAN(e)     (((input_t*)(e))->chan)
#define GETNEXT(e)     (((input_t*)(e))->next) 
#define GETNEXTCHAN(e) GETCHAN(GETNEXT(e))
#define GETID(e)       (((input_t*)(e))->id)
#define GETTEST(e)     (((input_t*)(e))->test) 

void ChanPCNetTest::exec_one_chan(ThreadProc *procs,size_t n)
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
  { size_t i=0;
    for(i=0;i<n;++i)
      Thread_Join(threads[i]);
  }
}

void* producer (void* arg);
void* consumer (void* arg);
void* processor(void* arg);

void ChanPCNetTest::execnet(int *graph, int nrows)
{ 
  Thread*  threads[N];
  ThreadProc procs[N];
  input_t  inputs[N];
  int w = nrows;
  int sources[N];
  ASSERT_TRUE(nrows<=N);
  memset(sources,0,N*sizeof(int));

  stop=0;
  { size_t i;
    for(i=0;i<nrows;++i)
    { int nout,nin;
      int j;
      nout=nin=0;
      //only check upper triangle
      //Count ins and outs
      for(j=0;j<nrows;++j){ nout+=graph[i*w+j]; nin +=graph[j*w+i]; }
      ASSERT_FALSE(nout==0 && nin==0);                              // all nodes must connect to something
      ASSERT_FALSE(nout>1);                                         // nodes can have at most one output  :(
      inputs[i].id   = i;
      inputs[i].test = this;
      if(nout==0)                                                   // sink
      { inputs[i].chan = chans[i];
        inputs[i].next = NULL;
        procs[i] = consumer;
      } else if(nin==0)                                             // source
      { for(j=0;j<nrows;++j)
          if(graph[i*w+j])
          { inputs[i].chan = chans[j];
            ++sources[j];
          }
        inputs[i].next=NULL;
        procs[i] = producer;
      } else                                                        // intermediate
      { inputs[i].chan = chans[i];
        for(j=0;j<nrows;++j)
          if(graph[i*w+j])
          { inputs[i].next = inputs+j;
            ++sources[j]; //because input[i].next = input[j].chan = chans[j]
          }
        procs[i]=processor;
      }
    }
    for(i=0;i<nrows;++i)
      threads[i] = Thread_Alloc(procs[i],(void*)(inputs+i));
  }

  for(size_t i=0;i<nrows;++i)
    if(sources[i])
      Chan_Wait_For_Writer_Count(chans[i],sources[i]);
  report(" *** STOP *** "ENDL);
  stop=1;
  { size_t i=0;
    for(i=0;i<nrows;++i)
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
  report("Producer  %d START"ENDL,id);
  buf = (int*)Chan_Token_Buffer_Alloc(writer);
  // Leave this as a do{}while(); loop to be sensitive
  // to early exit of consumer threads.
  //
  // In normal use you'd probably want to test for stop at the 
  // top of the loop.  Here, we want to gaurantee each producer
  // instanced generates at least one item.
  do
  { buf[0] = InterlockedIncrement((long*)&test->item);
    usleep(1);
    usleep(1);
#pragma omp critical
    {
      mymax(&test->pmax,buf[0]);
    }
    if(CHAN_FAILURE(Chan_Next(writer,(void**)&buf,sizeof(int))))
      printf("Producer  %d *** push failed for %d"ENDL,id,buf[0]);
     
  } while(!test->stop);
  Chan_Token_Buffer_Free(buf);
  report("Producer  %d exiting"ENDL,id);
  Chan_Close(writer);
  return NULL;
}

void* consumer(void* arg)
{ Chan* reader;
  int*  buf,id;
  ChanPCNetTest *test = GETTEST(arg);
  
  id = GETID(arg);
  reader = Chan_Open(GETCHAN(arg),CHAN_READ);
  report("Consumer  %d START"ENDL,id);
  buf = (int*)Chan_Token_Buffer_Alloc(reader);
  while(CHAN_SUCCESS(Chan_Next(reader,(void**)&buf,sizeof(int))))
  { 
    usleep(1);
    usleep(1);
#pragma omp critical
    {
      mymax(&test->cmax,buf[0]);
    }
  } 
  Chan_Token_Buffer_Free(buf);
  report("Consumer  %d exiting"ENDL,id);
  Chan_Close(reader);
  return NULL;
}

void* processor(void* arg)
{ Chan *reader,*writer;
  int  *buf,id;
  ChanPCNetTest *test = GETTEST(arg);
  
  id = GETID(arg);
  reader = Chan_Open(GETCHAN(arg),CHAN_READ);
  writer = Chan_Open(GETNEXTCHAN(arg),CHAN_WRITE);
  report("Processor %d START"ENDL,id);
  buf = (int*)Chan_Token_Buffer_Alloc(reader);
  while(CHAN_SUCCESS(Chan_Next(reader,(void**)&buf,sizeof(int))))
  { 
    usleep(1);
    usleep(1);
#pragma omp critical
    {
      mymax(&test->ppmax,buf[0]);
    }
    if(CHAN_FAILURE(Chan_Next(writer,(void**)&buf,sizeof(int))))
      printf("Processor %d *** push failed for %d"ENDL,id,buf[0]);
  } 
  Chan_Token_Buffer_Free(buf);
  report("Processor %d exiting"ENDL,id);
  Chan_Close(writer);
  Chan_Close(reader);
  return NULL;
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
  exec_one_chan(procs,sizeof(procs)/sizeof(ThreadProc));
  EXPECT_EQ(pmax,cmax);
}

TEST_F(ChanPCNetTest,OneToOne)
{ 
  ThreadProc procs[] = { 
    consumer,
    producer,
  };
  exec_one_chan(procs,sizeof(procs)/sizeof(ThreadProc));
  EXPECT_EQ(pmax,cmax);
}

TEST_F(ChanPCNetTest,Chain)
{ 
  int net[] =
  { //0 1 2 3 4 5 6 7 8
      0,1,0,0,0,0,0,0,0, // 0
      0,0,1,0,0,0,0,0,0, // 1
      0,0,0,1,0,0,0,0,0, // 2
      0,0,0,0,1,0,0,0,0, // 3
      0,0,0,0,0,1,0,0,0, // 4
      0,0,0,0,0,0,1,0,0, // 5
      0,0,0,0,0,0,0,1,0, // 6
      0,0,0,0,0,0,0,0,1, // 7
      0,0,0,0,0,0,0,0,0, // 8
  };
  execnet(net,9);
  EXPECT_EQ(pmax,cmax);
  EXPECT_EQ(pmax,ppmax);
}

TEST_F(ChanPCNetTest,Tree)
{ 
  int net[] =
  { //0 1 2 3 4 5 6 7 8
      0,1,0,0,0,0,0,0,0, // 0
      0,0,0,1,0,0,0,0,0, // 1 
      0,0,0,1,0,0,0,0,0, // 2
      0,0,0,0,0,0,1,0,0, // 3
      0,0,0,0,0,0,1,0,0, // 4
      0,0,0,0,0,0,1,0,0, // 5
      0,0,0,0,0,0,0,1,0, // 6
      0,0,0,0,0,0,0,0,1, // 7
      0,0,0,0,0,0,0,0,0, // 8 
  };//*   *   * *
  // topological order should be (0,2,4,5),1,3,6,7,8
  execnet(net,9);
  EXPECT_EQ(pmax,cmax);
  EXPECT_EQ(pmax,ppmax);
}

void *apc(void *q_)
{ typedef struct _T { int a,b,c; } T;
  Chan *q = (Chan*) q_;
  Chan *reader = Chan_Open(q,CHAN_READ);
  T v;
  EXPECT_TRUE(CHAN_SUCCESS(Chan_Next_Copy_Try(reader,&v,sizeof(T)) ));
  return (void*)(v.a*v.b*v.c);
}

TEST_F(ChanPCNetTest,AsynchronousProcedureCallPattern)
{ typedef struct _T { int a,b,c; } T;
  T arg = {1,2,3};
  Chan *q = Chan_Alloc(2,sizeof(T));
  Chan_Set_Expand_On_Full(q,1);
  Chan *writer = Chan_Open(q,CHAN_WRITE);
  Chan_Next_Copy(writer,&arg,sizeof(T));
  Chan_Close(writer);

  Thread *t = Thread_Alloc(apc,(void*)q);
  EXPECT_EQ(6,(size_t) Thread_Join(t));
}
