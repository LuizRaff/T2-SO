/* Includes */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <sys/wait.h>
#include <sys/stat.h> /* For mode constants */
#include <string.h>
#include <time.h>

/* Defines */
#define NUM_PROCESS 4
#define NUM_PAGES 32
#define NUM_ACCESS 120
#define MEMORY_SIZE 16
#define NUM_ROUNDS 50
#define SHM_NAME "/my_shared_memory"
#define SEM_NAME "/my_semaphore"
#define SHM_SIZE sizeof(SharedData)

#define RANDOM_PAGE() (rand() % (NUM_PAGES))
#define RANDOM_ACCESS() (rand() % 2)

/* Estrutura para os dados compartilhados */
typedef struct
{
    int flag;
    int pageNum;
    char accessType;
} SharedData;

void gmv(int algorithm, int set, int printFlag);

/* Access Log Generator Declaration */
int accessLogsGen(char **paths);

/* Page Algorithms Declarations */
int subs_NRU(int memorySeg);         // Not Recently Used (NRU)
int subs_2nCh(int memorySeg);        // Second Chance
int subs_LRU(int memorySeg);         // Aging (LRU)
int subs_WS(int set, int memorySeg); // Working Set (k)

int main(int argc, char **argv)
{
    int algorithm;

    // ARGV error checking
    if (argc < 3)
    {
        fprintf(stderr, "Usage ERROR: <Print Flag> <algorithm> {if WS}:[parameters]\n");
        exit(EXIT_FAILURE);
    }

    if (strcmp(argv[2], "NRU") == 0)
    {
        algorithm = 0;
    }
    else if (strcmp(argv[2], "2nCH") == 0)
    {
        algorithm = 1;
    }
    else if (strcmp(argv[2], "LRU") == 0)
    {
        algorithm = 2;
    }
    else if (strcmp(argv[2], "WS") == 0)
    {
        algorithm = 3;
    }
    else if (strcmp(argv[2], "REFRESH") == 0)
    {
        algorithm = 4;
    }
    else
    {
        fprintf(stderr, "Invalid algorithm. Please choose between NRU, 2nCH, LRU or WS.\n");
        exit(EXIT_FAILURE);
    }

    if (atoi(argv[1]) != 0 && atoi(argv[1]) != 1)
    {
        fprintf(stderr, "Invalid print flag. Please choose between 0 or 1.\n");
        exit(EXIT_FAILURE);
    }

    // Paths for access logs files
    char *processesPaths[NUM_PROCESS] = {"P1_AccessesLog.txt",
                                         "P2_AccessesLog.txt",
                                         "P3_AccessesLog.txt",
                                         "P4_AccessesLog.txt"};

    // Print Flag Declaration / Instantiation
    int printFlag = atoi(argv[1]);

    srand(time(NULL) ^ getpid());
    int shm_fd;
    SharedData *shared_data;
    sem_t *sem;

    // Remove o objeto de memória compartilhada caso exista
    shm_unlink(SHM_NAME);

    // Cria ou abre a memória compartilhada POSIX
    shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1)
    {
        perror("shm_open falhou");
        exit(EXIT_FAILURE);
    }

    // Define o tamanho da memória compartilhada
    if (ftruncate(shm_fd, SHM_SIZE) == -1)
    {
        perror("ftruncate falhou");
        shm_unlink(SHM_NAME);
        exit(EXIT_FAILURE);
    }

    // Mapeia a memória compartilhada
    shared_data = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shared_data == MAP_FAILED)
    {
        perror("mmap falhou");
        shm_unlink(SHM_NAME);
        exit(EXIT_FAILURE);
    }

    // Inicializa a estrutura compartilhada
    shared_data->flag = 0;

    // Remove o semáforo caso exista
    sem_unlink(SEM_NAME);

    // Cria o semáforo
    sem = sem_open(SEM_NAME, O_CREAT, 0644, 1);
    if (sem == SEM_FAILED)
    {
        perror("sem_open falhou");
        shm_unlink(SHM_NAME);
        munmap(shared_data, SHM_SIZE);
        exit(EXIT_FAILURE);
    }

    // Access Log Generator
    if (strcmp(argv[2], "REFRESH") == 0)
    {
        // Running page replacement algorithms
        printf("Gerando novos logs de accesso...\n");
        usleep(500000);
        // Generating process access logs
        if (accessLogsGen(processesPaths) == 1)
        {
            perror("Error when loading access logs");
            exit(1);
        }
    }
    else
    {
        pid_t pid = fork();
        if (pid == 0)
        {
            // Processo filho
            sem_close(sem); // Fecha o semáforo no processo filho (será reaberto em gmv)
            gmv(algorithm, atoi(argv[3]), printFlag);
            exit(0);
        }
        else if (pid > 0)
        {
            // Processo pai

            // Abrindo os arquivos
            FILE *files[NUM_PROCESS];
            for (int i = 0; i < NUM_PROCESS; i++)
            {
                files[i] = fopen(processesPaths[i], "r");
                if (files[i] == NULL)
                {
                    perror("Erro ao abrir os arquivos de log");
                    for (int j = 0; j < i; j++)
                    {
                        fclose(files[j]);
                    }
                    exit(EXIT_FAILURE);
                }
            }
            printf("Arquivos abertos com sucesso!\n\n");
            usleep(1000000);

            printf("Algoritmo Escolhido: %s\n", argv[2]);
            usleep(500000);
            if (strcmp(argv[2], "WS") == 0)
            {
                if (argc > 3)
                {
                    printf("Parâmetro do Working Set: %s\n", argv[3]);
                }
                else
                {
                    fprintf(stderr, "Missing parameter k for Working Set algorithm. Please provide a valid integer.\n");
                    exit(EXIT_FAILURE);
                }
            }

            usleep(500000);
            printf("Numero de rodadas: %d\n\n", NUM_ROUNDS);
            usleep(500000);

            int totalAccesses = 0;
            char accessType;
            int pageNum;
            int aux;
            while (1)
            {
                aux = fscanf(files[totalAccesses % NUM_PROCESS], "%d %c", &pageNum, &accessType);
                if (aux != 2)
                {
                    break;
                }
                sem_wait(sem);
                if (shared_data->flag == 1)
                {
                    usleep(500000);
                    shared_data->flag = 0;
                    shared_data->pageNum = pageNum;
                    shared_data->accessType = accessType;
                }
                sem_post(sem);
                totalAccesses++;
                usleep(100000); // Pequena espera para evitar loop rápido demais
            }
            sem_wait(sem);
            shared_data->flag = 2; // Sinaliza para o filho terminar
            sem_post(sem);

            // Aguarda o filho terminar
            wait(NULL);

            // Limpa recursos
            sem_close(sem);
            sem_unlink(SEM_NAME);
            munmap(shared_data, SHM_SIZE);
            shm_unlink(SHM_NAME);

            for (int j = 0; j < NUM_PROCESS; j++)
            {
                fclose(files[j]);
            }
        }
        else
        {
            perror("fork falhou");
            exit(EXIT_FAILURE);
        }
    }

    return 0;
}

