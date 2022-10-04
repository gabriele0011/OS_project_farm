#include "header_file.h"
#include "error_ctrl.h"
#include "aux_function.h"
#include "list.h"
#include "conc_queue.h"

//correzioni: 
//1. controlla che in take_from_dir si consideri il path dei file prelevati dalla directory

pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cv = PTHREAD_COND_INITIALIZER;
pthread_mutex_t op_mtx = PTHREAD_MUTEX_INITIALIZER;

//global variables
size_t n_thread = 4;
size_t q_len = 8;
char* dir_name = NULL;
size_t delay = 0;
node* files_list = NULL;
size_t queue_capacity = 0;
int fd_skt;
t_queue* conc_queue = NULL;
size_t tot_files = 0;
//SEGNALI
//variables
volatile sig_atomic_t sig_usr1 = 0;
volatile sig_atomic_t closing = 0;


//handlers
static void handler_sigquit(int signum){
	closing = 1;
	return;
}
static void handler_sigint(int signum){
	closing = 1;
	return;
}
static void handler_sighup(int signum){
	closing = 1;
	return;
}
static void handler_sigterm(int signum){
	closing = 1;
	return;
}
static void handler_sigusr1(int signum){
	sig_usr1 = 1;
	return;
}

//FUNC
static int file_check(char* path)
{
      //verifica se è un file regolare
      struct stat s;
      if (lstat(path, &s) == -1){
           //LOG_ERR(errno, "lstat");
            return -1;
      }
      //controllo file regolare
      if(S_ISREG(s.st_mode) != 0){ tot_files++; return 0; }
      else return -1;
}

static void take_from_dir(const char* dirname)
{
	//passo 1: apertura dir
	DIR* d;
	if ((d = opendir(dirname)) == NULL){ 
            LOG_ERR(errno, "opendir in take_from_dir"); 
            return; 
      }
	printf("take_from_dir:	dir '%s' aperta\n", dirname); //DEBUG
	//passo 2: esplorazione dir
	struct dirent* entry;
	errno = 0;
	while (errno == 0 && ((entry = readdir(d)) != NULL)) {
		//passo 3 : aggiornamento path
		char path[PATH_MAX];
		memset((void*)path, '\0', (sizeof(char)*PATH_MAX));
		snprintf(path, PATH_MAX , "%s/%s", dirname, entry->d_name);
		//passo 4 caso 1: controlla che se si tratti di una dir
		if (is_directory(path)){	
			//controlla che non si tratti della dir . o ..
			if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0)
				//visita ricorsiva su nuova directory
				take_from_dir(path);
		//passo 4 caso 2: inserimento del file in lista
		}else{
			//printf("take_from_dir su %s\n", path); DEBUG
			if(file_check(path) != -1)
            	insert_node(&files_list, path);
		}
	}
	//chiudere la directory
	if (closedir(d) == -1){ LOG_ERR(errno, "closedir in take_from_dir"); }
	return;
}
static int parser(int dim, char** array)
{		
    dir_name = NULL;
    //CICLO PARSING: si gestiscono i comandi di setting del server -n, -q, -d, -t + lista di files
	int i = 0;
	while (++i < dim){
        //CASO -n <_>
		if (is_opt(array[i], "-n")){
			if (is_argument(array[i+1])){
                if ((n_thread = is_number(array[++i])) == -1){
				    LOG_ERR(EINVAL, "(main) argomento -n non numerico");
				    return -1;
             	}
			}else{
                LOG_ERR(EINVAL, "(main) argomento -n mancante");
				return -1;
            }
			printf("DEBUG _ = %zu\n", n_thread); //DEBUG
            //i++;
		}

        //CASO -q <qlen>
		if (is_opt(array[i], "-q")){
			if (is_argument(array[i+1])){
                if ((q_len = is_number(array[++i])) == -1){
				    LOG_ERR(EINVAL, "(main) argomento -q non numerico");
				    return -1;
                }
            }else{
                LOG_ERR(EINVAL, "(main) argomento -q mancante");
				return -1;
            }
		    printf("DEBUG q_len = %zu\n", q_len); //DEBUG
            //i++;
		}
            
        //CASO -d <directory_name>
		if (is_opt(array[i], "-d")){
			//argomento obbligatorio
			if (is_argument(array[i+1])) 
				dir_name = array[++i];
			else{
				LOG_ERR(EINVAL, "(main) argomento -d mancante");
				return -1;
			}
            printf("DEBUG: dir_name = %s\n", dir_name); //DEBUG
            //i++;
		}
		
		//CASO -t <delay>
		if (is_opt(array[i], "-t")){
			if (is_argument(array[i+1])){
                if ((delay = is_number(array[++i])) == -1){
				    LOG_ERR(EINVAL, "(main) argomento -t non numerico");
				    return -1;
                }
			}else{
                LOG_ERR(EINVAL, "(main) argomento -t mancante");
				return -1;
			}
		    printf("DEBUG: delay = %zu\n", delay); //DEBUG
            //i++;
		}

        //CASO lista di file
		if (file_check(array[i]) != -1)
    		insert_node(&files_list, array[i]);
	}

	//controllo dipendenze
    if (files_list == NULL && dir_name == NULL){
    	LOG_ERR(EINVAL, "(main) nessun file in input");
    	return -1;
    }

    if(dir_name != NULL) take_from_dir(dir_name);
	printf("DEBUG - lista dei file acquisiti\n"); //DEBUG
    print_list(files_list); //DEBUG
	
	return 0;
}

