#include "header_file.h"
#include "aux_function.h"
#include "global_var.h"
#include "conc_queue.h"

int send_res(long int result, char* path);
int thread_func2(char* path);
void* thread_func1(void *arg);
pthread_t* create_pool_worker();

