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
#define NUM_ROUNDS 5
#define SHM_NAME "/my_shared_memory"
#define SEM_NAME "/my_semaphore"
#define SHM_SIZE sizeof(SharedData)

#define RANDOM_PAGE() (rand() % (NUM_PAGES))
#define RANDOM_ACCESS() (rand() % 2)

#define MAX_WORKING_SET_SIZE NUM_PAGES

/* Estrutura para os dados compartilhados */
typedef struct
{
    int flag;
    int pageNum;
    char accessType;
    int processID;
} SharedData;

typedef struct
{
    int memoryLRU[MEMORY_SIZE];
    int size;
    int reference_bitsLRU[MEMORY_SIZE];
    int modified_bitsLRU[MEMORY_SIZE];
    int process_bit[MEMORY_SIZE];
} LRU_Fila;

typedef struct // Frame do working set
{
    int pageNum;
    int processId;
    int referenceBit;
    int modifiedBit;
} Frame;

/* Funções dos algoritmos de substituição de página */
void subs_NRU(int *memory, int *pageReferenced, int *pageModified, int *pageFault, int *process_Bits, int pageNum, char accessType, int processNum); // Not Recently Used (NRU)
void subs_2nCh(int *pageFault, int pageNum, char accessType, int processNum);                                                                        // Second Chance
void subs_LRU(LRU_Fila *lru_Fila, int *pageFault, int pageNum, char accessType, int processNum);                                                     // Aging (LRU)
void subs_WS(int set, int *pageFault, int processID, int pageNum, char accessType);                                                                  // Working Set (k)

/* Gerenciador de Memória Virtual */
void gmv(int algorithm, int set, int printFlag);

/* Gerador de Logs de Acesso */
int accessLogsGen(char **paths, int process);

/* Variáveis globais para o algoritmo de Segunda Chance */
int memory2NCH[MEMORY_SIZE];
int reference_bits2NCH[MEMORY_SIZE];
int modified_bits2NCH[MEMORY_SIZE];
int process_bits2NCH[MEMORY_SIZE];
int circular_queue_pointer;

// Variáveis globais para o algoritmo de Working Set
Frame physicalMemory[MEMORY_SIZE];
int workingSet[NUM_PROCESS][MAX_WORKING_SET_SIZE];
int workingSetSize[NUM_PROCESS];
int framesPerProcess[NUM_PROCESS];

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
                    shared_data->processID = i;
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
        fila->process_bit[i] = -1;
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
                fila->process_bit[j] = fila->process_bit[j + 1];
            }
            fila->size--;
            return;
        }
    }
}

// Adds a value to the queue
void adicionarLRU(LRU_Fila *fila, int valor, int *pageFault, int pageModified, int pageReferenced, int processNum)
{
    if (contemLRU(fila, valor))
    {
        if (fila->process_bit[indexOfLRU(&fila, valor)] != processNum)
        {
            printf("Page fault: %d\n", valor + 1);
            (*pageFault)++;
        }
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
                fila->process_bit[i] = fila->process_bit[i + 1];
            }
            fila->size--;
        }
    }

    // Add the value to the end of the queue
    fila->memoryLRU[fila->size] = valor;
    fila->modified_bitsLRU[fila->size] = pageModified;
    fila->reference_bitsLRU[fila->size] = pageReferenced;
    fila->process_bit[fila->size] = processNum;
    fila->size++;
}

