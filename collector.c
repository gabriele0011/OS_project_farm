#include "header_file.h"
#include "error_ctrl.h"
#include "aux_function.h"


typedef struct _elem{
      char path[255];
      long int res;
}elem;

int fd_skt;
int readn(int fd, void* buf, size_t bytes)
{
	int N;
	N = read(fd, buf, bytes);
	if (N != bytes){
		LOG_ERR(errno, "write fallita");
		return -1;
	}
	return 0;
}


int writen(int fd, void* buf, size_t bytes)
{
	int N;
	N = write(fd, buf, bytes);
	if (N != bytes){
		LOG_ERR(errno, "write fallita");
		return -1;
	}
	return 0;
}
int main()
{
      //printf("(collector) pid=%d\n", getpid());
      long int* buf = NULL;
	buf = (long int*)malloc(sizeof(long int));
      int i = 0;

      ///////////////// SEGNALI /////////////////
      struct sigaction s;
	memset(&s, 0, sizeof(struct sigaction));
      s.sa_handler = SIG_IGN;
	ec_meno1_c(sigaction(SIGINT, &s, NULL), "(main) sigaction fallita", main_clean);
	
	struct sigaction s1;
	memset(&s1, 0, sizeof(struct sigaction));
      s1.sa_handler = SIG_IGN;
	if (sigaction(SIGQUIT, &s1, NULL)  == -1){
            LOG_ERR(errno, "(main) sigaction fallita");
            goto main_clean;
      }

      //ec_meno1_c(sigaction(SIGQUIT, &s1, NULL), "(main) sigaction fallita", main_clean);
	
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
    
      
      ///////////////// SOCKET /////////////////
	char socket_name[9];
	strcpy(socket_name, "farm.sck");
	socket_name[8] = '\0';
      struct sockaddr_un sa;
	size_t len_sockname =  strlen(socket_name);
	memset((void*)sa.sun_path, '\0', len_sockname);
	strncpy(sa.sun_path, socket_name, len_sockname);
	sa.sun_family = AF_UNIX;
    
	if ((fd_skt = socket(AF_UNIX, SOCK_STREAM, 0)) == -1){
            LOG_ERR(errno, "socket()");
            goto main_clean;
      }
      if (connect(fd_skt, (struct sockaddr*)&sa, sizeof(sa)) == -1){
	    LOG_ERR(errno, "(collector) connect");
          goto main_clean;
      }
      //printf("(collector) socket: %s - attivo\n", socket_name);

      //COMUNICA A COLLECTOR numero di file in input
      size_t tot_files = 0;
      //riceve: tot_files
      int n;
	n = read(fd_skt, buf, sizeof(size_t));
      //printf("(collector) byte letti=%d\n", n);
      tot_files = *buf;
      //invia: conferma ricezione
      *buf = 0;
	write(fd_skt, buf, sizeof(long int));
      printf("(collector) ho ricevuto tot_files=%ld\n", tot_files);

      //elem arr[tot_files]; //array che conterra tutti i risultati
      elem* arr = NULL;
      arr = (elem*)calloc(sizeof(elem), tot_files);

      int j = tot_files;
      size_t op;
      while(j > 0){
            //riceve: operazione
            *buf = 0;
            readn(fd_skt, buf, sizeof(long int));
            op = *buf;
            //invia: conferma ricezione
            *buf = 0;
            writen(fd_skt, buf, sizeof(long int));
            printf("op=%zu RICEVUTA\n", op);

            //stampa i risultati fino a questo istante
            if (op == 1){
                  printf("(collector) OPERAZIONE 1\n");
                  //MUTEX
                  //ordina array
                  //stampa array fino al j esimo
                  for(int k = 0; k < j; k++)
                        printf("%ld %s\n", arr[k].res, arr[k].path);
                  //MUTEX
            }

            //ricezione di un nuovo risultato+path
            if (op == 2){
                  //riceve: risultato
                  long int result;
	            readn(fd_skt, buf, sizeof(long int));
                  result = *buf;
                  //invia: conferma ricezione
                  *buf = 0;
	            writen(fd_skt, buf, sizeof(size_t));

                  //riceve: lung. str
                  size_t len_s;
	            readn(fd_skt, buf, sizeof(size_t));
                  len_s = *buf; //cast?
                  //invia: conferma ricezione
                  *buf = 0;
	            writen(fd_skt, buf, sizeof(size_t));

                  //riceve: str
                  char* str = NULL;
                  str = calloc(sizeof(char), len_s);
	            readn(fd_skt, str, sizeof(char)*len_s);
                  //invia: conferma ricezione
                  *buf = 0;
	            writen(fd_skt, buf, sizeof(size_t));

                  //printf("(collector) ricevuto 1) res=%ld - 2) len_s=%zu - 3) str:%s\n", result, len_s, str);
                  
                  //memorizza in array
                  strncpy(arr[i].path, str, len_s);
                  (arr[i].path)[len_s] = '\0';
                  arr[i].res = result; 
                  i++; j--;
            }
      }
      printf("(collector) terminazione\n");
      printf("(collector) stampa risultati\n");
      for(int k = 0; k < tot_files; k++)
            printf("%ld %s\n", arr[k].res, arr[k].path);
      if(buf) free(buf);
      return 0;
      
      main_clean:
      if(buf) free(buf);
      return -1;
}