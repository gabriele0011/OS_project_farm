#include "conc_queue.h"


pthread_mutex_t mtx1 = PTHREAD_MUTEX_INITIALIZER;

int enqueue(t_queue** queue, char* string)
{
	//creazione nuovo nodo + setting
	t_queue* new;
	ec_null_r((new = malloc(sizeof(t_queue))), "cache: malloc cache_writeFile fallita", -1);
	new->next = NULL;
	size_t len_str = strlen(string);
	new->data = calloc(len_str, sizeof(char));
	strncpy(new->data, string, len_str);
	(new->data)[len_str] = '\0';
	
	if (pthread_mutex_init(&(new->mtx), NULL) != 0){
		LOG_ERR(errno, "cache: pthread_mutex_init fallita in cache_create_file");
		free(new);
		exit(EXIT_FAILURE);	
	}

	//collocazione nodo
	mutex_lock(&mtx1, "cache: lock fallita in cache_enqueue");
	//caso 1: cache vuota
	if (*queue == NULL){
		*queue = new;
		mutex_unlock(&mtx1, "cache: unlock fallita in cache_enqueue");
		return 0;
	}
	mutex_unlock(&mtx1, "cache: unlock fallita in cache_enqueue");
	
	mutex_lock(&((*queue)->mtx), "cache: lock fallita in cache_enqueue");
	//caso 2: coda con uno o piu elementi
	if(*queue != NULL){
		new->next = *queue;
		*queue = new;
		mutex_unlock(&((*queue)->mtx), "cache: unlock fallita in cache_enqueue");
		return 0;
	}
	mutex_unlock(&((*queue)->mtx), "cache: unlock fallita in cache_enqueue");
	return 0;
}

char* dequeue(t_queue** queue)
{
	mutex_lock(&mtx1, "cache: lock fallita in cache_enqueue");
	//caso 1: cache vuota
	if (*queue == NULL){
		mutex_unlock(&mtx1, "cache: unlock fallita in cache_enqueue");
		return NULL;
	}
	
	//caso 2: un elemento
	if((*queue)->next == NULL){
		t_queue* aux = *queue;
		*queue = NULL;
		mutex_unlock((&mtx1), "cache: unlock fallita in cache_enqueue");
		size_t len_s = strlen(aux->data);
		char* s = calloc(len_s, sizeof(char));
		strncpy(s, aux->data, len_s);
		s[len_s] = '\0';
		pthread_mutex_destroy(&aux->mtx);
		free(aux->data);
		free(aux);
		return s;
	}
	mutex_unlock(&mtx1, "cache: unlock fallita in cache_enqueue");

	//caso 3: coda con due o piu elementi
	mutex_lock(&((*queue)->mtx), "cache: unlock fallita in cache_enqueue");
	t_queue* prev = *queue;
	mutex_lock(&((*queue)->next->mtx), "cache: unlock fallita in cache_enqueue");
	
	t_queue* curr = (*queue)->next;
	t_queue* aux;
	while (curr->next != NULL){
		//controllo duplicato
		aux = prev;
		prev = curr;
		curr = curr->next;
		mutex_lock(&(curr->mtx), "cache: lock fallita in cache_enqueue");
		mutex_unlock(&(aux->mtx), "cache: unlock fallita in cache_enqueue");

	}
	prev->next = NULL;
	size_t len_s = strlen(aux->data);
	char* s = calloc(len_s, sizeof(char));
	strncpy(s, curr->data, len_s);
	s[len_s] = '\0';
	mutex_unlock(&(prev->mtx), "cache: unlock fallita in cache_enqueue");
	mutex_unlock(&(curr->mtx), "cache: unlock fallita in cache_enqueue");
	pthread_mutex_destroy(&curr->mtx);
	free(curr);
	return s;
}

void printf_queue(t_queue* queue)
{	
	mutex_lock(&mtx1, "cache: lock fallita in cache_enqueue");
	while(queue != NULL){
		printf("%s ", queue->data);
		queue = queue->next;
	}
	mutex_unlock(&mtx1, "cache: unlock fallita in cache_enqueue");
	printf("\n");
	return;
}	

//funzioni di deallocazione coda
void dealloc_queue(t_queue** queue)
{
	mutex_lock(&mtx1, "cache: unlock fallita in cache_enqueue");
	//coda vuota
	if(*queue == NULL){
		mutex_unlock(&mtx1, "cache: unlock fallita in cache_enqueue");
		return;
	}
	//coda non vuota
	t_queue* curr = *queue;
	while(curr != NULL){
		t_queue* temp = curr;
		curr = curr->next;
		free(temp->data);
		free(temp);
	}
	*queue = NULL;
	mutex_unlock(&mtx1, "cache: unlock fallita in cache_enqueue");
	return;
}
