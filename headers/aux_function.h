#include "header_file.h"
#include "error_ctrl.h"

enum{ NS_PER_SECOND = 1000000000 };

//funzioni ausiliarie
void sub_timespec(struct timespec t1, struct timespec t2, struct timespec *td);
long is_number(const char* s);
int is_opt(char* arg, char* opt);
int is_argument(char* c);
int is_directory(const char *path);
int msleep(long int msec); 
void read_n(int fd, void* buf, size_t bytes);
void write_n(int fd, void* buf, size_t bytes);