void imprimiTabelaProcessosLRU(LRU_Fila *fila)
{
    printf("Tabela de Processos: \n");
    printf("----------------------------\n");
    printf("| Page | Frame | Ref | Mod | Pro |\n");
    for (int i = 0; i < NUM_PAGES; i++)
    {
        int index = indexOfLRU(fila, i);
        if (index != -1)
        {
            printf("|  %2d  |   %2d  |  %d  |  %d  |  %d  |\n", i + 1, index + 1, fila->reference_bitsLRU[index], fila->modified_bitsLRU[index], fila->process_bit[index] + 1);
        }
        else
        {
            printf("|  %2d  | ----- | --- | --- | --- |\n", i + 1);
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
void imprimiTabelaProcessosNRU(int *memory, int *pageReferenced, int *pageModified, int *processReference)
{
    printf("Tabela de Processos: \n");
    printf("----------------------------\n");
    printf("| Page | Frame | Ref | Mod | Pro |\n");
    for (int i = 0; i < NUM_PAGES; i++)
    {
        int index = indexOfNRU(memory, i);
        if (index != -1)
        {
            printf("|  %2d  |   %2d  |  %d  |  %d  |  %d  |\n", i + 1, index + 1, pageReferenced[index], pageModified[index], processReference[index] + 1);
        }
        else
        {
            printf("|  %2d  | ----- | --- | --- | --- |\n", i + 1);
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

void subs_NRU(int *memory, int *pageReferenced, int *pageModified, int *pageFault, int *process_Bits, int pageNum, char accessType, int processNum)
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

            if (process_Bits[i] != processNum)
            {
                printf("Page fault: %d\n", pageNum + 1);
                (*pageFault)++;
                process_Bits[i] = processNum;
                pageReferenced[i] = (accessType == 'R') ? 1 : 0;
                pageModified[i] = (accessType == 'W') ? 1 : 0;
            }
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
        process_Bits[pageToRemove] = processNum;
    }
}

/************************************************************************************************/
/************************************************************************************************/
/************************************************************************************************/
/************************************************************************************************/
/************************************************************************************************/
/************************************************************************************************/

void subs_LRU(LRU_Fila *lru_Fila, int *pageFault, int pageNum, char accessType, int processNum)
{
    // Implementação do algoritmo LRU (Aging)
    // Aqui você deve implementar a lógica do algoritmo LRU

    int pageReferenced = (accessType == 'R') ? 1 : 0;
    int pageModified = (accessType == 'W') ? 1 : 0;
    adicionarLRU(lru_Fila, pageNum, pageFault, pageModified, pageReferenced, processNum);
}

/************************************************************************************************/
/************************************************************************************************/
/************************************************************************************************/
/************************************************************************************************/
/************************************************************************************************/
/************************************************************************************************/

void imprimiTabelaProcessos2nCh()
{
    printf("Tabela de Processos (Second Chance): \n");
    printf("-------------------------------------\n");
    printf("| Frame | Page | Ref | Mod | Pro |\n");
    for (int i = 0; i < MEMORY_SIZE; i++)
    {
        if (memory2NCH[i] != -1)
        {
            printf("|   %2d  |  %2d  |  %d  |  %d  |  %d  |\n", i + 1, memory2NCH[i] + 1, reference_bits2NCH[i], modified_bits2NCH[i], process_bits2NCH[i] + 1);
        }
        else
        {
            printf("|   %2d  | ----- | --- | --- | --- |\n", i + 1);
        }
    }
    printf("-------------------------------------\n");
}

void subs_2nCh(int *pageFault, int pageNum, char accessType, int processNum)
{
    // Implementação do algoritmo Second Chance
    // First, check if the pageNum is already in memory
    int i, found = 0;
    for (i = 0; i < MEMORY_SIZE; i++)
    {
        if (memory2NCH[i] == pageNum)
        {
            // Page hit
            // Update reference bit
            reference_bits2NCH[i] = 1;
            // Update modified bit if necessary
            if (accessType == 'W')
            {
                modified_bits2NCH[i] = 1;
            }

            if (process_bits2NCH[i] != processNum)
            {
                printf("Page fault: %d\n", pageNum + 1);
                (*pageFault)++;
                process_bits2NCH[i] = processNum;
            }
            found = 1;
            break;
        }
    }

    if (!found)
    {
        // Page fault occurs
        (*pageFault)++;
        printf("Page fault: %d\n", pageNum + 1);

        // Try to find a free frame first
        int frame_index = -1;
        for (i = 0; i < MEMORY_SIZE; i++)
        {
            if (memory2NCH[i] == -1)
            {
                frame_index = i;
                break;
            }
        }

        if (frame_index != -1)
        {
            // Free frame found
            memory2NCH[frame_index] = pageNum;
            reference_bits2NCH[frame_index] = 1;
            if (accessType == 'W')
            {
                modified_bits2NCH[frame_index] = 1;
            }
            else
            {
                modified_bits2NCH[frame_index] = 0;
            }
        }
        else
        {
            // No free frame, need to find a victim using Second Chance algorithm
            while (1)
            {
                // If reference bit is 0, replace this page
                if (reference_bits2NCH[circular_queue_pointer] == 0)
                {
                    // Replace the page
                    printf("Page to remove: %d\n", memory2NCH[circular_queue_pointer] + 1);
                    if (modified_bits2NCH[circular_queue_pointer] == 1)
                    {
                        printf("Dirty page replaced and written back to swap area.\n");
                    }
                    else
                    {
                        printf("Clean page replaced.\n");
                    }

                    // Replace the page
                    memory2NCH[circular_queue_pointer] = pageNum;
                    reference_bits2NCH[circular_queue_pointer] = 1;
                    if (accessType == 'W')
                    {
                        modified_bits2NCH[circular_queue_pointer] = 1;
                    }
                    else
                    {
                        modified_bits2NCH[circular_queue_pointer] = 0;
                    }

                    // Advance the pointer
                    circular_queue_pointer = (circular_queue_pointer + 1) % MEMORY_SIZE;
                    break;
                }
                else
                {
                    // Give a second chance
                    reference_bits2NCH[circular_queue_pointer] = 0;
                    // Advance the pointer
                    circular_queue_pointer = (circular_queue_pointer + 1) % MEMORY_SIZE;
                }
            }
        }
    }
}

/************************************************************************************************/
/************************************************************************************************/
/************************************************************************************************/
/************************************************************************************************/
/************************************************************************************************/
/************************************************************************************************/

void imprimiTabelaProcessosWS()
{
    printf("Tabela de Processos (Working Set): \n");
    printf("-----------------------------------\n");
    printf("| Frame | Page | Proc | Ref | Mod |\n");
    for (int i = 0; i < MEMORY_SIZE; i++)
    {
        if (physicalMemory[i].pageNum != -1)
        {
            printf("|   %2d  |  %2d  |  %2d  |  %d  |  %d  |\n", i + 1, physicalMemory[i].pageNum + 1, physicalMemory[i].processId + 1, physicalMemory[i].referenceBit, physicalMemory[i].modifiedBit);
        }
        else
        {
            printf("|   %2d  | ----- | ---- | --- | --- |\n", i + 1);
        }
    }
    printf("-----------------------------------\n");
}

void subs_WS(int set, int *pageFault, int processId, int pageNum, char accessType)
{
    // Implementação do algoritmo Working Set
    int i;
    int foundInWorkingSet = 0;
    for (i = 0; i < workingSetSize[processId]; i++)
    {
        if (workingSet[processId][i] == pageNum)
        {
            foundInWorkingSet = 1;
            break;
        }
    }

    if (foundInWorkingSet)
    {
        // Page is in working set, check if it's in physical memory
        int foundInMemory = 0;
        for (i = 0; i < MEMORY_SIZE; i++)
        {
            if (physicalMemory[i].pageNum == pageNum && physicalMemory[i].processId == processId)
            {
                // Page hit
                physicalMemory[i].referenceBit = 1;
                if (accessType == 'W')
                {
                    physicalMemory[i].modifiedBit = 1;
                }
                foundInMemory = 1;
                break;
            }
        }
        if (!foundInMemory)
        {
            // Load the page into physical memory
            // Find a free frame or replace a page from the same process
            int frameIndex = -1;
            for (i = 0; i < MEMORY_SIZE; i++)
            {
                if (physicalMemory[i].pageNum == -1)
                {
                    frameIndex = i;
                    break;
                }
            }

            if (frameIndex != -1)
            {
                // Free frame found
                physicalMemory[frameIndex].pageNum = pageNum;
                physicalMemory[frameIndex].processId = processId;
                physicalMemory[frameIndex].referenceBit = 1;
                physicalMemory[frameIndex].modifiedBit = (accessType == 'W') ? 1 : 0;
                framesPerProcess[processId]++;
            }
            else
            {
                // Replace one of our own pages
                for (i = 0; i < MEMORY_SIZE; i++)
                {
                    if (physicalMemory[i].processId == processId)
                    {
                        printf("Page to remove: %d (Process %d)\n", physicalMemory[i].pageNum + 1, processId + 1);
                        if (physicalMemory[i].modifiedBit == 1)
                        {
                            printf("Dirty page replaced and written back to swap area.\n");
                        }
                        else
                        {
                            printf("Clean page replaced.\n");
                        }

                        physicalMemory[i].pageNum = pageNum;
                        physicalMemory[i].referenceBit = 1;
                        physicalMemory[i].modifiedBit = (accessType == 'W') ? 1 : 0;
                        break;
                    }
                }
            }
        }
    }
    else
    {
        // Page not in working set, page fault occurs
        (*pageFault)++;
        printf("Page fault: %d (Process %d)\n", pageNum + 1, processId + 1);

        // Add the page to the working set
        // If working set size exceeds 'k', remove the oldest page
        if (workingSetSize[processId] >= set)
        {
            // Remove the oldest page from the working set
            int pageToRemove = workingSet[processId][0];
            // Shift the working set queue
            for (i = 0; i < workingSetSize[processId] - 1; i++)
            {
                workingSet[processId][i] = workingSet[processId][i + 1];
            }
            workingSetSize[processId]--;

            // Remove the page from physical memory
            for (i = 0; i < MEMORY_SIZE; i++)
            {
                if (physicalMemory[i].pageNum == pageToRemove && physicalMemory[i].processId == processId)
                {
                    printf("Page to remove: %d (Process %d)\n", physicalMemory[i].pageNum + 1, processId + 1);
                    if (physicalMemory[i].modifiedBit == 1)
                    {
                        printf("Dirty page replaced and written back to swap area.\n");
                    }
                    else
                    {
                        printf("Clean page replaced.\n");
                    }
                    physicalMemory[i].pageNum = -1;
                    physicalMemory[i].processId = -1;
                    physicalMemory[i].referenceBit = 0;
                    physicalMemory[i].modifiedBit = 0;
                    framesPerProcess[processId]--;
                    break;
                }
            }
        }

        // Add the new page to the working set
        workingSet[processId][workingSetSize[processId]] = pageNum;
        workingSetSize[processId]++;

        // Now, load the page into physical memory
        // Find a free frame
        int frameIndex = -1;
        for (i = 0; i < MEMORY_SIZE; i++)
        {
            if (physicalMemory[i].pageNum == -1)
            {
                frameIndex = i;
                break;
            }
        }

        if (frameIndex != -1)
        {
            // Free frame found
            physicalMemory[frameIndex].pageNum = pageNum;
            physicalMemory[frameIndex].processId = processId;
            physicalMemory[frameIndex].referenceBit = 1;
            physicalMemory[frameIndex].modifiedBit = (accessType == 'W') ? 1 : 0;
            framesPerProcess[processId]++;
        }
        else
        {
            // No free frames, need to replace a page
            if (framesPerProcess[processId] > 0)
            {
                // Replace one of our own pages
                for (i = 0; i < MEMORY_SIZE; i++)
                {
                    if (physicalMemory[i].processId == processId)
                    {
                        printf("Page to remove: %d (Process %d)\n", physicalMemory[i].pageNum + 1, processId + 1);
                        if (physicalMemory[i].modifiedBit == 1)
                        {
                            printf("Dirty page replaced and written back to swap area.\n");
                        }
                        else
                        {
                            printf("Clean page replaced.\n");
                        }
                        physicalMemory[i].pageNum = pageNum;
                        physicalMemory[i].referenceBit = 1;
                        physicalMemory[i].modifiedBit = (accessType == 'W') ? 1 : 0;
                        break;
                    }
                }
            }
            else
            {
                // Steal a frame from another process
                printf("Process %d has no frames, stealing a frame from another process.\n", processId + 1);
                for (i = 0; i < MEMORY_SIZE; i++)
                {
                    if (physicalMemory[i].processId != processId && physicalMemory[i].processId != -1)
                    {
                        int victimProcessId = physicalMemory[i].processId;
                        printf("Page to remove: %d (Process %d)\n", physicalMemory[i].pageNum + 1, victimProcessId + 1);
                        if (physicalMemory[i].modifiedBit == 1)
                        {
                            printf("Dirty page replaced and written back to swap area.\n");
                        }
                        else
                        {
                            printf("Clean page replaced.\n");
                        }

                        // Remove page from victim's working set
                        int victimPageNum = physicalMemory[i].pageNum;
                        for (int j = 0; j < workingSetSize[victimProcessId]; j++)
                        {
                            if (workingSet[victimProcessId][j] == victimPageNum)
                            {
                                for (int k = j; k < workingSetSize[victimProcessId] - 1; k++)
                                {
                                    workingSet[victimProcessId][k] = workingSet[victimProcessId][k + 1];
                                }
                                workingSetSize[victimProcessId]--;
                                break;
                            }
                        }

                        physicalMemory[i].pageNum = pageNum;
                        physicalMemory[i].processId = processId;
                        physicalMemory[i].referenceBit = 1;
                        physicalMemory[i].modifiedBit = (accessType == 'W') ? 1 : 0;
                        framesPerProcess[processId]++;
                        framesPerProcess[victimProcessId]--;
                        break;
                    }
                }
            }
        }
    }
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
    int process_bitsNRU[MEMORY_SIZE];
    for (int i = 0; i < MEMORY_SIZE; i++)
    {
        memoryNRU[i] = -1;
        reference_bitsNRU[i] = 0;
        modified_bitsNRU[i] = 0;
        process_bitsNRU[i] = -1;
    }

    // Memory Block Declaration 2nCh
    circular_queue_pointer = 0;
    for (int i = 0; i < MEMORY_SIZE; i++)
    {
        memory2NCH[i] = -1;
        reference_bits2NCH[i] = 0;
        modified_bits2NCH[i] = 0;
    }

    // Memory Block Declaration WS
    int k = set;
    for (int i = 0; i < MEMORY_SIZE; i++)
    {
        physicalMemory[i].pageNum = -1;
        physicalMemory[i].processId = -1;
        physicalMemory[i].referenceBit = 0;
        physicalMemory[i].modifiedBit = 0;
    }
    for (int p = 0; p < NUM_PROCESS; p++)
    {
        workingSetSize[p] = 0;
        framesPerProcess[p] = 0;
        for (int i = 0; i < MAX_WORKING_SET_SIZE; i++)
        {
            workingSet[p][i] = -1;
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
            int processID = shared_data->processID;
            char accessType = shared_data->accessType;
            shared_data->flag = 0; // Buffer está vazio novamente
            sem_post(sem);

            // Processa os dados
            if (printFlag)
            {
                printf("Processo %d: Página %d, Tipo %c\n", processID + 1, pageNum + 1, accessType);
            }

            // Chama o algoritmo de substituição de página apropriado
            switch (algorithm)
            {
            case 0:
                subs_NRU(memoryNRU, reference_bitsNRU, modified_bitsNRU, &pageFaults, process_bitsNRU, pageNum, accessType, processID);
                if (printFlag)
                {
                    imprimiTabelaProcessosNRU(memoryNRU, reference_bitsNRU, modified_bitsNRU, process_bitsNRU);
                }
                break;
            case 1:
                subs_2nCh(&pageFaults, pageNum, accessType, processID);
                if (printFlag)
                {
                    imprimiTabelaProcessos2nCh(); // Call the function to print the page table
                }
                break;
            case 2:
                subs_LRU(&lru_Fila, &pageFaults, pageNum, accessType, processID);
                if (printFlag)
                {
                    imprimiTabelaProcessosLRU(&lru_Fila);
                }
                break;
            case 3:
                subs_WS(set, &pageFaults, processID, pageNum, accessType);
                if (printFlag)
                {
                    imprimiTabelaProcessosWS(); // Call the function to print the page table
                }
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