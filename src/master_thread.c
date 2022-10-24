#include "master_thread.h"

extern t_queue* conc_queue;
extern node* files_list;

//signal handlers
static void handler_sigchld(int signum){
	child_term = 1;
	return;
}
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
	
	///////////////// SEGNALI /////////////////
	struct sigaction s; 
	sigset_t set;
	if (sigemptyset(&s.sa_mask) != 0){
		LOG_ERR(errno, "(MasterWorker) sigemptyset fallita");
		goto mt_clean;
	}
	memset(&s, 0, sizeof(struct sigaction));
	s.sa_handler = SIG_IGN;
	ec_meno1_c(sigaction(SIGINT, &s, NULL), "(MasterWorker) sigaction fallita", mt_clean);
	ec_meno1_c(sigaction(SIGQUIT, &s, NULL), "(MasterWorker) sigaction fallita", mt_clean);
	ec_meno1_c(sigaction(SIGHUP, &s, NULL), "(MasterWorker) sigaction fallita", mt_clean);
	ec_meno1_c(sigaction(SIGTERM, &s, NULL), "(MasterWorker) sigaction fallita", mt_clean);
	ec_meno1_c(sigaction(SIGUSR1, &s, NULL), "(MasterWorker) sigaction fallita", mt_clean);
	ec_meno1_c(sigaction(SIGPIPE, &s, NULL), "(MasterWorker) sigaction fallita", mt_clean);
	ec_meno1_c(sigaction(SIGCHLD, &s, NULL), "(MasterWorker) sigaction fallita", mt_clean);
	
	//crea signal mask
	if (sigfillset(&set) != 0){
		LOG_ERR(errno, "sigfillset");
		goto mt_clean;
	}
	if ((err = pthread_sigmask(SIG_SETMASK, &set, NULL)) != 0){
		LOG_ERR(err, "(MasterWorker) pthread_signmask fallita");
		goto mt_clean;
	}

	///////////////// PROC. COLLECTOR /////////////////
	pid_t pid = fork();
	if (pid < 0){ //fork 1 fallita
		LOG_ERR(errno, "(MasterWorker) fork fallita");
		goto mt_clean;
	}
	if (pid == 0){ //proc. figlio 1
			collector();
	}else{
	
		///////////////// SEGNALI /////////////////
		//gestori permantenti
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
		ec_meno1_c(sigaction(SIGPIPE, &s6, NULL), "(MasterWorker) sigaction fallita", mt_clean);

		struct sigaction s7;
		memset(&s7, 0, sizeof(struct sigaction));
		s7.sa_handler = handler_sigchld;
		ec_meno1_c(sigaction(SIGCHLD, &s7, NULL), "(MasterWorker) sigaction fallita", mt_clean);

		if (sigemptyset(&set) != 0){
			LOG_ERR(errno, "(MasterWorker) sigemptyset fallita");
			goto mt_clean;
		}
		if ((err = pthread_sigmask(SIG_SETMASK, &set, NULL)) != 0){
			LOG_ERR(err, "(MasterWorker) pthread_signmask fallita");
			goto mt_clean;
		}

		//////////////// SOCKET /////////////////
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


		//comunica: numero di file reg. input
		*buf = tot_files;
		write_n(fd, buf, sizeof(size_t));
		//riceve: conferma ricezione 
		read_n(fd, buf, sizeof(size_t));
		if (*buf != 0) goto mt_clean;


		size_t rem_files = tot_files; //files ancora da elaborare

		///////////////// MasterWorker LOOP /////////////////
		while (!child_term){
			if (sig_usr1){
				//notificare al processo collector di stampare i risultati ottenuti fino ad ora
				*buf = 1;
				write_n(fd, buf, sizeof(size_t));
				read_n(fd, buf, sizeof(size_t));
				if (*buf != 0) goto mt_clean;
			}
			if (!closing){
				if (q_curr_capacity < q_len && rem_files > 0){
					//lock
					mutex_lock(&mtx, "(MasterWorker) lock fallita");
					//estrazione di un file
					node* temp = extract_node(&files_list);
					//enqueue
					if (enqueue(&conc_queue, temp->str) == -1){ 
						LOG_ERR(errno, "(MasterWorker) enqueue fallita");
						goto mt_clean; 
					}
					//dealloca il nodo estratto
					free(temp->str);
					free(temp);
					//aggiorna cap. attuale
					q_curr_capacity++;
					rem_files--;
					if (ms_delay != 0){
						if (msleep(ms_delay) < 0){
							LOG_ERR(errno, "(MasterWorker) msleep fallita");
						}
					}
					//signal
					if ((err = pthread_cond_signal(&cv)) == -1){ 
						LOG_ERR(err, "(MasterWorker) pthread_cond_signal"); 
						goto mt_clean;
					}
					//unlock
					mutex_unlock(&mtx, "(MasterWorker) unlock fallita");
				}
			}
			if (closing){
				//comunica: segnale di chiusura -> 3
				*buf = 3;
				write_n(fd, buf, sizeof(size_t));
				//riceve: conferma ricezione
				read_n(fd, buf, sizeof(size_t));
				if (*buf != 0) goto mt_clean;

				//comunica: num. elem. in coda ancora da elaborare
				*buf = q_curr_capacity;
				write_n(fd, buf, sizeof(size_t));
				//riceve: conferma ricezione
				read_n(fd, buf, sizeof(size_t));
				if (*buf != 0) goto mt_clean;
				break;
			}
		}

		///////////////// TERMINAZIONE /////////////////
		//printf("CLOSING\n");
		int status;
		//attesa terminazione collector
		if (waitpid(pid, &status, 0) == -1){
			LOG_ERR(errno, "(MasterWorker) wait fallita");
			exit(EXIT_FAILURE);
		}

		//broadcast per threads
		mutex_lock(&mtx, "(MasterWorker) lock fallita");
		if ((err=pthread_cond_broadcast(&cv)) != 0){
			LOG_ERR(err, "(MasterWorker) pthread_cond_broadcast fallita");
			exit(EXIT_FAILURE);
		}
		mutex_unlock(&mtx, "(MasterWorker) unlock fallita");

		//join threads
		for (int i = 0; i < n_thread; i++) {
			if ((err = pthread_join(thread_workers_arr[i], NULL)) == -1){
    	    		LOG_ERR(err, "(MasterWorker) pthread_join fallita");
    	  			goto mt_clean;
    	  		}	
		}

		//eliminazione socket file
		if (remove(SOCK_NAME) != 0 ){
			LOG_ERR(errno, "(MasterWorker) rimozione file socket fallita");
		}

		//chiusura normale
		if (close(fd_skt) != 0){
			LOG_ERR(errno, "(MasterWorker) close fallita");
		}
		if (close(fd) != 0){
			LOG_ERR(errno, "(MasterWorker) close fallita");
		}

		if (thread_workers_arr) free(thread_workers_arr);
		if (buf) free(buf);
		if (files_list) dealloc_list(&files_list);
		if (conc_queue) dealloc_queue(&conc_queue);
		exit(EXIT_SUCCESS);

		//chiususa in caso di errore
		mt_clean:
		//eliminazione socket file
		if (sa.sun_path != NULL) remove(SOCK_NAME);
		if (fd != -1) close(fd);
		if (fd_skt != -1) close(fd_skt);
		if (thread_workers_arr) free(thread_workers_arr);
		if (buf) free(buf);
		if (thread_workers_arr) free(thread_workers_arr);
		if (files_list) dealloc_list(&files_list);
		if (conc_queue) dealloc_queue(&conc_queue);
		exit(EXIT_FAILURE);
	}
}	