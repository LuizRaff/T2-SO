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

typedef struct
{
    int memoryLRU[MEMORY_SIZE];
    int size;
    int reference_bitsLRU[MEMORY_SIZE];
    int modified_bitsLRU[MEMORY_SIZE];
} LRU_Fila;

/* Funções dos algoritmos de substituição de página */
void subs_NRU(int *memory, int *pageReferenced, int *pageModified, int *pageFault, int pageNum, char accessType);   // Not Recently Used (NRU)
void subs_2nCh(int pageNum, char accessType);                                                                       // Second Chance
void subs_LRU(LRU_Fila *lru_Fila, int *pageFault, int pageNum, char accessType);                                    // Aging (LRU)
void subs_WS(int set, int pageNum, char accessType);                                                                // Working Set (k)

/* Gerenciador de Memória Virtual */
void gmv(int algorithm, int set, int printFlag);

/* Gerador de Logs de Acesso */
int accessLogsGen(char **paths, int process);

int main(int argc, char **argv)
{
    int algorithm;

    // Verificação dos argumentos
    if (argc < 3)
    {
        fprintf(stderr, "Uso: %s <Print Flag> <Algorithm> [Parameters]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Seleção do algoritmo
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
        algorithm = -1; // Código especial para gerar novos logs
    }
    else
    {
        fprintf(stderr, "Invalid algorithm. Please choose between NRU, 2nCH, LRU, WS or REFRESH.\n");
        exit(EXIT_FAILURE);
    }

    // Verificação da flag de impressão
    int printFlag = atoi(argv[1]);
    if (printFlag != 0 && printFlag != 1)
    {
        fprintf(stderr, "Invalid print flag. Please choose between 0 or 1.\n");
        exit(EXIT_FAILURE);
    }

    // Paths para os arquivos de logs de acesso
    char *processesPaths[NUM_PROCESS] = {"P1_AccessesLog.txt",
                                         "P2_AccessesLog.txt",
                                         "P3_AccessesLog.txt",
                                         "P4_AccessesLog.txt"};

    // Geração dos logs de acesso
    if (algorithm == -1)
    {
        printf("Gerando novos logs de acesso...\n");
        usleep(500000);
        // Gerando os logs de acesso dos processos
        if (accessLogsGen(processesPaths, -1) == 1)
        {
            perror("Error when generating access logs");
            exit(EXIT_FAILURE);
        }
        printf("Logs de acesso gerados com sucesso!\n");
        exit(EXIT_SUCCESS);
    }

    // Verifica se o algoritmo WS recebeu o parâmetro k
    int set = 0;
    if (algorithm == 3)
    {
        if (argc > 3)
        {
            set = atoi(argv[3]);
        }
        else
        {
            fprintf(stderr, "Missing parameter k for Working Set algorithm. Please provide a valid integer.\n");
            exit(EXIT_FAILURE);
        }
    }

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

    pid_t pid = fork();
    if (pid == 0)
    {
        // Processo filho
        sem_close(sem); // Fecha o semáforo no processo filho (será reaberto em gmv)
        gmv(algorithm, set, printFlag);
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
                // Limpa recursos
                sem_close(sem);
                sem_unlink(SEM_NAME);
                munmap(shared_data, SHM_SIZE);
                shm_unlink(SHM_NAME);
                exit(EXIT_FAILURE);
            }
        }
        printf("Arquivos abertos com sucesso!\n\n");
        usleep(500000);

        printf("Algoritmo Escolhido: %s\n", argv[2]);
        if (set != 0)
        {
            printf("Parâmetro do Working Set: %d\n", set);
        }
        usleep(500000);

        printf("Número de rodadas: %d\n\n", NUM_ROUNDS);
        usleep(500000);

        int totalAccesses = 0;
        char accessType;
        int pageNum;
        int aux;
        int round = 0;

        while (round < NUM_ROUNDS)
        {
            for (int i = 0; i < NUM_PROCESS; i++)
            {
                aux = fscanf(files[i], "%d %c", &pageNum, &accessType);
                if (aux != 2)
                {
                    // Recomeça a leitura do arquivo
                    if (accessLogsGen(processesPaths, i) == 1)
                    {
                        perror("Error when generating access logs");
                        exit(EXIT_FAILURE);
                    }
                    printf("Novos logs de acesso gerados com sucesso!\n");

                    fseek(files[i], 0, SEEK_SET);
                    aux = fscanf(files[i], "%d %c", &pageNum, &accessType);
                    if (aux != 2)
                    {
                        fprintf(stderr, "Erro ao ler os arquivos de log.\n");
                        break;
                    }
                }

                sem_wait(sem);
                if (shared_data->flag == 0)
                {
                    shared_data->pageNum = pageNum;
                    shared_data->accessType = accessType;
                    shared_data->flag = 1; // Dados disponíveis para o filho
                }
                sem_post(sem);
                totalAccesses++;
                usleep(10000); // Pequena espera para evitar loop rápido demais
            }
            round++;
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
        printf("Processo pai terminou.\n");
    }
    else
    {
        perror("fork falhou");
        exit(EXIT_FAILURE);
    }

    return 0;
}

