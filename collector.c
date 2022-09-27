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
      long int* buf = NULL;
      elem arr[tot_files]; //array che conterrà tutti i risultati
      int i = 0;
      int j = tot_files;



     	char socket_name[8];
	strcpy(socket_name, "farm.sck");
	socket_name[7] = '\0';
      struct sockaddr_un sa;
	size_t len_sockname =  strlen(sockname);
	memset((void*)sa.sun_path, '\0', len_sockname);
	strncpy(sa.sun_path, sockname, len_sockname);
	sa.sun_family = AF_UNIX;

      printf("socket name: %s\n", socket_name);
    
	if ((fd_skt = socket(AF_UNIX, SOCK_STREAM, 0)) == -1){
            LOG_ERR(errno, "socket()");
            return -1;
      }
      if (connect(fd_skt, (struct sockaddr*)&sa, sizeof(sa)) == -1){
            LOG_ERR(errno, "(collector) connect");
            exit(EXIT_FAILURE);
      }
      
      //gestione dei segnali -> ignora tutti -> 
      //gli handler sono reimpostati a default dopo la execl tranne quelli ignorati
      s.sa_handler = SIG_IGN;
      s1.sa_handler = SIG_IGN;
      s2.sa_handler = SIG_IGN;
      s3.sa_handler = SIG_IGN;
      s4.sa_handler = SIG_IGN;
      //s5.sa_handler = SIG_IGN;



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

      return 0;
}