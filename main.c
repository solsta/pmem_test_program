#include <stdio.h>
#include <libpmemobj/base.h>
#include <malloc.h>
#include <libpmemobj/pool_base.h>
#include <libpmemobj/types.h>
#include <library.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <libpmemobj/tx_base.h>
#include <stdlib.h>
#include <criu/criu.h>
#include <fcntl.h>
#include <jemalloc/jemalloc.h>
#include <signal.h>

void sm_op_begin(void);
void sm_op_end(void);

struct my_root {
    int number;
    char this_is_on_pmem[10];
};

void create_unique_file_name(char *path_to_pmem){
    strcat(path_to_pmem, "/mnt/dax/test_outputs/pmem_file_from_simple_pmem");
}

bool file_exists(const char *path){
    return access(path, F_OK) != 0;
}
static PMEMobjpool *pop;
char *get_some_pmem(size_t requested_pool_size){

    PMEMoid root;
    struct my_root *rootp;
    /* Change to a safe imlementation */
    char *path_to_pmem = calloc(200, 1);
    create_unique_file_name(path_to_pmem);
    size_t pool_size = requested_pool_size;
    printf("Requested pool size: %zu MB\n", pool_size/1000000);
    if(pool_size < PMEMOBJ_MIN_POOL){
        pool_size = PMEMOBJ_MIN_POOL;
    }

    if (file_exists((path_to_pmem)) != 0) {
        if ((pop = pmemobj_create(path_to_pmem, POBJ_LAYOUT_NAME(list),
                                  pool_size, 0666)) == NULL) {
            perror("failed to create pool\n");
        }
    } else {
        if ((pop = pmemobj_open(path_to_pmem, POBJ_LAYOUT_NAME(list))) == NULL) {
            perror("failed to open pool\n");
        }
    }
    root = pmemobj_root(pop, sizeof(struct my_root));
    rootp = pmemobj_direct(root);
    return rootp->this_is_on_pmem;
}
static void
log_stages(PMEMobjpool *pop_local, enum pobj_tx_stage stage, void *arg)
{
    /* Commenting this out because this is not required during normal execution. */
    /* dr_fprintf(STDERR, "cb stage: ", desc[stage], " "); */
}


void sm_op_begin(){
}
void sm_op_end(){
}

void sig_handler(int signum){
    printf("Handling signal \n");
    pmemobj_tx_abort(-1);
    exit(1);
}

void setup_criu(){
    long pid;
    pid = (long)getpid();
    printf("Pid is: %ld \n", pid);
    criu_init_opts();
    if(criu_set_service_address("../../crui_service/criu_service.socket")!=0){
            printf("Failed to set service address!\n");

        }

    int fd = open("../../criu_dump", O_DIRECTORY);
    criu_set_images_dir_fd(fd);
    criu_set_log_level(4);
    criu_set_leave_running(true);
    criu_set_log_file("restore.log");
    criu_set_shell_job(true);
    criu_set_ext_sharing(true);
    criu_set_file_locks(true);
    criu_set_evasive_devices(true);


    int res = criu_dump();
    if (res < 0) {
        printf("Failed to dump! \n");
    }
}

void test_non_instrumented_pmem(){
    printf("Non istrumented pmem. \n");
    char *pmem_test_string = get_some_pmem(100);
    char *initial_string = "FOO BAR";
    memcpy(pmem_test_string, initial_string, 8);

    setup_criu();
    printf("Initial value: %s \n", pmem_test_string);
    pmemobj_tx_begin(pop, NULL, TX_PARAM_CB, log_stages, NULL,
                     TX_PARAM_NONE);
    pmemobj_tx_add_range_direct(pmem_test_string,100);
    memcpy(pmem_test_string, "BAR",3);
    printf("Updated value: %s \n", pmem_test_string);
    /* Invoke failure here */
    pmemobj_tx_commit();
    pmemobj_tx_end();
}

void open_file_and_read_state_value(size_t requested_pool_size){
    PMEMoid root;
    struct my_root *rootp;
    /* Change to a safe imlementation */
    char *path_to_pmem = calloc(200, 1);
    create_unique_file_name(path_to_pmem);
    size_t pool_size = requested_pool_size;
    printf("Requested pool size: %zu MB\n", pool_size/1000000);
    if(pool_size < PMEMOBJ_MIN_POOL){
        pool_size = PMEMOBJ_MIN_POOL;
    }

    if (file_exists((path_to_pmem)) != 0) {
        if ((pop = pmemobj_create(path_to_pmem, POBJ_LAYOUT_NAME(list),
                                  pool_size, 0666)) == NULL) {
            perror("failed to create pool\n");
        }
    } else {
        if ((pop = pmemobj_open(path_to_pmem, POBJ_LAYOUT_NAME(list))) == NULL) {
            perror("failed to open pool\n");
        }
    }
}

void test_instrumented(){
    printf("Instrumented pmem. \n");
    char *test_string = malloc(sizeof 10);
    char *initial_string = "FOO BAR";
    memcpy(test_string, initial_string, 8);
    setup_criu();
    printf("Initial: %s \n",test_string);
    sm_op_begin();
    memcpy(test_string,"BAR",3);
    printf("Updated: %s \n",test_string);
    /* uncomment to see a roll back */
    //int pid=getpid();
    //kill(pid,SIGUSR1);

    sm_op_end();
}



void persistent_loop(){
    setup_criu();
    int *count = malloc(sizeof(int));
    *count=0;
    while(*count < 1000){
        sm_op_begin();
        printf("Count is %d \n", *count);
        *count = *count +1;
        sm_op_end();
        sleep(1);
    }
}

int main() {
    //setup_criu();
    //signal(SIGUSR1,sig_handler);
    //test_non_instrumented_pmem();
    //open_and_close_file(100);
    test_instrumented();

    return 0;
}
