#include "master_thread.h"

extern t_queue* conc_queue;
extern node* files_list;

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


void MasterWorker()
{
	int err;
	long int* buf = NULL;
	buf = (long int*)malloc(sizeof(long int));
	
	///////////////// PROC. COLLECTOR /////////////////
	pid_t pid = fork();
	if (pid < 0){ //fork 1 fallita
		LOG_ERR(errno, "fork()");
		goto main_clean;
	}else{
		if (pid == 0) //proc. figlio 1
			collector();
	}

    ///////////////// THREAD POOL  /////////////////
	pthread_t* thread_workers_arr = NULL;
    thread_workers_arr = create_pool_worker();
    if (thread_workers_arr == NULL) goto main_clean;


	///////////////// SEGNALI /////////////////
	struct sigaction s1;
	memset(&s1, 0, sizeof(struct sigaction));
	s1.sa_handler = handler_sigint;
	ec_meno1_c(sigaction(SIGINT, &s1, NULL), "(main) sigaction fallita", main_clean);
	
	struct sigaction s2;
	memset(&s2, 0, sizeof(struct sigaction));
	s2.sa_handler = handler_sigquit;
	ec_meno1_c(sigaction(SIGQUIT, &s2, NULL), "(main) sigaction fallita", main_clean);
	
	struct sigaction s3;
	memset(&s3, 0, sizeof(struct sigaction));
	s3.sa_handler = handler_sighup;
	ec_meno1_c(sigaction(SIGHUP, &s3, NULL), "(main) sigaction fallita", main_clean); 

	struct sigaction s4;
	memset(&s4, 0, sizeof(struct sigaction));
	s4.sa_handler = handler_sigterm;
	ec_meno1_c(sigaction(SIGTERM, &s4, NULL), "(main) sigaction fallita", main_clean);  

	struct sigaction s5;
	memset(&s5, 0, sizeof(struct sigaction));
	s5.sa_handler = handler_sigusr1;
	ec_meno1_c(sigaction(SIGUSR1, &s5, NULL), "(main) sigaction fallita", main_clean);

	struct sigaction s6;
	memset(&s6, 0, sizeof(struct sigaction));
	s6.sa_handler = SIG_IGN; 
	//printf("(MasterWorker) installazione segnali avvenuta con successo\n"); //DEBUG
	
	
	///////////////// SOCKET /////////////////
   	size_t len = 9;
	char* socket_name = NULL;
	socket_name = (char*)calloc(sizeof(char), len);
	//char socket_name[len];
	strncpy(socket_name, "farm.sck", len-1);
	socket_name[len-1] = '\0';
	struct sockaddr_un sa;
	sa.sun_family = AF_UNIX;
	size_t N = strlen(socket_name);
	strncpy(sa.sun_path, socket_name, N);
	sa.sun_path[N] = '\0';
   	//socket/bind/listen
	ec_meno1_c((fd_skt = socket(AF_UNIX, SOCK_STREAM, 0)), "(main) socket fallita", main_clean);
	ec_meno1_c(bind(fd_skt, (struct sockaddr*)&sa, sizeof(sa)), "(main) bind fallita", main_clean);
	ec_meno1_c(listen(fd_skt, SOMAXCONN), "(main) listen fallita", main_clean);
	ec_meno1_c((fd = accept(fd_skt, NULL, 0)), "(main) accept fallita", main_clean);
	//printf("(main) socket: %s - attivo\n", socket_name); //DEBUG


	//invia: tot_files
	*buf = tot_files;
	writen(fd, buf, sizeof(size_t));
	//riceve: conferma ricezione 
	readn(fd, buf, sizeof(size_t));
	if (*buf != 0) goto main_clean;


	///////////////// MAIN LOOP /////////////////
	while (!closing && queue_capacity >= 0){
		if (sig_usr1){
			//notificare al processo collector di stampare i risultati ottenuti fino ad ora
			*buf = 1;
			writen(fd, buf, sizeof(long int));
			readn(fd, buf, sizeof(long int));
			if (*buf != 0) goto main_clean;
		}
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
				//elimino il nodo da files_list
				free(temp->str);
				free(temp);
				queue_capacity++;
				tot_files--;
				//printf("queue_capacity=%zu / tot_files=%zu\n", queue_capacity, tot_files); //DEBUG
				if (ms_delay != 0){
					msleep(ms_delay);
				}
				//segnala
				if ((err = pthread_cond_signal(&cv)) == -1){ 
					LOG_ERR(err, "(main) pthread_cond_signal"); 
					goto main_clean;
				}
				//unlock
				mutex_unlock(&mtx, "(main) unlock fallita");
				//printf("(main) signal e unlock ok \n");
			}
		}
	}
	printf("exit loop\n"); //DEBUG

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

	close(fd_skt);
	if (buf) free(buf);
	if (thread_workers_arr) free(thread_workers_arr);
	if (socket_name) free(socket_name);
	if (files_list) dealloc_list(&files_list);
	if (conc_queue) dealloc_queue(&conc_queue);
	exit(EXIT_SUCCESS);

	//chiususa server in caso di errore
	main_clean:
	if (buf) free(buf);
	if (thread_workers_arr) free(thread_workers_arr);
	if (socket_name) free(socket_name);
	if (files_list) dealloc_list(&files_list);
	if (conc_queue) dealloc_queue(&conc_queue);
	exit(EXIT_FAILURE);
}