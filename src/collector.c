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
      long int* long_buf = NULL;
	long_buf = (long int*)malloc(sizeof(long int));
      size_t* sizet_buf = NULL; 
      sizet_buf = (size_t*)malloc(sizeof(size_t));
      int k;
      int fd_skt = -1;
      elem* arr = NULL;

      ///////////////// SEGNALI /////////////////
      struct sigaction s;
	memset(&s, 0, sizeof(struct sigaction));
      s.sa_handler = SIG_IGN;
	ec_meno1_c(sigaction(SIGINT, &s, NULL), "(collector) sigaction", c_clean);
	
	struct sigaction s1;
	memset(&s1, 0, sizeof(struct sigaction));
      s1.sa_handler = SIG_IGN;
      ec_meno1_c(sigaction(SIGQUIT, &s1, NULL), "(collector) sigaction", c_clean);
	
	struct sigaction s2;
	memset(&s2, 0, sizeof(struct sigaction));
      s2.sa_handler = SIG_IGN;
	ec_meno1_c(sigaction(SIGHUP, &s2, NULL), "(collector) sigaction", c_clean); 

	struct sigaction s3;
	memset(&s3, 0, sizeof(struct sigaction));
      s3.sa_handler = SIG_IGN;
	ec_meno1_c(sigaction(SIGTERM, &s3, NULL), "(collector) sigaction", c_clean);  

	struct sigaction s4;
	memset(&s4, 0, sizeof(struct sigaction));
      s4.sa_handler = SIG_IGN;
	ec_meno1_c(sigaction(SIGUSR1, &s4, NULL), "(collector) sigaction", c_clean);

      struct sigaction s5;
      memset(&s5, 0, sizeof(struct sigaction));
	s5.sa_handler = SIG_IGN;
	ec_meno1_c(sigaction(SIGPIPE, &s5, NULL), "(collector) sigaction", c_clean);


      //azzera signal mask
      sigset_t set;
	ec_meno1_c(sigemptyset(&set), "(collector) sigemptyset", c_clean);
      int err;
	if ((err = pthread_sigmask(SIG_SETMASK, &set, NULL)) != 0){
		LOG_ERR(err, "(collector) pthread_signmask");
		goto c_clean;
	}

      ///////////////// SOCKET /////////////////
      struct sockaddr_un sa;
	sa.sun_family = AF_UNIX;
	size_t len = strlen(SOCK_NAME);
	strncpy(sa.sun_path, SOCK_NAME, len);
	sa.sun_path[len] = '\0';

	if ((fd_skt = socket(AF_UNIX, SOCK_STREAM, 0)) == -1){
            LOG_ERR(errno, "(collector) socket");
            goto c_clean;
      }

      //timer connect setting and loop
	struct timespec time1, time2, delta;
      struct timespec abstime;
      memset(&abstime, 0, sizeof(struct timespec));
      memset(&time1, 0, sizeof(struct timespec));
      memset(&time2, 0, sizeof(struct timespec));
      memset(&delta, 0, sizeof(struct timespec));
	abstime.tv_sec = 10; //tempo assoluto 10 secondi

      if (clock_gettime(CLOCK_REALTIME, &time1) == -1){
            LOG_ERR(errno, "(collector) clock_gettime");
            goto c_clean;
      }
      while (connect(fd_skt, (struct sockaddr*)&sa, sizeof(sa)) == -1){
    	      if (errno == ENOENT){
                  msleep(SLEEP_TIME_MS);
                  // seconda misurazione
                  if (clock_gettime(CLOCK_REALTIME, &time2) == -1){
                        LOG_ERR(errno, "(collector) clock_gettime");
                        goto c_clean;
                  }
                  // calcola tempo trascorso tra time1 e time2 in delta
                  sub_timespec(time1, time2, &delta);
                  // controllo che non si sia superato il tempo assoluto dal primo tentativo di connessione
                  if ((delta.tv_nsec >= abstime.tv_nsec && delta.tv_sec >= abstime.tv_sec) || delta.tv_sec >= abstime.tv_sec){
                        LOG_ERR(ETIMEDOUT, "(collector) connect");
                        goto c_clean;
                  }
            }
      }
	//printf("(collector) socket: %s - attivo\n", sa.sun_path); //DEBUG

      //riceve: numero di file reg. in input al MW
      size_t tot_files = 0;
	read_n(fd_skt, (void*)sizet_buf, sizeof(size_t));
      tot_files = *sizet_buf;
      //invia: conferma ricezione
      *sizet_buf = 0;
	write_n(fd_skt, (void*)sizet_buf, sizeof(size_t));

      if (tot_files == 0) goto c_clean;
      arr = (elem*)calloc(sizeof(elem), tot_files);
      
      int rem_files = tot_files;
      int count = 0;
      int last_files = -1;
      size_t op;
      while (rem_files != 0 && last_files != 0){
            
            mutex_lock(&c_mtx, "(collector) lock");
            //ricezione op
            //riceve: operazione
            *sizet_buf = 0;
            read_n(fd_skt, (void*)sizet_buf, sizeof(size_t));
            op = *sizet_buf;
            //invia: conferma ricezione
            *sizet_buf = 0;
            write_n(fd_skt, (void*)sizet_buf, sizeof(size_t));

            //stampa i risultati fino a questo istante
            if (op == PRINT){
                  //ordina risultati attuali

                  size_t temp_index = tot_files - count;
                  qsort(arr, tot_files, sizeof(elem), cmp_func);
                  //stampa risultati
                  for (k = temp_index; k < tot_files; k++)
                        printf("%ld %s\n", arr[k].res, arr[k].path);
      
            }
            //ricezione di un nuovo risultato+path
            if (op == SEND_RES){
                  //riceve: risultato
                  long int result;
	            read_n(fd_skt, (void*)long_buf, sizeof(long int));
                  result = *long_buf;
                  //invia: conferma ricezione
                  *sizet_buf = 0;
	            write_n(fd_skt, (void*)sizet_buf, sizeof(size_t));

                  //riceve: lung. str
                  size_t len_s;
	            read_n(fd_skt, (void*)sizet_buf, sizeof(size_t));
                  len_s = *sizet_buf; //cast?
                  //invia: conferma ricezione
                  *sizet_buf = 0;
	            write_n(fd_skt, (void*)sizet_buf, sizeof(size_t));

                  //riceve: str
                  char* str = NULL;
                  str = calloc(len_s+1, sizeof(char));
	            read_n(fd_skt, (void*)str, sizeof(char)*len_s);
                  str[len_s] = '\0';
                  //invia: conferma ricezione
                  *sizet_buf = 0;
	            write_n(fd_skt, (void*)sizet_buf, sizeof(size_t));

                  //memorizza in array
                  strncpy(arr[count].path, str, len_s);
                  (arr[count].path)[len_s] = '\0';
                  arr[count].res = result;
                  //printf("save: result=%ld path:%s\n", result, str); //DEBUG
                  count++;
                  rem_files--;
                  if (str) free(str);
            }
            //chisura
            if (op == CLOSE){
                  //riceve: num. di elem. ancora da elaborare
	            read_n(fd_skt, (void*)sizet_buf, sizeof(size_t));
                  last_files = *sizet_buf;
                  //invia: conferma ricezione
                  *sizet_buf = 0;
	            write_n(fd_skt, (void*)sizet_buf, sizeof(size_t));
                  //printf("RICEVUTO SEGNALE DI CHIUSURA\n");
            }
            mutex_unlock(&c_mtx, "(collector) lock");
            //printf("COLLETOR LOOP\n");
      }
      //OUTPUT
      //calcola dim finale array di output
      size_t final_index;
      final_index = tot_files - count;

      //ordina risultati
      qsort(arr, tot_files, sizeof(elem), cmp_func);
      //stampa risultati
      for (k = final_index; k < tot_files; k++){
            printf("%ld %s\n", arr[k].res, arr[k].path);
      }
      
      if (arr) free(arr);
      if (fd_skt != -1) close(fd_skt);
      if (long_buf) free(long_buf);
      if (sizet_buf) free(sizet_buf);
      exit(EXIT_SUCCESS);
      
      //chiusura errore
      c_clean:
      if (arr) free(arr);
      if (fd_skt != -1) close(fd_skt);
      if (long_buf) free(long_buf);
      if (sizet_buf) free(sizet_buf);
      exit(EXIT_FAILURE);
}