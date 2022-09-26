#include "header_file.h"
#include "error_ctrl.h"
#include "aux_function.h"
#include "list.h"

//correzioni: 
//1. controlla che in take_from_dir si consideri il path dei file prelevati dalla directory

pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cv = PTHREAD_COND_INITIALIZER;

//global variables
size_t n_thread = 4;
size_t q_len = 8;
char* dir_name = NULL;
size_t delay = 0;
node* files_list = NULL;
size_t queue_capacity = 0;
int fd_skt;
t_queue* conc_queue = NULL;

//SEGNALI
//variables
volatile sig_atomic_t sig_usr1 = 0;
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
	int err;
	char* buf = NULL;

	while (1){
		mutex_lock(&mtx, "thread_func: lock fallita");
		//pop richiesta dalla coda concorrente
		while ( (buf = (int*)dequeue(&conc_queue)) == NULL){
			if ( (err = pthread_cond_wait(&cv, &mtx)) != 0){
				LOG_ERR(err, "thread_func: phtread_cond_wait fallita");
				exit(EXIT_FAILURE);
			}
		}
		mutex_unlock(&mtx, "thread_func: unlock fallita");
	
		//funzione che opera sul file
		function(buf);

		if(buf) free(buf);
		buf = NULL;
	}
}

static int function(char* file_name)
{
	//1. leggere dal disco il contenuto dell'intero file
	//2. effettuare il calcolo sugli elementi contenuti nel file
	//3. inviare il risultato al processo collector tramite il socket insieme al nome del file

	FILE *fd;
  	long int x;
 	int res;
	
	//apre il file in lettura
  	fd = fopen(file_name, "r");
 	if (fd == NULL) {
   		perror("Errore in apertura del file");
   		exit(1);
  	}

	//dimensione del file
	struct stat s;
      if (lstat(file_name, &s) == -1){
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
    		printf("%ld\n", x);
  	}

	//chiude il file
  	fclose(fd);

	//inviare il risultato al processo collector tramite socket
	long int* buf;

	//invia: result
	*buf = result;
	write(fd_skt, buf, sizeof(long int));
	//riceve: conferma ricezione
	read(fd_skt, buf, sizeof(long int));
	if(buf != 0) return -1;

	//invia: len file name
	size_t len_s = strlen(file_name);
	*buf = len_s;
	write(fd_skt, buf, sizeof(size_t));
	//riceve: conferma ricezione
	read(fd_skt, buf, sizeof(long int));
	if(buf != 0) return -1;

	//invia file name
	write(fd_skt, file_name, sizeof(char)*len_s);
	//riceve: conferma ricezione
	read(fd_skt, buf, sizeof(long int));
	if(buf != 0) return -1;	

	return 0;
}


static void MasterWorker()
{
	///////////////// SOCKET /////////////////
   	char socket_name[8] = {'f', 'a', 'r', 'm', '.', 's', 'c', 'k', '\0'};
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
	int pid = fork();
	if (pid == -1){
		LOG_ERR(errno, "fork()");
		goto main_clean;
	execl("/src/collector", "collector.c", (char*)NULL);
	if( errno != 0 ){
		LOG_ERR(errno, "execl");
		goto main_clean;
	}


	///////////////// MAIN LOOP /////////////////

	while( !close && queue_capacity > 0 ){

		//se si attiva il segnale sigusr1
		//comunica al processo collector di stampare i risultati ottenuti fino ad ora
		

		if(!close){
			mutex_lock(&mtx, "(main) lock fallita");
			if(queue_capacity < q_len){
				//estrazione del nodo
				node* temp = extract_node(&files_list);
				if(enqueue(&conc_queue, temp->str == -1){ 
					LOG_ERR(-1, "(main) enqueue fallita");
					goto main_clean; 
				}
				//elimino il nodo
				free(temp->str);
				free(temp);
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
	if(socket_name) free(socket_name);
	if(files_list) dealloc_list(&files_list);
	if(conc_queue) dealloc_queue(&conc_queue);
	exit(EXIT_SUCCESS);

	//chiususa server in caso di errore
	main_clean:
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