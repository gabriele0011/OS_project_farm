#include "header_file.h"
#include "error_ctrl.h"
#include "aux_function.h"
#include "list.h"

pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cv = PTHREAD_COND_INITIALIZER;

//global variables
size_t n_thread = 4;
size_t q_len = 8;
char* dir_name = NULL;
size_t delay = 0;
node* files_list = NULL;
char* socket_name = NULL;
size_t queue_capacity = 0;

//SEGNALI
//variables
volatile sig_atomic_t sig_usr1 = 0;
volatile sig_atomic_t sig_pipe = 0;
volatile sig_atomic_t close = 0;

//handlers
static void handler_sigquit(int signum){
	close = 1;
	return;
}
static void handler_sigint(int signum){
	close = 1;
	return;
}
static void handler_sighup(int signum){
	close = 1;
	return;
}
static void handler_sigterm(int signum){
	close = 1;
	return;
}
static void handler_sigusr1(int signum){
	sig_usr1 = 1;
	return;
}

//FUNC
static int file_check(char* file_name)
{
      //verifica se è un file regolare
      struct stat s;
      if (lstat(file_name, &s) == -1){
           //LOG_ERR(errno, "lstat");
            return -1;
      }
      //controllo file regolare
      if(S_ISREG(s.st_mode) != 0) return 0;
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
                        if ((_ = is_number(array[++i])) == -1){
				      LOG_ERR(EINVAL, "(main) argomento -n non numerico");
				      return -1;
                        }
			}else{
                        LOG_ERR(EINVAL, "(main) argomento -n mancante");
				return -1;
                  }
			printf("DEBUG _ = %zu\n", _); //DEBUG
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

//thread_func
void* thread_func(void *arg)
{
	int fd_c; //diventa nome del file
	int op;
	int err;
	int* buf = NULL;

	while (1){
		mutex_lock(&mtx, "thread_func: lock fallita");
		//pop richiesta dalla coda concorrente
		while (((buf = (int*)dequeue(&conc_queue)) == NULL) && (!sig_int && !sig_quit)){
			if ( (err = pthread_cond_wait(&cv, &mtx)) != 0){
				LOG_ERR(err, "thread_func: phtread_cond_wait fallita");
				exit(EXIT_FAILURE);
			}
		}
		mutex_unlock(&mtx, "thread_func: unlock fallita");
		
		if(sig_int || sig_quit ){ 	
			if(buf) free(buf);
			return NULL;
		}
		//salvo il client in fd_c
		fd_c = *buf;
      	*buf = 0;
		//leggi la richiesta di fd_c se fallisce torna 0 al manager
		if (read(fd_c, buf, sizeof(int)) == -1){
			LOG_ERR(errno, "thread_func: read su fd_client fallita -> client disconnesso");
			*buf = 0;
		}
		op = *buf;

		printf("\n");
		printf("(SERVER) - thread_func:		elaborazione nuova richiesta di tipo %d\n", op); //DEBUG
		//printf("(SERVER) - thread_func: fd_c= %d / op =%d / byte_read=%d\n", fd_c, op, N); //DEBUG
		
		//client disconnesso
		if (op == EOF || op == 0){
			//fprintf(stderr, "thread_func: client %d disconnesso\n", fd_c);
			*buf = fd_c;
			*buf *=(-1);
			if (write(fd_pipe_write, buf, PIPE_MAX_LEN_MSG) == -1){
				LOG_ERR(errno, "thread_func: write su pipe fallita");
			}
		}
		
		//closeFile
		if( op == 9){
			if ( worker_closeFile(fd_c) == -1){ 
				LOG_ERR(errno, "thread_func: (9) worker_closeFile fallita");
				exit(EXIT_FAILURE);
			}
			*buf = fd_c;
			if (write(fd_pipe_write, buf, PIPE_MAX_LEN_MSG) == -1){
				LOG_ERR(EPIPE, "thread_func: (9) write su pipe fallita");
			}
			tot_requests++;
		}
		if(buf) free(buf);
		buf = NULL;
	}
}



