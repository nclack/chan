#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#define N 10

void* work(void *tid_)
{ long tid=(long)tid_;
  printf("Hello thread %ld.\n");
  pthread_exit(NULL);
}

int main(int argc,char* argv[])
{
  pthread_t threads[N];
  int rc;
  long t;
  for(t=0;t<N;++t)
  { printf("Create thread %ld.\n",t);
    rc = pthread_create(threads+t,NULL,work,(void*)t);
    assert(rc==0);
  }
  pthread_exit(NULL);
}
