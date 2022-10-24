#include "aux_function.h"


enum { NS_PER_SECOND = 1000000000 };

int msleep(long int msec) 
{
    struct timespec ts;
    int res;

    if (msec < 0){
        errno = EINVAL;
        return -1;
    }
    ts.tv_sec = msec/1000;
    ts.tv_nsec = (msec % 1000) * 1000000;

    do {
        res = nanosleep(&ts, &ts);
    } while (res && errno == EINTR);
    
    return res;
}

int is_opt( char* arg, char* opt)
{
	size_t n = strlen(arg);
	if(strncmp(arg, opt, n) == 0) return 1;
	else return 0;
}
int is_argument(char* c)
{
	if (c == NULL || (*c == '-') ) return 0;
	else return 1;
}
int is_directory(const char *path)
{
    struct stat path_stat;
    if (lstat(path, &path_stat) == -1) return -1;
    return S_ISDIR(path_stat.st_mode);
}

long is_number(const char* s)
{
	char* e = NULL;
   	long val = strtol(s, &e, 0);
   	if (e != NULL && *e == (char)0) return val; 
	return -1;
}
void read_n(int fd, void* buf, size_t bytes)
{
	int N;
	N = read(fd, buf, bytes);
	if (N != bytes && errno != EINTR){
		LOG_ERR(errno, "read fallita");
		exit(EXIT_FAILURE);
	}
	return;
}

void write_n(int fd, long int* buf, size_t bytes)
{
	int N;
	N = write(fd, buf, bytes);
	if (N != bytes && errno != EINTR){
		LOG_ERR(errno, "write fallita");
		exit(EXIT_FAILURE);
	}
	return;
}
