#include "header_file.h"
#include "aux_function.h"
#include "global_var.h"
#include "conc_queue.h"

extern pthread_mutex_t op_mtx;

int send_res(long int result, char* path);
void thread_func2(char* path);
void* thread_func1(void *arg);
pthread_t* create_pool_worker();

