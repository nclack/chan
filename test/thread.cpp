#include "thread.h"
#include <gtest/gtest.h>

void* pause_100ms(void* a)  {usleep(10000); return a;}
void* pause_random_10ms(void* a) {usleep(drand48()*10.0*1000.0); return a;}
void* fortytwo(void* a)     {return (void*)42;}
void* identity(void* a)     {return a;}

typedef struct _counter 
{ Mutex *lock;
  int n;
} counter;

void init_counter(counter *c)
{ c->lock = Mutex_Alloc();
  c->n=0;
  ASSERT_NE(c->lock,(void*)NULL);
}
void* inc(void *a)
{ counter *c=(counter*)a;
  int v;
  Mutex_Lock(c->lock);
  v=c->n;
  v++;
  pause_random_10ms(NULL);
  c->n = v;
	Mutex_Unlock(c->lock);
}
void* inc_ctl(void *a)
{ counter *c=(counter*)a;
  int v = c->n;
  v++;
  pause_random_10ms(NULL);
  c->n = v;
}

class ThreadTest:public ::testing::Test
{ 
  protected:
	Thread *pause1_,*fortytwo_;
  virtual void SetUp()
  { pause1_   = Thread_Alloc(pause_100ms,NULL);
    fortytwo_ = Thread_Alloc(pause_100ms,(void*)42);
  }
  virtual void TearDown()
  { Thread_Free(pause1_);
    Thread_Free(fortytwo_);  
  }
};

TEST_F(ThreadTest,Alloc)
{ ASSERT_NE(pause1_,(void*)NULL);
}
TEST_F(ThreadTest,Join)
{ 
  EXPECT_EQ(Thread_Join(fortytwo_),(void*)42);
}
/*
TEST_F(ThreadTest,Futures)
{ Future *f = Thread_Result(fortytwo_);
  ASSERT_NE(f,NULL);
  EXPECT_EQ((int)Future_Get(f),42);
  Future_Free(f);
}
 */
TEST(MutexTest,Inc)
{ Thread *pool[100];
  int i,N=100;
	counter c;
  init_counter(&c);
  EXPECT_EQ(c.n,0);
  for(i=0;i<N;++i)
  { pool[i] = Thread_Alloc(inc,(void*)&c); // <50ms>*100 = 5s
    ASSERT_NE(pool[i],(void*)NULL);
  }
  for(i=0;i<N;++i)
    Thread_Join(pool[i]);
  EXPECT_EQ(c.n,N);
  Mutex_Free(c.lock);
}
TEST(MutexTest,IncControl)
{ Thread *pool[100];
  int i,N=100;
	counter c;
  init_counter(&c);
  EXPECT_EQ(c.n,0);
  for(i=0;i<N;++i)
  { pool[i] = Thread_Alloc(inc_ctl,(void*)&c); // <50ms>*100 = 5s
    ASSERT_NE(pool[i],(void*)NULL);
  }
  for(i=0;i<N;++i)
    Thread_Join(pool[i]);
  EXPECT_NE(c.n,N);
  Mutex_Free(c.lock);
}

#define HERE printf("HERE: Line % 5d File: %s\n",__LINE__,__FILE__)
TEST(MutexTest,RecursiveLockFails)
{ Mutex *m = Mutex_Alloc();
  ASSERT_NE(m,(void*)NULL);
  Mutex_Lock(m);
  ASSERT_DEATH(Mutex_Lock(m),"Detected an attempt to recursively acquire a mutex.*");
  Mutex_Unlock(m);
  Mutex_Free(m);
}
TEST(MutexTest,OrphanUnlockFails)
{ Mutex *m = Mutex_Alloc();
  ASSERT_NE(m,(void*)NULL);
  ASSERT_DEATH(Mutex_Unlock(m),"Detected an attempt to unlock a mutex that hasn't been locked.*");
  Mutex_Free(m);
}
