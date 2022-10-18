#include "collector.h"
#define SOCK_NAME "farm.sck"


int cmp_func(const void*a, const void* b)
{
      const elem* x = a;
      const elem* y = b;
      if (x->res >= y->res) return 1;
      else return -1;
}


void collector()
{
      //printf("(collector) pid=%d\n", getpid());
      long int* buf = NULL;
	buf = (long int*)malloc(sizeof(long int));

      ///////////////// SOCKET /////////////////
	int sockfd;
      struct sockaddr_un sa;
	sa.sun_family = AF_UNIX;
	size_t len = strlen(SOCK_NAME);
	strncpy(sa.sun_path, SOCK_NAME, len);
	sa.sun_path[len] = '\0';

	if ((sockfd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1){
            LOG_ERR(errno, "(collector) socket");
            goto main_clean;
      }
      if (connect(sockfd, (struct sockaddr*)&sa, sizeof(sa)) == -1){
	    LOG_ERR(errno, "(collector) connect");
          goto main_clean;
      }
	//printf("(collector) socket: %s - attivo\n", sa.sun_path); //DEBUG

      ///////////////// SEGNALI /////////////////
      struct sigaction s;
	memset(&s, 0, sizeof(struct sigaction));
      s.sa_handler = SIG_IGN;
	ec_meno1_c(sigaction(SIGINT, &s, NULL), "(main) sigaction fallita", main_clean);
	
	struct sigaction s1;
	memset(&s1, 0, sizeof(struct sigaction));
      s1.sa_handler = SIG_IGN;
      ec_meno1_c(sigaction(SIGQUIT, &s1, NULL), "(main) sigaction fallita", main_clean);
	
	struct sigaction s2;
	memset(&s2, 0, sizeof(struct sigaction));
      s2.sa_handler = SIG_IGN;
	ec_meno1_c(sigaction(SIGHUP, &s2, NULL), "(main) sigaction fallita", main_clean); 

	struct sigaction s3;
	memset(&s3, 0, sizeof(struct sigaction));
      s3.sa_handler = SIG_IGN;
	ec_meno1_c(sigaction(SIGTERM, &s3, NULL), "(main) sigaction fallita", main_clean);  

	struct sigaction s4;
	memset(&s4, 0, sizeof(struct sigaction));
      s4.sa_handler = SIG_IGN;
	ec_meno1_c(sigaction(SIGUSR1, &s4, NULL), "(main) sigaction fallita", main_clean);

	struct sigaction s5;
	memset(&s5, 0, sizeof(struct sigaction));
	s5.sa_handler = SIG_IGN;
    

      //COMUNICA A COLLECTOR numero di file in input
      size_t tot_file_num = 0;
      //riceve: tot_file_num
	readn(sockfd, buf, sizeof(size_t));
      tot_file_num = *buf;
      //invia: conferma ricezione
      *buf = 0;
	writen(sockfd, buf, sizeof(size_t));

      //elem arr[tot_file_num]; //array che conterra tutti i risultati
      elem* arr = NULL;
      arr = (elem*)calloc(sizeof(elem), tot_file_num);
      if (arr == NULL){
            LOG_ERR(errno, "calloc");
            exit(EXIT_FAILURE);
      }
      
      int rest_files = -1;
      int i = 0;
      int j = tot_file_num;
      size_t op;
      while(j > 0 && rest_files != 0){
            //riceve: operazione
            *buf = 0;
            readn(sockfd, buf, sizeof(size_t));
            op = *buf;
            //invia: conferma ricezione
            *buf = 0;
            writen(sockfd, buf, sizeof(size_t));
           //printf("op=%zu RICEVUTA\n", op); //DEBUG

            //stampa i risultati fino a questo istante
            if (op == 1){
                  //printf("(collector) OPERAZIONE 1\n"); //DEBUG
                  //ordinamento
                  qsort(arr, tot_file_num, sizeof(elem), cmp_func);
                  //stampa array fino al j-esimo
                  for(int k = 0; k < j; k++)
                        printf("%ld %s\n", arr[k].res, arr[k].path);
                  //MUTEX
            }

            //ricezione di un nuovo risultato e path
            if (op == 2){
                  //riceve: risultato
                  long int result;
	            readn(sockfd, buf, sizeof(long int));
                  result = *buf;
                  //invia: conferma ricezione
                  *buf = 0;
	            writen(sockfd, buf, sizeof(size_t));

                  //riceve: lung. str
                  size_t len_s;
	            readn(sockfd, buf, sizeof(size_t));
                  len_s = *buf; //cast?
                  //invia: conferma ricezione
                  *buf = 0;
	            writen(sockfd, buf, sizeof(size_t));

                  //riceve: str
                  char* str = NULL;
                  str = calloc(len_s+1, sizeof(char));
	            readn(sockfd, str, sizeof(char)*len_s);
                  str[len_s] = '\0';
                  //invia: conferma ricezione
                  *buf = 0;
	            writen(sockfd, buf, sizeof(size_t));

                  //printf("(collector) ricevuto 1) res=%ld - 2) len_s=%zu - 3) str:%s\n", result, len_s, str);
                  
                  //memorizza in array
                  strncpy(arr[i].path, str, len_s);
                  (arr[i].path)[len_s] = '\0';
                  arr[i].res = result;
                  i++; j--;
                  if (str) free(str);
                  if (rest_files > 0) rest_files--;
            }
            //segnale di chisura
            //riceve il numero di elementi rimasti in coda da elaborare
            if (op == 3){
                  //riceve: remain
                  rest_files;
	            readn(sockfd, buf, sizeof(size_t));
                  rest_files = *buf; //cast?
                  //invia: conferma ricezione
                  *buf = 0;
	            writen(sockfd, buf, sizeof(size_t));
                 //printf("RICEVUTO rest_files=%d\n", rest_files);
            }

      }
      printf("(collector) stampa risultati\n"); //DEBUG
      
      size_t final_dim_arr = tot_file_num - i;

      //ordina risultati
      qsort(arr, tot_file_num, sizeof(elem), cmp_func);
      
      //stampa risultati
      for(int k = final_dim_arr; k < tot_file_num; k++){
            printf("%ld %s\n", arr[k].res, arr[k].path);
      }
      
      //chiusura normale
      if (arr) free(arr);
      if (buf) free(buf);
      printf("(collector) terminazione SUCCESSO\n"); //DEBUG
      exit(EXIT_SUCCESS);
      
      //chiusura errore
      main_clean:
      //if (arr) free(arr);
      if (buf) free(buf);
      printf("(collector) terminazione FALLITO\n"); //DEBUG
      exit(EXIT_FAILURE);
}