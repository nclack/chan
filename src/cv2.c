/*
 * From the "Using Conditional Variables" example on msdn
 * http://msdn.microsoft.com/en-us/library/ms686903(v=vs.85).aspx
 *
 * Seems to be a better pattern than the llnl example
 *
 */
#include "config.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#define SZ 10
#define PSLEEP  50  //ms
#define CSLEEP 200  //ms

long buffer[SZ];
long last;
unsigned long qsz;
unsigned long qoff;

unsigned long pitems;
unsigned long citems;

pthread_cond_t  bufferNotEmpty;
pthread_cond_t  bufferNotFull;
pthread_mutex_t bufferLock;

int stop;

void* producer(void* p)
{ long id =(long)p;
  while(1)
  { long item;
    usleep( drand48()*PSLEEP*1000.0 );
    item = InterlockedIncrement(&last);
    pthread_mutex_lock(&bufferLock);

    while(qsz==SZ && stop==0)
      pthread_cond_wait(&bufferNotFull,&bufferLock);

    if(stop)
    { pthread_mutex_unlock(&bufferLock);
      break;
    }

    buffer[(qoff+qsz)%SZ]=item;
    ++qsz;
    ++pitems;

    printf ("Producer %ld: item %2ld, queue size %2lu\r\n", id, item, qsz);
    pthread_mutex_unlock(&bufferLock);
    // Wake waiting consumers
    pthread_cond_signal(&bufferNotEmpty);
  }
  printf("Producer %ld exiting\n",id);
  return NULL;
}

void* consumer(void *p)
{ long id=(long)p;
  while(1)
  { long item;
    pthread_mutex_lock(&bufferLock);
    while( qsz==0 && stop==0 )
      pthread_cond_wait(&bufferNotEmpty,&bufferLock);
    if(stop && qsz==0)
    { pthread_mutex_unlock(&bufferLock);
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
    pthread_mutex_unlock(&bufferLock);
    // Wake waiting producers
    pthread_cond_signal(&bufferNotFull);
    // Simulate processing time
    usleep( drand48()*CSLEEP*1000.0 );
  }
  printf("Consumer %ld exiting.\n",id);
  return NULL;
}

#define N 4
int main(int argc,char* argv[])
{ pthread_t threads[N];
  pthread_cond_init(&bufferNotFull,NULL);
  pthread_cond_init(&bufferNotEmpty,NULL);
  pthread_mutex_init(&bufferLock,NULL);

  stop=0;
  pthread_create(threads+0,NULL,producer,(void*)0);
  pthread_create(threads+1,NULL,consumer,(void*)1);
  pthread_create(threads+2,NULL,consumer,(void*)2);
  pthread_create(threads+3,NULL,producer,(void*)3);

  puts("Press enter to stop...");
  getchar();

  pthread_mutex_lock(&bufferLock);
  stop=1;
  pthread_mutex_unlock(&bufferLock);

  pthread_cond_broadcast(&bufferNotFull);
  pthread_cond_broadcast(&bufferNotEmpty);

  { int i=0;
    for(i=0;i<N;++i)
      pthread_join(threads[i],NULL);
  }
  return 0;
}
