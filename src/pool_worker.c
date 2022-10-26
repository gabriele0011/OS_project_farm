#include "pool_worker.h"

extern t_queue* conc_queue;

int send_res(long int result, char* path)
{
	//printf("(send_res) in corso\n"); //DEBUG
	mutex_lock(&op_mtx, "(pool_worker) send_res");
	long int* long_buf = NULL;
	long_buf = (long int*)malloc(sizeof(long int));
	size_t* sizet_buf = NULL;
	sizet_buf = (size_t*)malloc(sizeof(size_t));
	
	//comunica: tipo operazione (cod.2) invio result+path
	*sizet_buf = SEND_RES;
	write_n(fd, (void*)sizet_buf, sizeof(size_t));
	//riceve: conferma ricezione
	read_n(fd, (void*)sizet_buf, sizeof(size_t));
	if(*sizet_buf != 0) goto sr_clean;
	
	//comunica: result
	*long_buf = result;
	write_n(fd, (void*)long_buf, sizeof(long int));
	//riceve: conferma ricezione
	read_n(fd, (void*)sizet_buf, sizeof(size_t));
	if(*sizet_buf != 0) goto sr_clean;

	//comunica: len file name
	size_t len_s = strlen(path);
	*sizet_buf = len_s;
	write_n(fd, (void*)sizet_buf, sizeof(size_t));
	//riceve: conferma ricezione
	read_n(fd, (void*)sizet_buf, sizeof(size_t));
	if(*sizet_buf != 0) goto sr_clean;

	//comunica: file name
	write_n(fd, (void*)path, sizeof(char)*len_s);
	//riceve: conferma ricezione
	read_n(fd, (void*)sizet_buf, sizeof(size_t));
	if(*sizet_buf != 0) goto sr_clean;
	
	mutex_unlock(&op_mtx, "(pool_worker) send_res");
	
	//deallocazioni
	if (long_buf) free(long_buf);
	if (sizet_buf) free(sizet_buf);
	//printf("send_res eseguita\n"); //DEBUG
	return 0;

	sr_clean:
	mutex_unlock(&op_mtx, "(pool_worker) send_res");
	if (long_buf) free(long_buf);
	if (sizet_buf) free(sizet_buf);
	return -1;
}

void thread_func2(char* path)
{
	//printf("(thread_func2)\n"); //DEBUG
	//1. leggere dal disco il contenuto dell'intero file
	//2. effettuare il calcolo sugli elementi contenuti nel file
	//3. inviare il risultato al processo collector tramite il socket insieme al nome del file
	FILE *fd;
  	long int x;
 	int res;
	
	//apre il file in lettura
	fd = fopen(path, "r");
	if (fd == NULL) {
   		perror("(pool_worker) fopen");
   		exit(EXIT_FAILURE);
  	}

	//calcolo dimensione del file
	struct stat s;
	if (lstat(path, &s) == -1){
      	LOG_ERR(errno, "(pool_worker) lstat");
      	exit(EXIT_FAILURE);
    }
	int N = s.st_size / sizeof(long int); //num. long int in un file di s.st_size bytes
	long int result = 0;
	size_t i = 0;
	//ciclo di lettura
	while (N > 0){	
    		res = fread(&x, sizeof(long int), 1, fd);
    		if( res != 1 ) break;
		result = result + (i * x);
		i++; N--;
    	//printf("%ld\n", x); //DEBUG
  	}
	//printf("(thread_func2) result=%ld\n", result); //DEBUG
	
	//chiusura file
  	if (fclose(fd) == -1){
		LOG_ERR(errno, "(pool_worker) fclose");
	}

	//invia result+path al collector
	if (send_res(result, path) == -1){
		exit(EXIT_FAILURE);
	}
	return;
}

void* thread_func1(void *arg)
{
	//printf("thread = %d\n", gettid());
	int err;
	char* buf = NULL;

	while (1){
		mutex_lock(&mtx, "(pool_worker) lock ");
		//pop richiesta dalla coda concorrente
		while ( !(buf = (char*)dequeue(&conc_queue)) && !child_term){	
			if ((err = pthread_cond_wait(&cv, &mtx)) != 0){
				LOG_ERR(err, "(pool_worker) phtread_cond_wait");
				exit(EXIT_FAILURE);
			}
		}
		q_curr_capacity--;
		mutex_unlock(&mtx, "(pool_worker) unlock");
		
		//uscita 
		if (child_term) break;
		
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
			LOG_ERR(err, "(pool_worker) pthread_create");
			return NULL;
		}
    }
    return thread_workers_arr;
    }