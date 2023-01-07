#include "header_file.h"
#include "error_ctrl.h"
#include "aux_function.h"
#include "master_thread.h"

#define PATH_LEN_MAX 255

int file_check(char* path);
void take_from_dir(const char* dirname);
int parser(int dim, char** array);
int main(int argc, char* argv[]);