void gmv(int algorithm, int set, int printFlag)
{
    srand(time(NULL) ^ getpid());
    int shm_fd;
    SharedData *shared_data;
    sem_t *sem;

    // Abre a memória compartilhada existente
    shm_fd = shm_open(SHM_NAME, O_RDWR, 0666);
    if (shm_fd == -1)
    {
        perror("shm_open falhou no filho");
        exit(EXIT_FAILURE);
    }

    // Mapeia a memória compartilhada
    shared_data = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shared_data == MAP_FAILED)
    {
        perror("mmap falhou no filho");
        exit(EXIT_FAILURE);
    }

    // Abre o semáforo existente
    sem = sem_open(SEM_NAME, 0);
    if (sem == SEM_FAILED)
    {
        perror("sem_open falhou no filho");
        munmap(shared_data, SHM_SIZE);
        exit(EXIT_FAILURE);
    }

    printf("Filho iniciado\n");

    while (1)
    {
        sem_wait(sem);
        if (shared_data->flag == 0)
        {
            usleep(500000);
            shared_data->flag = 1;
            printf("Acesso: %d %c\n", shared_data->pageNum, shared_data->accessType);
        }
        else if (shared_data->flag == 2)
        {
            sem_post(sem);
            break; // Sinal para terminar
        }
        sem_post(sem);
        usleep(100000); // Pequena espera para evitar loop rápido demais
    }

    // Limpa recursos
    sem_close(sem);
    munmap(shared_data, SHM_SIZE);
    printf("Filho terminou\n");
}

/************************************************************************************************/
/* Access Log Generator */
int accessLogsGen(char **paths)
{
    FILE *file;
    srand(time(NULL));

    for (int i = 0; i < NUM_PROCESS; i++)
    {
        file = fopen(paths[i], "a+");
        if (file == NULL)
        {
            return 1;
        }
        else
        {
            for (int j = 0; j < NUM_ACCESS; j++)
            {
                if (RANDOM_ACCESS())
                { // Read Access
                    fprintf(file, "%d R\n", RANDOM_PAGE());
                }
                else
                { // Write Access
                    fprintf(file, "%d W\n", RANDOM_PAGE());
                }
            }
        }

        fclose(file);
    }

    return 0;
}
/************************************************************************************************/

/* Page Algorithms */
/************************************************************************************************/
/* Not Recently Used (NRU) */
int subs_NRU(int memorySeg)
{
    return -1;
};
/************************************************************************************************/

/************************************************************************************************/
/* Second Chance */
int subs_2nCh(int memorySeg)
{
    return -1;
};
/************************************************************************************************/

/************************************************************************************************/
/* Agin (LRU) */
int subs_LRU(int memorySeg)
{
    return -1;
};
/************************************************************************************************/

/************************************************************************************************/
/* Working Set (K) */
int subs_WS(int set, int memorySeg)
{
    return -1;
};
/************************************************************************************************/