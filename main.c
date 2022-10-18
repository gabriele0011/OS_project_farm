//correzioni: 
//1. controlla che in take_from_dir si consideri il path dei file prelevati dalla directory

#include "main.h"
#define PATH_LEN_MAX 255

//var. mutex e cond.
pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cv = PTHREAD_COND_INITIALIZER;
pthread_mutex_t op_mtx = PTHREAD_MUTEX_INITIALIZER;


//parsing global var
size_t n_thread = 4;
size_t q_len = 8;
char* dir_name = NULL;
int ms_delay = 0;
//var socket
int fd;
int fd_skt;
//var globali lista e coda
node* files_list = NULL;
size_t tot_files = 0;
t_queue* conc_queue = NULL;
size_t queue_capacity = 0;

//segnali
volatile sig_atomic_t sig_usr1 = 0;
volatile sig_atomic_t closing = 0;
volatile sig_atomic_t child_term = 0;



void take_from_dir(const char* dirname)
{
	//passo 1: apertura dir
	DIR* d;
	if ((d = opendir(dirname)) == NULL){ 
            LOG_ERR(errno, "opendir in take_from_dir"); 
            return; 
      }
	//printf("take_from_dir:	dir '%s' aperta\n", dirname); //DEBUG
	//passo 2: esplorazione dir
	struct dirent* entry;
	errno = 0;
	while (errno == 0 && ((entry = readdir(d)) != NULL)){
		//passo 3 : aggiornamento path
		char path[PATH_LEN_MAX];
		memset((void*)path, '\0', (sizeof(char)*PATH_LEN_MAX));
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
			//file check file dalla dir
			if(file_check(path) != -1)
            	insert_node(&files_list, path);
			else{
				LOG_ERR(errno, "file_check in take_from_dir");
			}
		}
	}
	//chiudere la directory
	if (closedir(d) == -1){ LOG_ERR(errno, "closedir in take_from_dir"); }
	return;
}


int file_check(char* path)
{
	//printf("file_check on %s\n", path); //DEBUG
    //verifica file regolare
    struct stat s;
    if (lstat(path, &s) == -1){
        //LOG_ERR(errno, "lstat");
 	    return -1;
    }

	//controllo file regolare
    if(S_ISREG(s.st_mode) != 0){ 
		tot_files++; 
		return 0; 
	}else 
		return -1;
}


int parser(int dim, char** array)
{
    dir_name = NULL;
    //CICLO PARSING: si gestiscono i comandi di setting del server -n, -q, -d, -t + lista di files
	int i = 0;
	while (++i < dim){
		//printf("pasing now: %s\n", array[i+1]);
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
			//printf("DEBUG n_thread = %zu\n", n_thread); //DEBUG
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
		    //printf("DEBUG q_len = %zu\n", q_len); //DEBUG
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
            //printf("DEBUG: dir_name = %s\n", dir_name); //DEBUG
            //i++;
		}
		
		//CASO -t <ms_delay>
		if (is_opt(array[i], "-t")){
			if (is_argument(array[i+1])){
                if ((ms_delay = is_number(array[++i])) == -1){
				    LOG_ERR(EINVAL, "(main) argomento -t non numerico");
				    return -1;
                }
			}else{
                LOG_ERR(EINVAL, "(main) argomento -t mancante");
				return -1;
			}
		    //printf("DEBUG: ms_delay = %d\n", ms_delay); //DEBUG
            //i++;
		}
        //CASO file check lista di file
		if (file_check(array[i]) != -1){
    		insert_node(&files_list, array[i]);
		}
		/* //log errore file check
		else{
			//fprintf(stdout, "array[i] = %s\n", array[i]);
			//LOG_ERR(errno, "file_check parsing");
		}
		*/
	}

	//controllo dipendenze
    if (files_list == NULL && dir_name == NULL){
    	LOG_ERR(EINVAL, "(main) nessun file in input");
    	return -1;
    }

	//se -d allora esplora dir
    if(dir_name != NULL) take_from_dir(dir_name);
	

	//printf("DEBUG - lista dei file acquisiti\n"); //DEBUG
    //print_list(files_list); //DEBUG
	return 0;
}

int main(int argc, char* argv[])
{
	//printf("processo MasterWorker pid=%d\n", getpid());
	//parsing
    if (parser(argc, argv) == -1){
    	exit(EXIT_FAILURE);
    }
    //master worker
	MasterWorker();
    return 0;
}
