#include "master_thread.h"

extern t_queue* conc_queue;
extern node* files_list;

static void handler_sigchld(int signum){
	child_term = 1;
	return;
}

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
		goto mt_clean;
	}
	if (pid == 0){ //proc. figlio 1
			collector();
			//printf("processo figlio\n");
	}else{
	
		///////////////// SOCKET /////////////////
		struct sockaddr_un sa;
		sa.sun_family = AF_UNIX;
		size_t len = strlen(SOCK_NAME);
		strncpy(sa.sun_path, SOCK_NAME, len);
		sa.sun_path[len] = '\0';
   		//socket/bind/listen
		ec_meno1_c((fd_skt = socket(AF_UNIX, SOCK_STREAM, 0)), "(MasterWorker) socket fallita", mt_clean);
		ec_meno1_c(bind(fd_skt, (struct sockaddr*)&sa, sizeof(sa)), "(MasterWorker) bind fallita", mt_clean);
		ec_meno1_c(listen(fd_skt, SOMAXCONN), "(MasterWorker) listen fallita", mt_clean);
		ec_meno1_c((fd = accept(fd_skt, NULL, 0)), "(MasterWorker) accept fallita", mt_clean);
		//printf("(MasterWorker) socket: %s - attivo\n", sa.sun_path); //DEBUG
    
		///////////////// THREAD POOL  /////////////////
		pthread_t* thread_workers_arr = NULL;
    	thread_workers_arr = create_pool_worker();
    	if (thread_workers_arr == NULL) goto mt_clean;


		///////////////// SEGNALI /////////////////
		struct sigaction s1;
		memset(&s1, 0, sizeof(struct sigaction));
		s1.sa_handler = handler_sigint;
		ec_meno1_c(sigaction(SIGINT, &s1, NULL), "(MasterWorker) sigaction fallita", mt_clean);

		struct sigaction s2;
		memset(&s2, 0, sizeof(struct sigaction));
		s2.sa_handler = handler_sigquit;
		ec_meno1_c(sigaction(SIGQUIT, &s2, NULL), "(MasterWorker) sigaction fallita", mt_clean);

		struct sigaction s3;
		memset(&s3, 0, sizeof(struct sigaction));
		s3.sa_handler = handler_sighup;
		ec_meno1_c(sigaction(SIGHUP, &s3, NULL), "(MasterWorker) sigaction fallita", mt_clean); 

		struct sigaction s4;
		memset(&s4, 0, sizeof(struct sigaction));
		s4.sa_handler = handler_sigterm;
		ec_meno1_c(sigaction(SIGTERM, &s4, NULL), "(MasterWorker) sigaction fallita", mt_clean);  

		struct sigaction s5;
		memset(&s5, 0, sizeof(struct sigaction));
		s5.sa_handler = handler_sigusr1;
		ec_meno1_c(sigaction(SIGUSR1, &s5, NULL), "(MasterWorker) sigaction fallita", mt_clean);

		struct sigaction s6;
		memset(&s6, 0, sizeof(struct sigaction));
		s6.sa_handler = SIG_IGN; 
		//printf("(MasterWorker) installazione segnali avvenuta con successo\n"); //DEBUG

		struct sigaction s7;
		memset(&s7, 0, sizeof(struct sigaction));
		s7.sa_handler = handler_sigchld;
		ec_meno1_c(sigaction(SIGCHLD, &s7, NULL), "(MasterWorker) sigaction fallita", mt_clean);


		//invia: numero di file reg. input
		*buf = tot_files;
		write_n(fd, buf, sizeof(size_t));
		//riceve: conferma ricezione 
		read_n(fd, buf, sizeof(size_t));
		if (*buf != 0) goto mt_clean;


		///////////////// MasterWorker LOOP /////////////////
		while (!closing && !child_term){
			if (sig_usr1){
				//notificare al processo collector di stampare i risultati ottenuti fino ad ora
				*buf = 1;
				write_n(fd, buf, sizeof(size_t));
				read_n(fd, buf, sizeof(size_t));
				if (*buf != 0) goto mt_clean;
			}
			if (!closing){
				if (queue_capacity < q_len && tot_files > 0){
					//lock
					mutex_lock(&mtx, "(MasterWorker) lock fallita");
					//estrazione di un file
					node* temp = extract_node(&files_list);
					//enqueue
					if (enqueue(&conc_queue, temp->str) == -1){ 
						LOG_ERR(errno, "(MasterWorker) enqueue fallita");
						goto mt_clean; 
					}
					//elimino il nodo da files_list
					free(temp->str);
					free(temp);
					queue_capacity++;
					tot_files--;
					if (ms_delay != 0){
						if (msleep(ms_delay) < 0){
							LOG_ERR(errno, "(MasterWorker) msleep");
						}
					}
					//segnala
					if ((err = pthread_cond_signal(&cv)) == -1){ 
						LOG_ERR(err, "(MasterWorker) pthread_cond_signal"); 
						goto mt_clean;
					}
					//unlock
					mutex_unlock(&mtx, "(MasterWorker) unlock fallita");
					//printf("(MasterWorker) signal e unlock ok \n");
				}
			}
			if (closing){
				//comunica: chiusura
				*buf = 3;
				write_n(fd, buf, sizeof(size_t));
				//riceve: conferma ricezione
				read_n(fd, buf, sizeof(size_t));
				if (*buf != 0) goto mt_clean;

				//comunica num. elem. ancora da elaborare
				*buf = queue_capacity;
				write_n(fd, buf, sizeof(size_t));
				//riceve: conferma ricezione
				read_n(fd, buf, sizeof(size_t));
				if (*buf != 0) goto mt_clean;
			}
		}

		///////////////// TERMINAZIONE /////////////////
		//printf("CLOSING\n");
		int status;
		//attesa terminazione collector
		if (waitpid(pid, &status, 0) == -1){
			LOG_ERR(errno, "(MasterWorker) wait()");
			exit(EXIT_FAILURE);
		}

		//broadcast per threads
		mutex_lock(&mtx, "(MasterWorker) lock fallita");
		//printf("ptread_cond_broadcast...\n");
		if ((err=pthread_cond_broadcast(&cv)) != 0){
			LOG_ERR(err, "pthread_cond_broadcast");
			exit(EXIT_FAILURE);
		}
		//printf("ptread_cond_broadcast terminata\n");
		mutex_unlock(&mtx, "(MasterWorker) unlock fallita");

		//join threads
		for (int i = 0; i < n_thread; i++) {
			if ((err = pthread_join(thread_workers_arr[i], NULL)) == -1){
    	    		LOG_ERR(err, "(MasterWorker) pthread_join fallita");
    	  			goto mt_clean;
    	  		}	
		}

		//eliminazione socket file
		int ret;
		ret = remove(SOCK_NAME);
		if (ret != 0 ){
			LOG_ERR(errno, "rimozione file socket fallita");
		}

		//deallocazioni heap
		close(fd_skt);
		if (thread_workers_arr) free(thread_workers_arr);
		if (buf) free(buf);
		if (files_list) dealloc_list(&files_list);
		if (conc_queue) dealloc_queue(&conc_queue);
		exit(EXIT_SUCCESS);

		//chiususa server in caso di errore
		mt_clean:
		if (thread_workers_arr) free(thread_workers_arr);
		if (buf) free(buf);
		if (thread_workers_arr) free(thread_workers_arr);
		if (files_list) dealloc_list(&files_list);
		if (conc_queue) dealloc_queue(&conc_queue);
		exit(EXIT_FAILURE);
	}
}	