#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>

#define DB_SIZE 10
#define SEM_WRITER "/sem_writer"
#define SHM_PATH "/shm_example"

int shm_fd;
int *database;
sem_t *writer_sem;

// Функция факториала
unsigned long long factorial(int n) {
    if (n == 0) return 1;
    unsigned long long f = 1;
    for (int i = 1; i <= n; ++i) f *= i;
    return f;
}

// Обработчик сигнала SIGINT для очистки ресурсов
void handle_sigint(int sig) {
    printf("Received SIGINT, cleaning up and exiting...\n");

    munmap(database, DB_SIZE * sizeof(int));
    close(shm_fd);
    shm_unlink(SHM_PATH);
    sem_close(writer_sem);
    sem_unlink(SEM_WRITER);

    exit(0);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <number_of_processes>\n", argv[0]);
        return 1;
    }

    int num_processes = atoi(argv[1]);
    if (num_processes <= 0) {
        fprintf(stderr, "The number of processes must be greater than 0.\n");
        return 1;
    }

    signal(SIGINT, handle_sigint);

    // Создание и открытие семафора
    writer_sem = sem_open(SEM_WRITER, O_CREAT | O_EXCL, 0666, 1);
    if (writer_sem == SEM_FAILED) {
        perror("Failed to open semaphore");
        exit(1);
    }

    // Создание и открытие разделяемой памяти
    shm_fd = shm_open(SHM_PATH, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("Failed to open shared memory");
        exit(1);
    }

    // Установка размера разделяемой памяти
    ftruncate(shm_fd, DB_SIZE * sizeof(int));

    // Отображение разделяемой памяти
    database = mmap(NULL, DB_SIZE * sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (database == MAP_FAILED) {
        perror("Failed to map shared memory");
        exit(1);
    }

    // Инициализация данных в базе
    for (int i = 0; i < DB_SIZE; i++) {
        database[i] = rand() % 20 + 1;
    }

    pid_t pid;

    // Создание указанного числа процессов
    for (int i = 0; i < num_processes; i++) {
        pid = fork();
        if (pid == 0) { // Дочерний процесс
            if (i % 2 == 0) { // Писатели
                while (1) {
                    sem_wait(writer_sem);
                    int idx = rand() % DB_SIZE;
                    int old_val = database[idx];
                    int new_val = rand() % 20 + 1;
                    database[idx] = new_val;
                    printf("Writer %d: index %d, old value %d, new value %d\n", getpid(), idx, old_val, new_val);
                    sem_post(writer_sem);
                    sleep(1);
                }
            } else { // Читатели
                while (1) {
                    int idx = rand() % DB_SIZE;
                    int val = database[idx];
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
