#include "pool_worker.h"

extern t_queue* conc_queue;

int send_res(long int result, char* path)
{
	//printf("(send_res) in corso\n"); //DEBUG
	long int* buf = NULL;
	buf = (long int*)malloc(sizeof(long int));
	
	mutex_lock(&op_mtx, "send_res");

	//invia: tipo operazione 2=invio result+path
	*buf = 2;
	writen(fd, buf, sizeof(size_t));
	//riceve: conferma ricezione
	readn(fd, buf, sizeof(size_t));
	if(*buf != 0) goto sr_clean;
	
	//invia: result
	*buf = result;
	writen(fd, buf, sizeof(long int));
	//riceve: conferma ricezione
	readn(fd, buf, sizeof(size_t));
	if(*buf != 0) goto sr_clean;

	//invia: len file name
	size_t len_s = strlen(path);
	*buf = len_s;
	writen(fd, buf, sizeof(size_t));
	//riceve: conferma ricezione
	readn(fd, buf, sizeof(size_t));
	if(*buf != 0) goto sr_clean;

	//invia file name
	write(fd, path, sizeof(char)*len_s);
	//riceve: conferma ricezione
	readn(fd, buf, sizeof(size_t));
	if(*buf != 0) goto sr_clean;
	
	mutex_unlock(&op_mtx, "send_res");
	//printf("send_res eseguita\n"); //DEBUG
	if (buf) free(buf);
	return 0;

	sr_clean:
	mutex_unlock(&op_mtx, "send_res");
	if (buf) free(buf);
	return -1;
}

int thread_func2(char* path)
{
	//1. leggere dal disco il contenuto dell'intero file
	//2. effettuare il calcolo sugli elementi contenuti nel file
	//3. inviare il risultato al processo collector tramite il socket insieme al nome del file
	//printf("(thread_func2)\n"); //DEBUG
	FILE *fd;
  	long int x;
 	int res;
	
	//apre il file in lettura
	fd = fopen(path, "r");
	if (fd == NULL) {
   		perror("Errore in apertura del file");
   		exit(EXIT_FAILURE);
  	}

	//dimensione del file
	struct stat s;
	if (lstat(path, &s) == -1){
      	LOG_ERR(errno, "lstat");
      	return -1;
    }
	int N = s.st_size / sizeof(long int); //num. long int in un file di s.st_size byte
	long int result = 0;
	size_t i = 0;
	//ciclo di lettura
	while (N > 0){	
    		res = fread(&x, sizeof(long int), 1, fd);
    		if( res != 1 ) break;
		result = result + (i * x);
		i++;
		N--;
    	//printf("%ld\n", x);
  	}
	//printf("(thread_func2) result=%ld\n", result); //DEBUG
	//chiude il file
  	fclose(fd);

	if (send_res(result, path) == -1){
		return -1;
	}
	return 0;
}

void* thread_func1(void *arg)
{
	printf("thread = %d\n", gettid());
	//printf_queue(conc_queue);
	int err;
	char* buf = NULL;

	while (1){
		mutex_lock(&mtx, "thread_func1: lock fallita");
		//pop richiesta dalla coda concorrente
		while ( !(buf = (char*)dequeue(&conc_queue)) && !closing ){	
			if ((err = pthread_cond_wait(&cv, &mtx)) != 0){
				LOG_ERR(err, "thread_func1: phtread_cond_wait fallita");
				exit(EXIT_FAILURE);
			}
		}
		//printf("queue_capacity=%zu\n", queue_capacity); //DEBUG
		queue_capacity--;
		mutex_unlock(&mtx, "thread_func1: unlock fallita");
		if (closing) break;
		//funzione che opera sul file
		thread_func2(buf);
		if(buf) free(buf);
		buf = NULL;
	}
	if (buf) free(buf);
	return (void*)0;
}

pthread_t* create_pool_worker()
{
	int err;
	///////////////// THREAD POOL  /////////////////
	pthread_t* thread_workers_arr = NULL;
	thread_workers_arr = (pthread_t*)calloc(sizeof(pthread_t), n_thread);
	//pthread_t thread_workers_arr[n_thread];
	for(int i = 0; i < n_thread; i++){
		if ((err = pthread_create(&(thread_workers_arr[i]), NULL, thread_func1, NULL)) != 0){    
			LOG_ERR(err, "pthread_create in server_manager");
			return NULL;
		}
    }
    return thread_workers_arr;
    }