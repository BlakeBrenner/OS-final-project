#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <stdint.h>

pthread_mutex_t mtx;
uint64_t glbl=0;



void* thread_func(void *arg){
  uint64_t idx = (uint64_t)arg;
  //printf("in thread_");
  //printf("func()!   %ld\n", idx);
  for(int k = 0; k < 10000; k++){
    pthread_mutex_lock(&mtx);  
    glbl++;
    pthread_mutex_unlock(&mtx);
  }

  return (void*)7;
}

int main(){

  pthread_t new_thread[10];
  void * retval = 0;
  pthread_mutex_init(&mtx, NULL);

  for(uint64_t k = 0; k < 10; k++){
    pthread_create(&new_thread[k],
                       NULL,
                       thread_func,
                       (void*)k);
  }
  for(int k = 0; k < 10; k++){
    pthread_join(new_thread[k], &retval);
  }
  //sleep(1);
  printf("done creating thread. glbl = %ld \n", glbl);
  return 0;
}