/************************************************************************************************/
/************************************************************************************************/
/************************************************************************************************/
/************************************************************************************************/
/************************************************************************************************/
/************************************************************************************************/
/* LRU - FUNCTIONS */
// Initializes the queue
void inicializarFilaLRU(LRU_Fila *fila)
{
    fila->size = 0;
    for (int i = 0; i < MEMORY_SIZE; i++)
    {
        fila->memoryLRU[i] = -1;
        fila->reference_bitsLRU[i] = 0;
        fila->modified_bitsLRU[i] = 0;
    }
}

// Checks if a value is in the queue
int contemLRU(LRU_Fila *fila, int valor)
{
    for (int i = 0; i < fila->size; i++)
    {
        if (fila->memoryLRU[i] == valor)
        {
            return 1;
        }
    }
    return 0;
}

// Returns the index of the value in the queue, or -1 if not found
int indexOfLRU(LRU_Fila *fila, int valor)
{
    for (int i = 0; i < fila->size; i++)
    {
        if (fila->memoryLRU[i] == valor)
        {
            return i;
        }
    }
    return -1;
}

// Removes a specific value from the queue
void removerValorLRU(LRU_Fila *fila, int valor)
{
    int i, j;
    for (i = 0; i < fila->size; i++)
    {
        if (fila->memoryLRU[i] == valor)
        {
            // Remove the value by shifting elements
            for (j = i; j < fila->size - 1; j++)
            {
                fila->memoryLRU[j] = fila->memoryLRU[j + 1];
                fila->modified_bitsLRU[j] = fila->modified_bitsLRU[j + 1];
                fila->reference_bitsLRU[j] = fila->reference_bitsLRU[j + 1];
            }
            fila->size--;
            return;
        }
    }
}

// Adds a value to the queue
void adicionarLRU(LRU_Fila *fila, int valor, int *pageFault, int pageModified, int pageReferenced)
{
    if (contemLRU(fila, valor))
    {
        // Remove the value if it already exists
        removerValorLRU(fila, valor);
    }
    else
    {
        // Page fault occurs
        (*pageFault)++;
        // If the queue is full, remove the least recently used page
        if (fila->size == MEMORY_SIZE)
        {
            printf("Page fault: %d\n", valor + 1);
            printf("Page to remove: %d\n", fila->memoryLRU[0] + 1);
            if (fila->modified_bitsLRU[0] == 1)
            {
                printf("Dirty page replaced and written back to swap area.\n");
            }
            else
            {
                printf("Clean page replaced.\n");
            }
            // Shift all elements to the left to remove the oldest page
            for (int i = 0; i < MEMORY_SIZE - 1; i++)
            {
                fila->memoryLRU[i] = fila->memoryLRU[i + 1];
                fila->modified_bitsLRU[i] = fila->modified_bitsLRU[i + 1];
                fila->reference_bitsLRU[i] = fila->reference_bitsLRU[i + 1];
            }
            fila->size--;
        }
    }

    // Add the value to the end of the queue
    fila->memoryLRU[fila->size] = valor;
    fila->modified_bitsLRU[fila->size] = pageModified;
    fila->reference_bitsLRU[fila->size] = pageReferenced;
    fila->size++;
}

void imprimiTabelaProcessosLRU(LRU_Fila *fila)
{
    printf("Tabela de Processos: \n");
    printf("----------------------------\n");
    printf("| Page | Frame | Ref | Mod |\n");
    for (int i = 0; i < NUM_PAGES; i++)
    {
        int index = indexOfLRU(fila, i);
        if (index != -1)
        {
            printf("|  %2d  |   %2d  |  %d  |  %d  |\n", i + 1, index + 1, fila->reference_bitsLRU[index], fila->modified_bitsLRU[index]);
        }
        else
        {
            printf("|  %2d  | ----- | --- | --- |\n", i + 1);
        }
    }
    printf("----------------------------\n");
}

