#include "header_file.h"
#include "error_ctrl.h"


//funzioni ausiliarie
long is_number(const char* s);
int is_opt(char* arg, char* opt);
int is_argument(char* c);
int is_directory(const char *path);
int msleep(long int msec) 
void read_n(int fd, long int* buf, size_t bytes);
void write_n(int fd, long int* buf, size_t bytes)
