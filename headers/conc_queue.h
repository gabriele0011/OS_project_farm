#include "header_file.h"
#include "error_ctrl.h"

//mutex coda concorrente richieste


typedef struct node {
	char* data;
	struct node* next;
	pthread_mutex_t mtx;
}t_queue;

int enqueue(t_queue** queue, char* string);
char* dequeue(t_queue** queue);
void printf_queue(t_queue* queue);
void dealloc_queue(t_queue** queue);