/************************************************************************************************/
/************************************************************************************************/
/************************************************************************************************/
/************************************************************************************************/
/************************************************************************************************/
/************************************************************************************************/
/* NRU - FUNCTION */
int NRU_whichPageToRemove(int *memory, int *pageModified, int *pageReferenced, int pageNum)
{
    /*
    Priorities of page classes in the NRU algorithm:
                                                 | M | R |
        Case 0: not modified, not referenced     | 0 | 0 |
        Case 1: not modified, referenced         | 0 | 1 |
        Case 2: modified, not referenced         | 1 | 0 |
        Case 3: modified, referenced             | 1 | 1 |
    */

    // Look for a free space in memory
    for (int i = 0; i < MEMORY_SIZE; i++)
    {
        if (memory[i] == -1)
        {
            return i;
        }
    }

    // Identify the page to be removed based on priority cases
    for (int caso = 0; caso < 4; caso++)
    {
        int candidates[MEMORY_SIZE];
        int count = 0;
        for (int i = 0; i < MEMORY_SIZE; i++)
        {
            int whichCase = (pageModified[i] << 1) | pageReferenced[i];
            if (whichCase == caso)
            {
                candidates[count++] = i;
            }
        }
        if (count > 0)
        {
            // Select a random index among candidates
            int randIndex = rand() % count;
            return candidates[randIndex];
        }
    }

    return -1;
}
// Returns the index of the value in the queue, or -1 if not found
int indexOfNRU(int *memory, int valor)
{
    for (int i = 0; i < MEMORY_SIZE; i++)
    {
        if (memory[i] == valor)
        {
            return i;
        }
    }
    return -1;
}
void imprimiTabelaProcessosNRU(int *memory, int *pageReferenced, int *pageModified)
{
    printf("Tabela de Processos: \n");
    printf("----------------------------\n");
    printf("| Page | Frame | Ref | Mod |\n");
    for (int i = 0; i < NUM_PAGES; i++)
    {
        int index = indexOfNRU(memory, i);
        if (index != -1)
        {
            printf("|  %2d  |   %2d  |  %d  |  %d  |\n", i + 1, index + 1, pageReferenced[index], pageModified[index]);
        }
        else
        {
            printf("|  %2d  | ----- | --- | --- |\n", i + 1);
        }
    }
    printf("----------------------------\n");
}

/************************************************************************************************/
/************************************************************************************************/
/************************************************************************************************/
/************************************************************************************************/
/************************************************************************************************/
/************************************************************************************************/
/* GMV - FUNCTION */
void gmv(int algorithm, int set, int printFlag)
{
    int pageFaults = 0;
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

    // Memory Block Declaration LRU
    LRU_Fila lru_Fila;
    inicializarFilaLRU(&lru_Fila);

    // Memory Block Declaration NRU
    int memoryNRU[MEMORY_SIZE];
    int reference_bitsNRU[MEMORY_SIZE];
    int modified_bitsNRU[MEMORY_SIZE];
    for (int i = 0; i < MEMORY_SIZE; i++)
    {
        memoryNRU[i] = -1;
        reference_bitsNRU[i] = 0;
        modified_bitsNRU[i] = 0;
    }

    // Memory Block Declaration 2nCh
    int memory2NCH[MEMORY_SIZE];
    int reference_bits2NCH[MEMORY_SIZE];
    int modified_bits2NCH[MEMORY_SIZE];
    int circular_queue_pointer = 0;
    for (int i = 0; i < MEMORY_SIZE; i++)
    {
        memory2NCH[i] = -1;
        reference_bits2NCH[i] = 0;
        modified_bits2NCH[i] = 0;
    }

    // Memory Block Declaration WS
    int k = set;
    int memoryWS[NUM_PROCESS][MEMORY_SIZE];
    int modified_bitsWS[NUM_PROCESS][NUM_PAGES];
    int reference_bitsWS[NUM_PROCESS][NUM_PAGES];
    int working_set_queue[NUM_PROCESS][MEMORY_SIZE];
    int working_set_size[NUM_PROCESS] = {0};
    for (int p = 0; p < NUM_PROCESS; p++)
    {
        for (int i = 0; i < MEMORY_SIZE; i++)
        {
            memoryWS[p][i] = -1;
            working_set_queue[p][i] = -1;
        }
        for (int i = 0; i < NUM_PAGES; i++)
        {
            modified_bitsWS[p][i] = 0;
            reference_bitsWS[p][i] = 0;
        }
    }

    printf("GMV iniciado\n");

    while (1)
    {
        sem_wait(sem);
        if (shared_data->flag == 1)
        {
            // Dados disponíveis para ler
            int pageNum = shared_data->pageNum;
            char accessType = shared_data->accessType;
            shared_data->flag = 0; // Buffer está vazio novamente
            sem_post(sem);

            // Processa os dados
            if (printFlag)
            {
                printf("Acesso: Página %d, Tipo %c\n", pageNum + 1, accessType);
            }

            // Chama o algoritmo de substituição de página apropriado
            switch (algorithm)
            {
            case 0:
                subs_NRU(memoryNRU, reference_bitsNRU, modified_bitsNRU, &pageFaults, pageNum, accessType);
                if (printFlag)
                {
                    imprimiTabelaProcessosNRU(memoryNRU, reference_bitsNRU, modified_bitsNRU);
                }
                break;
            case 1:
                subs_2nCh(pageNum, accessType);
                break;
            case 2:
                subs_LRU(&lru_Fila, &pageFaults, pageNum, accessType);
                if (printFlag)
                {
                    imprimiTabelaProcessosLRU(&lru_Fila);
                }
                break;
            case 3:
                subs_WS(set, pageNum, accessType);
                break;
            default:
                fprintf(stderr, "Código de algoritmo inválido\n");
                break;
            }
        }
        else if (shared_data->flag == 2)
        {
            sem_post(sem);
            break; // Sinal para terminar
        }
        else
        {
            sem_post(sem);
        }
        usleep(10000); // Pequena espera para evitar loop rápido demais
    }

    // Limpa recursos
    sem_close(sem);
    munmap(shared_data, SHM_SIZE);
    printf("GMV terminou\n");
    printf("Número de page faults: %d\n", pageFaults);
}

