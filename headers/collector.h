#include "header_file.h"
#include "error_ctrl.h"
#include "aux_function.h"


#define SOCK_NAME "farm.sck"
#define SLEEP_TIME_MS 400
//codifica operazioni
#define PRINT 1
#define SEND_RES 2
#define CLOSE 3

typedef struct _elem{
      char path[255];
      long int res;
}elem;

int qs_compare(const void*a, const void* b);
void collector();
