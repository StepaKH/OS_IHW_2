#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <signal.h>
#include <sys/shm.h>

#define DB_SIZE 10

typedef struct {
    int data[DB_SIZE];
} SharedData;

#define SEM_KEY 1234
#define SHM_KEY 5678

int sem_id;
int shm_id;
SharedData *shared_data;

unsigned long long factorial(int n) {
    if (n == 0) return 1;
    unsigned long long f = 1;
    for (int i = 1; i <= n; ++i) f *= i;
    return f;
}

void handle_sigint(int sig) {
    printf("Received SIGINT, cleaning up and exiting...\n");
    
    // Освобождение семафора
    semctl(sem_id, 0, IPC_RMID, 0);

    // Отключение и удаление разделяемой памяти
    shmdt(shared_data);
    shmctl(shm_id, IPC_RMID, NULL);

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

    // Создание семафора
    sem_id = semget(SEM_KEY, 1, IPC_CREAT | 0666);
    if (sem_id == -1) {
        perror("Failed to create semaphore");
        exit(EXIT_FAILURE);
    }
    semctl(sem_id, 0, SETVAL, 1); // Установка начального значения семафора

    // Создание или подключение к разделяемой памяти
    shm_id = shmget(SHM_KEY, sizeof(SharedData), IPC_CREAT | 0666);
    if (shm_id == -1) {
        perror("Failed to create shared memory");
        exit(EXIT_FAILURE);
    }
    shared_data = (SharedData *) shmat(shm_id, NULL, 0);
    if (shared_data == (void *) -1) {
        perror("Failed to attach shared memory");
        exit(EXIT_FAILURE);
    }

    // Инициализация данных в разделяемой памяти
    for (int i = 0; i < DB_SIZE; i++) {
        shared_data->data[i] = rand() % 20 + 1;
    }

    pid_t pid;

    // Создание указанного числа процессов
    for (int i = 0; i < num_processes; i++) {
        pid = fork();
        if (pid == 0) { // Дочерний процесс
            if (i % 2 == 0) { // Писатели
                while (1) {
                    struct sembuf sb;
                    sb.sem_num = 0;
                    sb.sem_op = -1; // Уменьшаем счетчик семафора на 1
                    sb.sem_flg = SEM_UNDO;
                    semop(sem_id, &sb, 1); // Захватываем семафор

                    int idx = rand() % DB_SIZE;
                    int old_val = shared_data->data[idx];
                    int new_val = rand() % 20 + 1;
                    shared_data->data[idx] = new_val;
                    printf("Writer %d: index %d, old value %d, new value %d\n", getpid(), idx, old_val, new_val);

                    sb.sem_op = 1; // Увеличиваем счетчик семафора на 1
                    semop(sem_id, &sb, 1); // Освобождаем семафор

                    sleep(1);
                }
            } else { // Читатели
                while (1) {
                    struct sembuf sb;
                    sb.sem_num = 0;
                    sb.sem_op = -1; // Уменьшаем счетчик семафора на 1
                    sb.sem_flg = SEM_UNDO;
                    semop(sem_id, &sb, 1); // Захватываем семафор

                    int idx = rand() % DB_SIZE;
                    int val = shared_data->data[idx];
                    if (val <= 20) {
                        printf("Reader %d: index %d, value %d, factorial %llu\n", getpid(), idx, val, factorial(val));
                    } else {
                        printf("Reader %d: index %d, value %d, factorial too large\n", getpid(), idx, val);
                    }

                    sb.sem_op = 1; // Увеличиваем счетчик семафора на 1
                    semop(sem_id, &sb, 1); // Освобождаем семафор

                    sleep(2);
                }
            }
        }
    }

    while (wait(NULL) > 0);

    return 0;
}