#include "header_file.h"
#include "error_ctrl.h"
#include "aux_function.h"

typedef struct _elem{
      char path[255];
      long int res;
}elem;


int cmp_func(const void*a, const void* b);
void collector();