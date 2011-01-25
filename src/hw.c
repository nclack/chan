#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include <math.h>

#define N 10
#define KER work

void* work(void *tid_)
{ long tid=(long)tid_;
  printf("Hello thread %ld.\n",tid);
  pthread_exit(NULL);
}
void* work2(void *tid_)
{ long tid=(long)tid_;
  int i;
  double r=0.0;
  sleep(3);
  for(i=0;i<10000;++i)
    r=r+sin(i)*tan(i);
  printf("Hello thread %ld.\n",tid);
  pthread_exit(NULL);
}

int main(int argc,char* argv[])
{
  pthread_t threads[N];
  int rc;
  long t;
  for(t=0;t<N;++t)
  { printf("Create thread %ld.\n",t);
    rc = pthread_create(threads+t,NULL,KER,(void*)t);
    assert(rc==0);
  }
  pthread_exit(NULL);
}