/* Função de geração dos logs de acesso */
int accessLogsGen(char **paths, int process)
{
    FILE *file;
    srand(time(NULL) ^ getpid() ^ process);

    if (process == -1)
    {
        for (int i = 0; i < NUM_PROCESS; i++)
        {
            file = fopen(paths[i], "w");
            if (file == NULL)
            {
                perror("Erro ao criar os arquivos de log");
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
    }
    else
    {
        file = fopen(paths[process], "w");
        if (file == NULL)
        {
            perror("Erro ao criar os arquivos de log");
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
/************************************************************************************************/
/************************************************************************************************/
/************************************************************************************************/
/************************************************************************************************/
/************************************************************************************************/
/* Implementação das funções dos algoritmos */
/* Estas funções precisam ser implementadas de acordo com a lógica de cada algoritmo */

void subs_NRU(int *memory, int *pageReferenced, int *pageModified, int *pageFault, int pageNum, char accessType)
{
    // Implementação do algoritmo NRU
    // Aqui você deve implementar a lógica do algoritmo NRU
    // Check if the page is already in memory
    int found = 0;
    for (int i = 0; i < MEMORY_SIZE; i++)
    {
        if (memory[i] == pageNum)
        {
            // Page hit
            if (accessType == 'R')
            {
                pageReferenced[i] = 1;
            }
            if (accessType == 'W')
            {
                pageModified[i] = 1;
            }
            found = 1;
            break;
        }
    }

    if (!found)
    {
        // Page fault
        (*pageFault)++;
        printf("Page fault: %d\n", pageNum + 1);

        // Select the page to be removed
        int pageToRemove = NRU_whichPageToRemove(memory, pageModified, pageReferenced, pageNum);

        printf("Page to remove: %d\n", memory[pageToRemove] + 1);
        if (memory[pageToRemove] != -1 && pageModified[pageToRemove] == 1)
        {
            printf("Dirty page replaced and written back to swap area.\n");
        }
        else
        {
            printf("Clean page replaced.\n");
        }

        // Replace the page in memory and update bits
        memory[pageToRemove] = pageNum;
        pageReferenced[pageToRemove] = (accessType == 'R') ? 1 : 0;
        pageModified[pageToRemove] = (accessType == 'W') ? 1 : 0;
    }
}

void subs_2nCh(int pageNum, char accessType)
{
    // Implementação do algoritmo Second Chance
    // Aqui você deve implementar a lógica do algoritmo Second Chance
}

void subs_LRU(LRU_Fila *lru_Fila, int *pageFault, int pageNum, char accessType)
{
    // Implementação do algoritmo LRU (Aging)
    // Aqui você deve implementar a lógica do algoritmo LRU

    int pageReferenced = (accessType == 'R') ? 1 : 0;
    int pageModified = (accessType == 'W') ? 1 : 0;
    adicionarLRU(lru_Fila, pageNum, pageFault, pageModified, pageReferenced);
}

void subs_WS(int set, int pageNum, char accessType)
{
    // Implementação do algoritmo Working Set
    // Aqui você deve implementar a lógica do algoritmo Working Set
}