static void MasterWorker()
{
	///////////////// PIPE SENZA NOME /////////////////
	int pfd[2];
	if((err = pipe(pfd)) == -1) { LOG_ERR(errno, "server_manager"); goto main_clean; }
	fd_pipe_read = pfd[0];
	fd_pipe_write = pfd[1];

	///////////////// LISTEN SOCKET /////////////////
   	struct sockaddr_un sa;
	sa.sun_family = AF_UNIX;
	size_t N = strlen(socket_name);
	strncpy(sa.sun_path, socket_name, N);
	sa.sun_path[N] = '\0';
   	//socket/bind/listen
	ec_meno1_c((fd_skt = socket(AF_UNIX, SOCK_STREAM, 0)), "(main) socket fallita", main_clean);
	ec_meno1_c(bind(fd_skt, (struct sockaddr*)&sa, sizeof(sa)), "(main) bind fallita", main_clean);
	ec_meno1_c(listen(fd_skt, SOMAXCONN), "(main) listen fallita", main_clean);
	printf("socket_name:%s\n", socket_name);
	
	///////////////// THREAD POOL  /////////////////
	pthread_t thread_workers_arr[n_thread];
	for(int i = 0; i < n_thread; i++){
		if ((err = pthread_create(&(thread_workers_arr[i]), NULL, thread_func, NULL)) != 0){    
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
	s2.sa_handler = handler_sigterm;
	ec_meno1_c(sigaction(SIGTERM, &s3, NULL), "(main) sigaction fallita", main_clean);  

	struct sigaction s4;
	memset(&s4, 0, sizeof(struct sigaction));
	s2.sa_handler = handler_sigusr1;
	ec_meno1_c(sigaction(SIGUSR1, &s4, NULL), "(main) sigaction fallita", main_clean);

	struct sigaction s5;
	memset(&s5, 0, sizeof(struct sigaction));
	s5.sa_handler = SIG_IGN;

	///////////////// PROC. COLLECTOR /////////////////
	//genera il processo collector
	//scrivere codice e chiamare fork/exec


	///////////////// MAIN LOOP /////////////////

	while(1){
		if(!close){
			mutex_lock(&mtx, "(main) lock fallita");
			if(queue_capacity < q_len){
				if(enqueue(&conc_queue, ...) == -1){ 
					LOG_ERR(-1, "(main) enqueue fallita");
					goto main_clean; 
				}
				queue_capacity++;
			}
			if ((err = pthread_cond_signal(&cv)) == -1){ 
				LOG_ERR(err, "(main) pthread_cond_signal"); 
				goto main_clean;
			}
			mutex_unlock(&mtx, "(main) unlock fallita");
		}
	}


	///////////////// TERMINAZIONE /////////////////
	mutex_lock(&mtx, "(main) lock fallita");
	pthread_cond_broadcast(&cv);
	mutex_unlock(&mtx, "(main) unlock fallita");

	//chiusura server normale
	for (int i = 0; i < t_workers_num; i++) {
		if ((err = pthread_join(thread_workers_arr[i], NULL)) == -1){
        		LOG_ERR(err, "(main) pthread_join fallita");
      			goto main_clean;
      		}	
	}
	//(!) processo collector?

	close(fd_skt);
	close(fd_pipe_read);
	close(fd_pipe_write);
	if(socket_name) free(socket_name);
	if(files_list) dealloc_list(&files_list);
	if(conc_queue) dealloc_queue(&conc_queue);
	exit(EXIT_SUCCESS);

	//chiususa server in caso di errore
	main_clean:
	close(fd_pipe_read);
	close(fd_pipe_write);
	if(socket_name) free(socket_name);
	if(buf) free(buf);
	if(files_list) dealloc_list(&files_list);
	if(conc_queue) dealloc_queue(&conc_queue);
	exit(EXIT_FAILURE);
}


int main(int argc, char* argv[])
{
      //parsing
      if (parser(argc, argv) == -1){
            exit(EXIT_FAILURE);
      }
      //master worker
      MasterWorker();
      return 0;
}