static int send_res(long int result, char* path)
{
	printf("(send_res)\n");
	//inviare il risultato al processo collector tramite socket
	mutex_lock(&op_mtx, "send_res");
	long int* buf = NULL;

	//invia: tipo operazione 2=invio result+path
	*buf = 2;
	write(fd_skt, buf, sizeof(long int));
	//riceve: conferma ricezione
	read(fd_skt, buf, sizeof(long int));
	if(buf != 0) goto sr_clean;
	
	//invia: result
	*buf = result;
	write(fd_skt, buf, sizeof(long int));
	//riceve: conferma ricezione
	read(fd_skt, buf, sizeof(long int));
	if(buf != 0) goto sr_clean;

	//invia: len file name
	size_t len_s = strlen(path);
	*buf = len_s;
	write(fd_skt, buf, sizeof(size_t));
	//riceve: conferma ricezione
	read(fd_skt, buf, sizeof(long int));
	if(buf != 0) goto sr_clean;

	//invia file name
	write(fd_skt, path, sizeof(char)*len_s);
	//riceve: conferma ricezione
	read(fd_skt, buf, sizeof(long int));
	if(buf != 0) goto sr_clean;
	
	mutex_unlock(&op_mtx, "send_res");
	return 0;

	sr_clean:
	mutex_unlock(&op_mtx, "send_res");
	return -1;

}

static int thread_func2(char* path)
{
	//1. leggere dal disco il contenuto dell'intero file
	//2. effettuare il calcolo sugli elementi contenuti nel file
	//3. inviare il risultato al processo collector tramite il socket insieme al nome del file
	printf("(thread_func2)\n");
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
	printf("(thread_func2) result=%ld\n", result);
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
	printf_queue(conc_queue);
	int err;
	char* buf = NULL;

	while (1){
		mutex_lock(&mtx, "thread_func1: lock fallita");
		//pop richiesta dalla coda concorrente
		while ( !(buf = (char*)dequeue(&conc_queue)) ){	
			if ((err = pthread_cond_wait(&cv, &mtx)) != 0){
				LOG_ERR(err, "thread_func1: phtread_cond_wait fallita");
				exit(EXIT_FAILURE);
			}
		}
		mutex_unlock(&mtx, "thread_func1: unlock fallita");
		//funzione che opera sul file
		//thread_func2(buf);
		if(buf) free(buf);
		buf = NULL;
	}
	return NULL;
}


