#include "header_file.h"
#include "error_ctrl.h"
#include "aux_function.h"


typedef struct _elem{
      char path[255];
      long int res;
}elem;

int fd_skt;

int main()
{
      printf("(collector) SONO NATO! pid=%d\n", getpid());
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
      printf("(collector) socket: %s - attivo\n", socket_name);


      size_t tot_files = 0;
      printf("(collector) ricevo tot_files=%zu\n", tot_files);
      //riceve: tot_files
      int n;
      printf("BUG HERE\n");
	n = read(fd_skt, buf, sizeof(size_t));
      printf("(collector) byte letti=%d\n", n);
      tot_files = *buf;
      //invia: conferma ricezione
      *buf = 0;
	write(fd_skt, buf, sizeof(long int));
      printf("(collector) BUG HERE\n");
      printf("(collector) ho ricevuto tot_files=%ld\n", tot_files);

      //elem arr[tot_files]; //array che conterra tutti i risultati
      elem* arr = NULL;
      arr = (elem*)calloc(sizeof(elem), tot_files);

      int j = tot_files;

      while(j > 0){
            //riceve: operazione
            read(fd_skt, buf, sizeof(long int));

            //stampa i risultati fino a questo istante
            if (*buf == 1){
                  //MUTEX
                  //ordina array
                  //stampa array fino al j esimo
                  for(int k = 0; k < j; k++)
                        printf("%ld %s\n", arr[k].res, arr[k].path);
                  //MUTEX
            }

            //ricezione di un nuovo risultato+path
            if(*buf == 2){
                  //riceve: risultato
                  long int result;
	            read(fd_skt, buf, sizeof(long int));
                  result = *buf;
                  //invia: conferma ricezione
                  *buf = 0;
	            write(fd_skt, buf, sizeof(long int));

                  //riceve: lung. str
                  size_t len_s;
	            read(fd_skt, buf, sizeof(long int));
                  len_s = *buf; //cast?
                  //invia: conferma ricezione
                  *buf = 0;
	            write(fd_skt, buf, sizeof(long int));

                  //riceve: str
                  char* str = NULL;
                  str = calloc(sizeof(char), len_s);
	            read(fd_skt, &str, sizeof(char)*len_s);
                  //invia: conferma ricezione
                  *buf = 0;
	            write(fd_skt, buf, sizeof(long int));

                  //memorizza dato nell'array
                  strncpy(arr[i].path, str, len_s);
                  (arr[i].path)[len_s] = '\0';
                  arr[i].res = result; 
                  i++; j--;
            }
      }
      if(buf) free(buf);
      return 0;
      
      main_clean:
      if(buf) free(buf);
      return -1;
}