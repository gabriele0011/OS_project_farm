#include "header_file.h"
#include "error_ctrl.h"
#include "list.h"
#include "global_var.h"
#include "pool_worker.h"
#include "collector.h"

extern t_queue* conc_queue;
extern node* files_list;

void MasterWorker();