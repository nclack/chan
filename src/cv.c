#include "config.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#define N 4  //Number of threads
#define T 10 //
#define L 12 //Limit

int count = 0;
pthread_mutex_t count_mutex;
pthread_cond_t  count_threshold_cv;

void *inc(void *t)
{
  int i;
  long my_id = (long)t;

  for(i=0;i<T;++i)
  { pthread_mutex_lock(&count_mutex);
    ++count;
    if(count>=L)
    { printf("inc(): thread %ld, count = %d  Threshold reached.",my_id,count);
      pthread_cond_signal(&count_threshold_cv);
      //pthread_cond_broadcast(&count_threshold_cv);
      printf("\tSignal sent.\n");
    }
    printf("inc(): thread %ld, count = %d  Release mutex.\n",my_id,count); 
    pthread_mutex_unlock(&count_mutex);
    //sleep(1);
  }
  pthread_exit(NULL);
}

void* watch(void *t)
{ printf("Starting watch(): thread %ld\n", (long)t);
  pthread_mutex_lock(&count_mutex);
  while(count<L)
  { printf("watch(): thread %ld going into wait...\n",(long)t);
    pthread_cond_wait(&count_threshold_cv,&count_mutex);
    printf("watch(): thread %ld Condition signal received (count=%d).\n",(long)t,count);
    count+=125;
    printf("watch(): thread %ld count @ %d.\n",(long)t,count);
  }
  pthread_mutex_unlock(&count_mutex);
  pthread_exit(NULL);
}

int main(int argc, char *argv[])
{ 
  pthread_t threads[N];
  pthread_mutex_init(&count_mutex,NULL);
  pthread_cond_init(&count_threshold_cv,NULL);
 
  // assume impl creates joinable threads
  pthread_create(&threads[0],NULL,watch,(void*)1);
  pthread_create(&threads[3],NULL,watch,(void*)4);
  pthread_create(&threads[1],NULL,  inc,(void*)2);
  pthread_create(&threads[2],NULL,  inc,(void*)3);

  //join
  { int i;
    for(i=0;i<N;++i)
      pthread_join(threads[i],NULL);
  }
  printf("Done. Count = %d.\n",count);

  pthread_mutex_destroy(&count_mutex);
  pthread_cond_destroy(&count_threshold_cv);
  pthread_exit(NULL);
}