static void MasterWorker()
{
	int err;
	long int* buf = NULL;
	buf = (long int*)malloc(sizeof(long int));
	/*
	///////////////// PROC. COLLECTOR /////////////////
	pid_t pid = fork();
	if (pid < 0){ //fork 1 fallita
		LOG_ERR(errno, "fork()");
		goto main_clean;
	}else{
		if (pid == 0){ //proc. figlio 1
			setsid();
			pid_t pid2 = fork();
			if (pid2 < 0){ //fork 2 fallita
				LOG_ERR(errno, "fork()");
				goto main_clean;
			}else{
				if (pid2 > 0) //proc. genitore 2 
					exit(0); //termino genitore 2 -> figlio 2 orfano 
				else{	//proc. figlio 2
					//if (close(0) == -1){ LOG_ERR(errno, "(main) close stdin"); goto main_clean; }
					//if (close(1) == -1){ LOG_ERR(errno, "(main) close stdout"); goto main_clean; }
					//if (close(2) == -1){ LOG_ERR(errno, "(main) close stderr"); goto main_clean; }
					//umask(0);
					//chdir("/");
					printf("(main) exec - differenziazione in corso\n");
					execl("./collector", "collector", (char*)NULL); //-> puoi inserire gli argomenti necessari da esportare
					LOG_ERR(errno, "execl");
					goto main_clean;
				}
			}
		}else{ //proc. genitore 1
			printf("(main) processo genitore\n");
			//exit(0);
			printf("processo 1 - pid=%d\n", getpid());
		}
	}
*/
	printf("creazione thread pool\n");
	///////////////// THREAD POOL  /////////////////
	pthread_t* thread_workers_arr = NULL;
	thread_workers_arr = (pthread_t*)calloc(sizeof(pthread_t), n_thread);
	//pthread_t thread_workers_arr[n_thread];
	for(int i = 0; i < n_thread; i++){
		if ((err = pthread_create(&(thread_workers_arr[i]), NULL, thread_func1, NULL)) != 0){    
			LOG_ERR(err, "pthread_create in server_manager");
			goto main_clean;
		}
	}

	///////////////// SEGNALI /////////////////
	struct sigaction s;
	memset(&s, 0, sizeof(struct sigaction));
	s.sa_handler = handler_sigint;
	ec_meno1_c(sigaction(SIGINT, &s, NULL), "(main) sigaction fallita", main_clean);
	
	struct sigaction s1;
	memset(&s1, 0, sizeof(struct sigaction));
	s1.sa_handler = handler_sigquit;
	ec_meno1_c(sigaction(SIGQUIT, &s1, NULL), "(main) sigaction fallita", main_clean);
	
	struct sigaction s2;
	memset(&s2, 0, sizeof(struct sigaction));
	s2.sa_handler = handler_sighup;
	ec_meno1_c(sigaction(SIGHUP, &s2, NULL), "(main) sigaction fallita", main_clean); 

	struct sigaction s3;
	memset(&s3, 0, sizeof(struct sigaction));
	s3.sa_handler = handler_sigterm;
	ec_meno1_c(sigaction(SIGTERM, &s3, NULL), "(main) sigaction fallita", main_clean);  

	struct sigaction s4;
	memset(&s4, 0, sizeof(struct sigaction));
	s4.sa_handler = handler_sigusr1;
	ec_meno1_c(sigaction(SIGUSR1, &s4, NULL), "(main) sigaction fallita", main_clean);

	struct sigaction s5;
	memset(&s5, 0, sizeof(struct sigaction));
	s5.sa_handler = SIG_IGN;      
	
	/*
	///////////////// SOCKET /////////////////
   	char socket_name[9];
	strcpy(socket_name, "farm.sck");
	socket_name[8] = '\0';
	struct sockaddr_un sa;
	sa.sun_family = AF_UNIX;
	size_t N = strlen(socket_name);
	strncpy(sa.sun_path, socket_name, N);
	sa.sun_path[N] = '\0';
   	//socket/bind/listen
	ec_meno1_c((fd_skt = socket(AF_UNIX, SOCK_STREAM, 0)), "(main) socket fallita", main_clean);
	ec_meno1_c(bind(fd_skt, (struct sockaddr*)&sa, sizeof(sa)), "(main) bind fallita", main_clean);
	ec_meno1_c(listen(fd_skt, SOMAXCONN), "(main) listen fallita", main_clean);
	
	
	int fd;
	ec_meno1_c((fd = accept(fd_skt, NULL, 0)), "(main) accept fallita", main_clean);
	printf("(main) socket: %s - attivo\n", socket_name);
*/
	//printf("(main) BUG HERE?\n");
/*
	//invia tot_files a collector
	*buf = tot_files;
	int n; //conta byte inviati
	if( (n = write(fd, buf, sizeof(size_t))) == -1) { LOG_ERR(errno, "(main) write"); goto main_clean; }
	//printf("(main) byte inviati = %d\n", n);
	read(fd, buf, sizeof(long int));
	if(*buf != 0 ) goto main_clean;
*/

	///////////////// MAIN LOOP /////////////////
	while( !closing && queue_capacity >= 0 ){
		//printf("main loop\n");
		/*
		if (sig_usr1){
			//notificare al processo collector di stampare i risultati ottenuti fino ad ora
			*buf = 1;
			write(fd, buf, sizeof(long int));
			read(fd, buf, sizeof(long int));
			if(*buf != 0 ) goto main_clean;
		}
*/
		if (!closing){
			if (queue_capacity < q_len && tot_files > 0){
				//lock
				mutex_lock(&mtx, "(main) lock fallita");
				//estrazione di un file
				node* temp = extract_node(&files_list);
				//enqueue
				if (enqueue(&conc_queue, temp->str) == -1){ 
					LOG_ERR(errno, "(main) enqueue fallita");
					goto main_clean; 
				}
				printf("enqueue %s ok\n", temp->str);
				printf_queue(conc_queue);
				/*
				//elimino il nodo
				free(temp->str);
				free(temp);
				*/
				queue_capacity++;
				tot_files--;
				printf("queue_capacity=%zu / tot_files=%zu\n", queue_capacity, tot_files);
				//segnala
				if ((err = pthread_cond_signal(&cv)) == -1){ 
					LOG_ERR(err, "(main) pthread_cond_signal"); 
					goto main_clean;
				}
				//unlock
				mutex_unlock(&mtx, "(main) unlock fallita");
				printf("(main) signal e unlock ok \n");
			}
		}
	}
	printf("exit loop\n");

	///////////////// TERMINAZIONE /////////////////
	mutex_lock(&mtx, "(main) lock fallita");
	pthread_cond_broadcast(&cv);
	mutex_unlock(&mtx, "(main) unlock fallita");

	//chiusura server normale
	for (int i = 0; i < n_thread; i++) {
		if ((err = pthread_join(thread_workers_arr[i], NULL)) == -1){
        		LOG_ERR(err, "(main) pthread_join fallita");
      			goto main_clean;
      		}	
	}
	//(!) processo collector?

	close(fd_skt);
	//if(socket_name) free(socket_name);
	if(files_list) dealloc_list(&files_list);
	if(conc_queue) dealloc_queue(&conc_queue);
	exit(EXIT_SUCCESS);

	//chiususa server in caso di errore
	main_clean:
	//if(socket_name) free(socket_name);
	if(buf) free(buf);
	if(files_list) dealloc_list(&files_list);
	if(conc_queue) dealloc_queue(&conc_queue);
	exit(EXIT_FAILURE);
}


int main(int argc, char* argv[])
{
	printf("processo 1 = %d\n", getpid());
	//parsing
    if (parser(argc, argv) == -1){
    	exit(EXIT_FAILURE);
    }
    //master worker
	MasterWorker();
    return 0;
}
