#include "collector.h"


pthread_mutex_t c_mtx = PTHREAD_MUTEX_INITIALIZER;

int cmp_func(const void*a, const void* b)
{
      const elem* x = a;
      const elem* y = b;
      if (x->res >= y->res) return 1;
      else return -1;
}


void collector()
{
      //printf("(collector) pid=%d\n", getpid()); //DEBUG
      long int* buf = NULL;
	buf = (long int*)malloc(sizeof(long int));
      int k;
      elem* arr = NULL;

      ///////////////// SEGNALI /////////////////
      struct sigaction s;
	memset(&s, 0, sizeof(struct sigaction));
      s.sa_handler = SIG_IGN;
	ec_meno1_c(sigaction(SIGINT, &s, NULL), "(main) sigaction fallita", c_clean);
	
	struct sigaction s1;
	memset(&s1, 0, sizeof(struct sigaction));
      s1.sa_handler = SIG_IGN;
      ec_meno1_c(sigaction(SIGQUIT, &s1, NULL), "(main) sigaction fallita", c_clean);
	
	struct sigaction s2;
	memset(&s2, 0, sizeof(struct sigaction));
      s2.sa_handler = SIG_IGN;
	ec_meno1_c(sigaction(SIGHUP, &s2, NULL), "(main) sigaction fallita", c_clean); 

	struct sigaction s3;
	memset(&s3, 0, sizeof(struct sigaction));
      s3.sa_handler = SIG_IGN;
	ec_meno1_c(sigaction(SIGTERM, &s3, NULL), "(main) sigaction fallita", c_clean);  

	struct sigaction s4;
	memset(&s4, 0, sizeof(struct sigaction));
      s4.sa_handler = SIG_IGN;
	ec_meno1_c(sigaction(SIGUSR1, &s4, NULL), "(main) sigaction fallita", c_clean);

      struct sigaction s5;
      memset(&s5, 0, sizeof(struct sigaction));
	s5.sa_handler = SIG_IGN;
	ec_meno1_c(sigaction(SIGPIPE, &s5, NULL), "(MasterWorker) sigaction fallita", c_clean);

	//gestione del segnale SUIGPIPE ereditata dal processo padre

      //azzera signal mask
      sigset_t set;
	ec_meno1_c(sigemptyset(&set), "(collector) sigemptyset fallita", c_clean);
      int err;
	if ((err = pthread_sigmask(SIG_SETMASK, &set, NULL)) != 0){
		LOG_ERR(err, "(collector) pthread_signmask fallita");
		goto c_clean;
	}

      ///////////////// SOCKET /////////////////
	int sockfd;
      struct sockaddr_un sa;
	sa.sun_family = AF_UNIX;
	size_t len = strlen(SOCK_NAME);
	strncpy(sa.sun_path, SOCK_NAME, len);
	sa.sun_path[len] = '\0';

	if ((sockfd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1){
            LOG_ERR(errno, "(collector) socket");
            goto c_clean;
      }
      if (connect(sockfd, (struct sockaddr*)&sa, sizeof(sa)) == -1){
	    LOG_ERR(errno, "(collector) connect");
          goto c_clean;
      }
	//printf("(collector) socket: %s - attivo\n", sa.sun_path); //DEBUG


      //riceve: numero di file reg. in input
      size_t tot_files = 0;
	read_n(sockfd, buf, sizeof(size_t));
      tot_files = *buf;
      //invia: conferma ricezione
      *buf = 0;
	write_n(sockfd, buf, sizeof(size_t));

      //elem arr[tot_files]; //array che conterra tutti i risultati
      if (tot_files == 0) goto c_clean;
      arr = (elem*)calloc(tot_files, sizeof(elem));
      if (arr == NULL){
            LOG_ERR(errno, "calloc");
            exit(EXIT_FAILURE);
      }
      
      int rem_files = -1;
      int i = 0;
      int j = tot_files;
      size_t op;
      while(j > 0 && rem_files != 0){
            mutex_lock(&c_mtx, "lock fallita");
            //riceve: operazione
            *buf = 0;
            read_n(sockfd, buf, sizeof(size_t));
            op = *buf;
            //invia: conferma ricezione
            *buf = 0;
            write_n(sockfd, buf, sizeof(size_t));

            //stampa i risultati fino a questo istante
            if (op == 1){
                  size_t temp_num_files = tot_files - i;
                  //ordina risultati
                  qsort(arr, tot_files, sizeof(elem), cmp_func);
                  //stampa risultati
                  for (k = temp_num_files; k < tot_files; k++){
                        printf("%ld %s\n", arr[k].res, arr[k].path);
      }
            }
            //ricezione di un nuovo risultato+path
            if (op == 2){
                  //riceve: risultato
                  long int result;
	            read_n(sockfd, buf, sizeof(long int));
                  result = *buf;
                  //invia: conferma ricezione
                  *buf = 0;
	            write_n(sockfd, buf, sizeof(size_t));

                  //riceve: lung. str
                  size_t len_s;
	            read_n(sockfd, buf, sizeof(size_t));
                  len_s = *buf; //cast?
                  //invia: conferma ricezione
                  *buf = 0;
	            write_n(sockfd, buf, sizeof(size_t));

                  //riceve: str
                  char* str = NULL;
                  str = calloc(len_s+1, sizeof(char));
	            read_n(sockfd, str, sizeof(char)*len_s);
                  str[len_s] = '\0';
                  //invia: conferma ricezione
                  *buf = 0;
	            write_n(sockfd, buf, sizeof(size_t));

                  //memorizza in array
                  strncpy(arr[i].path, str, len_s);
                  (arr[i].path)[len_s] = '\0';
                  arr[i].res = result;
                  i++; j--;
                  if (str) free(str);
                  if (rem_files > 0) rem_files--;
            }
            //chisura
            if (op == 3){
                  //riceve: num. di elem. ancora da elaborare
                  rem_files;
	            read_n(sockfd, buf, sizeof(size_t));
                  rem_files = *buf;
                  //invia: conferma ricezione
                  *buf = 0;
	            write_n(sockfd, buf, sizeof(size_t));
            }
            mutex_unlock(&c_mtx, "lock fallita");
      }
      
      //OUTPUT
      size_t final_num_files = tot_files - i;
      //ordina risultati
      qsort(arr, tot_files, sizeof(elem), cmp_func);
      //stampa risultati
      for (k = final_num_files; k < tot_files; k++){
            printf("%ld %s\n", arr[k].res, arr[k].path);
      }
      
      //chiusura normale
      if (close(sockfd) != 0){
      	LOG_ERR(errno, "(Collector) close fallita");
      }
      if (arr) free(arr);
      if (buf) free(buf);
      exit(EXIT_SUCCESS);
      
      //chiusura errore
      c_clean:
      //if (arr) free(arr);
      if (buf) free(buf);
      exit(EXIT_FAILURE);
}