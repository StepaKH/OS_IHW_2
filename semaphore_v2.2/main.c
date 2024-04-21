#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <signal.h>

#define DB_SIZE 10
#define SHM_PATH "/shm_example"

typedef struct {
    int data[DB_SIZE];
    sem_t sem;
} SharedData;

SharedData *shared_data;
int shm_fd;

unsigned long long factorial(int n) {
    if (n == 0) return 1;
    unsigned long long f = 1;
    for (int i = 1; i <= n; ++i) f *= i;
    return f;
}

void handle_sigint(int sig) {
    printf("Received SIGINT, cleaning up and exiting...\n");
    munmap(shared_data, sizeof(SharedData));
    close(shm_fd);
    shm_unlink(SHM_PATH);
    exit(0);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <number_of_processes>\n", argv[0]);
        return EXIT_FAILURE;
    }

    int num_processes = atoi(argv[1]);
    if (num_processes <= 0) {
        fprintf(stderr, "Number of processes must be greater than zero.\n");
        return EXIT_FAILURE;
    }

    signal(SIGINT, handle_sigint);

    shm_fd = shm_open(SHM_PATH, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("Failed to open shared memory");
        exit(EXIT_FAILURE);
    }
    ftruncate(shm_fd, sizeof(SharedData));
    shared_data = mmap(NULL, sizeof(SharedData), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shared_data == MAP_FAILED) {
        perror("Failed to map shared memory");
        exit(EXIT_FAILURE);
    }

    sem_init(&shared_data->sem, 1, 1);

    for (int i = 0; i < DB_SIZE; i++) {
        shared_data->data[i] = rand() % 20 + 1;
    }

    pid_t pid;

    for (int i = 0; i < num_processes; i++) {
        pid = fork();
        if (pid == 0) { // Дочерний процесс
            if (i % 2 == 0) { // Писатели
                while (1) {
                    sem_wait(&shared_data->sem);
                    int idx = rand() % DB_SIZE;
                    int old_val = shared_data->data[idx];
                    int new_val = rand() % 20 + 1;
                    shared_data->data[idx] = new_val;
                    printf("Writer %d: index %d, old value %d, new value %d\n", getpid(), idx, old_val, new_val);
                    sem_post(&shared_data->sem);
                    sleep(1);
                }
            } else { // Читатели
                while (1) {
                    int idx = rand() % DB_SIZE;
                    int val = shared_data->data[idx];
                    if (val <= 20) {
                        printf("Reader %d: index %d, value %d, factorial %llu\n", getpid(), idx, val, factorial(val));
                    } else {
                        printf("Reader %d: index %d, value %d, factorial too large\n", getpid(), idx, val);
                    }
                    sleep(2);
                }
            }
        }
    }
    while (wait(NULL) > 0);

    return 0;
}

