#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#define data_size 4096*256
#define thd_num 4
#define block_size (data_size/thd_num)

//int a[data_size];

void SFP_TaskStart(int i) {
}

void SFP_TaskEnd(int i) {
}

void roi_begin() {
  //printf("begin\n");
}

void roi_end() {
  //printf("end\n");
}

int *pid;

void* worker(void* i) {
  int id = *(int*)i;
  int y = 0;
  int a[data_size];

  SFP_TaskStart(id);
  roi_begin();
  for(y=0;y<data_size;y++) {
    if((y/block_size) == id) {
      a[y] = 1;
    }
  }
  roi_end();
  SFP_TaskEnd(id);

  return NULL;
}
  
int main() {

  int i;

  //printf("a is at %p to %p\n", a, a+data_size);

  pthread_t* p = (pthread_t*)malloc(thd_num*sizeof(pthread_t));
  pid = (int*)malloc(thd_num*sizeof(int));

  for(i=0;i<thd_num;i++) {
    pid[i] = i;
    pthread_create(&p[i], NULL, worker, &pid[i]);
  }

  for(i=0;i<thd_num;i++) {
    pthread_join(p[i], NULL);
  }

  free(p);
  free(pid);
  return 0;
}
