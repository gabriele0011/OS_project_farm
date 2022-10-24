#define PRINT 1
#define SEND_RES 2
#define CLOSE 3

extern int fd;
extern int fd_skt;

//extern t_queue* conc_queue;
extern size_t tot_files;
extern size_t q_curr_capacity;

//parser global var
extern size_t n_thread;
extern size_t q_len;
extern char* dir_name;
extern int ms_delay;

//segnali
extern volatile sig_atomic_t sig_usr1;
extern volatile sig_atomic_t closing;
extern volatile sig_atomic_t child_term;


//var. mutex e cond.
extern pthread_mutex_t mtx;
extern pthread_cond_t cv;
extern pthread_mutex_t op_mtx;

// var. per pipe
extern int fd_pipe_read;
extern int fd_pipe_write;