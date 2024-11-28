/*  TRABALHO 2 - SISTEMAS OPERACIONAIS                  */
/*  Alunos: Luiz Eduardo Raffaini e Isabela Braconi     */
/*  Professor: Markus Endler                            */

/* Includes */
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

/* Defines */
#define NUM_PROCESS 4
#define NUM_PAGES 32
#define NUM_ACCESS 120
#define MEMORY_SIZE 16
#define NRU_RESET_INTERVAL 15
#define NUM_ROUNDS 1

/* Macros */
#define RANDOM_PAGE() (rand() % (NUM_PAGES))
#define RANDOM_ACCESS() (rand() % 2)

/* Access Log Generator Function */
int accessLogsGen(char **paths);

/* Page Algorithms Declarations */
int subs_NRU(char **paths, int print); // Not Recently Used (NRU)
int subs_2nCh(char **paths);           // Second Chance
int subs_LRU(char **paths, int print); // Aging (LRU)
int subs_WS(char **paths, int set);    // Working Set (k)

/* Main Function */
int main(int argv, char **argc)
{
    int pageFaultsLRU[NUM_ROUNDS];
    int pageFaultsNRU[NUM_ROUNDS];
    int pageFaults2nCH[NUM_ROUNDS];
    int pageFaultsWS[NUM_ROUNDS];

    // Paths for access logs files
    char *processesPaths[NUM_PROCESS] = {"AccessesLogs/P1_AccessesLog.txt",
                                         "AccessesLogs/P2_AccessesLog.txt",
                                         "AccessesLogs/P3_AccessesLog.txt",
                                         "AccessesLogs/P4_AccessesLog.txt"};

    // Running page replacement algorithms
    printf("Gerando novos logs de accesso...\n");
    if (strcmp(argc[1], "REFRESH") == 0)
    {
        // Generating process access logs
        if (accessLogsGen(processesPaths) == 1)
        {
            perror("Error when loading access logs");
            exit(1);
        }
    }
    else
    {

        printf("Algoritmo Escolhido: %s\n", argc[1]);
        if (argv > 2)
        {
            printf("Parâmetro do Working Set: %s\n", argc[2]);
        }
        printf("Numero de rodadas: %d\n\n", NUM_ROUNDS);

        for (int i = 0; i < NUM_ROUNDS; i++)
        {
            printf("Rodada %d\n\n", i + 1);

            if (strcmp(argc[1], "LRU") == 0)
            {
                pageFaultsLRU[i] = subs_LRU(processesPaths, 1);
                if (pageFaultsLRU[i] == -1)
                {
                    perror("Error when running LRU algorithm");
                    exit(1);
                }
            }
            else if (strcmp(argc[1], "NRU") == 0)
            {
                pageFaultsNRU[i] = subs_NRU(processesPaths, 1);
                if (pageFaultsNRU[i] == -1)
                {
                    perror("Error when running NRU algorithm");
                    exit(1);
                }
            }
            else if (strcmp(argc[1], "2nCH") == 0)
            {
                pageFaults2nCH[i] = subs_2nCh(processesPaths);
                if (pageFaults2nCH[i] == -1)
                {
                    perror("Error when running Second Chance algorithm");
                    exit(1);
                }
            }
            else if (strcmp(argc[1], "WS") == 0)
            {
                if (strcmp(argc[2], "") == 0)
                {
                    perror("Missing parameter k for Working Set algorithm");
                    exit(1);
                }
                pageFaultsWS[i] = subs_WS(processesPaths, atoi(argc[2]));
                if (pageFaultsWS[i] == -1)
                {
                    perror("Error when running Working Set algorithm");
                    exit(1);
                }
            }
            else
            {
                perror("Invalid algorithm");
                exit(1);
            }
        }
    }

    // Calculating and printing average page faults
    int total;
    if (strcmp(argc[1], "LRU") == 0)
    {
        total = 0;
        for (int i = 0; i < NUM_ROUNDS; i++)
        {
            total += pageFaultsLRU[i];
        }
        printf("Average Page Faults LRU: %d\n", total / NUM_ROUNDS);
    }
    else if (strcmp(argc[1], "NRU") == 0)
    {
        total = 0;
        for (int i = 0; i < NUM_ROUNDS; i++)
        {
            total += pageFaultsNRU[i];
        }
        printf("Average Page Faults NRU: %d\n", total / NUM_ROUNDS);
    }
    else if (strcmp(argc[1], "2nCH") == 0)
    {
        total = 0;
        for (int i = 0; i < NUM_ROUNDS; i++)
        {
            total += pageFaults2nCH[i];
        }
        printf("Average Page Faults Second Chance: %d\n", total / NUM_ROUNDS);
    }
    else if (strcmp(argc[1], "WS") == 0)
    {
        total = 0;
        for (int i = 0; i < NUM_ROUNDS; i++)
        {
            total += pageFaultsWS[i];
        }
        printf("Average Page Faults Working Set (k=%d): %d\n", atoi(argc[2]), total / NUM_ROUNDS);
    }
    return 0;
}

/************************************************************************************************/
/************************************************************************************************/
/************************************************************************************************/
/************************************************************************************************/
/************************************************************************************************/
/************************************************************************************************/
/*              LRU - ALGORITHM                 */
typedef struct
{
    int dados[MEMORY_SIZE];
    int tamanho;
    int referencia[MEMORY_SIZE];
    int modificado[MEMORY_SIZE];
} LRU_Fila;

// Initializes the queue
void inicializarFilaLRU(LRU_Fila *fila)
{
    fila->tamanho = 0;
    for (int i = 0; i < MEMORY_SIZE; i++)
    {
        fila->dados[i] = -1;
        fila->referencia[i] = 0;
        fila->modificado[i] = 0;
    }
}

// Checks if a value is in the queue
int contemLRU(LRU_Fila *fila, int valor)
{
    for (int i = 0; i < fila->tamanho; i++)
    {
        if (fila->dados[i] == valor)
        {
            return 1;
        }
    }
    return 0;
}

// Returns the index of the value in the queue, or -1 if not found
int indexOfLRU(LRU_Fila *fila, int valor)
{
    for (int i = 0; i < fila->tamanho; i++)
    {
        if (fila->dados[i] == valor)
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
    for (i = 0; i < fila->tamanho; i++)
    {
        if (fila->dados[i] == valor)
        {
            // Remove the value by shifting elements
            for (j = i; j < fila->tamanho - 1; j++)
            {
                fila->dados[j] = fila->dados[j + 1];
                fila->modificado[j] = fila->modificado[j + 1];
                fila->referencia[j] = fila->referencia[j + 1];
            }
            fila->tamanho--;
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
        if (fila->tamanho == MEMORY_SIZE)
        {
            printf("Page fault: %d\n", valor + 1);
            printf("Page to remove: %d\n", fila->dados[0] + 1);
            if (fila->modificado[0] == 1)
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
                fila->dados[i] = fila->dados[i + 1];
                fila->modificado[i] = fila->modificado[i + 1];
                fila->referencia[i] = fila->referencia[i + 1];
            }
            fila->tamanho--;
        }
    }

    // Add the value to the end of the queue
    fila->dados[fila->tamanho] = valor;
    fila->modificado[fila->tamanho] = pageModified;
    fila->referencia[fila->tamanho] = pageReferenced;
    fila->tamanho++;
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
            printf("|  %2d  |   %2d  |  %d  |  %d  |\n", i + 1, index + 1, fila->referencia[index], fila->modificado[index]);
        }
        else
        {
            printf("|  %2d  | ----- | --- | --- |\n", i + 1);
        }
    }
    printf("----------------------------\n");
}

int subs_LRU(char **paths, int print)
{
    // Memory Block Declaration
    LRU_Fila lru_Fila;
    inicializarFilaLRU(&lru_Fila);

    // Opening files
    FILE *files[NUM_PROCESS];
    for (int i = 0; i < NUM_PROCESS; i++)
    {
        files[i] = fopen(paths[i], "r");
        if (files[i] == NULL)
        {
            for (int j = 0; j < i; j++)
            {
                fclose(files[j]);
            }
            return -1;
        }
    }

    int totalAccesses = 0;
    int pageNum;
    char accessType;
    int pageFault = 0;
    while (fscanf(files[totalAccesses % NUM_PROCESS], "%d %c", &pageNum, &accessType) == 2)
    {
        printf("Accesso: %d %c\n", pageNum + 1, accessType);

        // Check if the page number is valid
        if (pageNum < 0)
        {
            perror("Invalid page number");
            for (int i = 0; i < NUM_PROCESS; i++)
            {
                fclose(files[i]);
            }
            return -1;
        }

        int pageReferenced = (accessType == 'R') ? 1 : 0;
        int pageModified = (accessType == 'W') ? 1 : 0;

        // Add the page to the queue
        adicionarLRU(&lru_Fila, pageNum, &pageFault, pageModified, pageReferenced);

        totalAccesses++;

        if (print)
        {
            imprimiTabelaProcessosLRU(&lru_Fila);
        }
    }

    // Closing files
    for (int i = 0; i < NUM_PROCESS; i++)
    {
        fclose(files[i]);
    }

    return pageFault;
}

/************************************************************************************************/
/************************************************************************************************/
/************************************************************************************************/
/************************************************************************************************/
/************************************************************************************************/
/************************************************************************************************/
/*              NRU - ALGORITHM                 */
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

int subs_NRU(char **paths, int print)
{
    // Memory Block Declaration
    int memory[MEMORY_SIZE];
    int pageModified[MEMORY_SIZE];
    int pageReferenced[MEMORY_SIZE];
    for (int i = 0; i < MEMORY_SIZE; i++)
    {
        memory[i] = -1;
        pageModified[i] = 0;
        pageReferenced[i] = 0;
    }

    // Opening files
    FILE *files[NUM_PROCESS];
    for (int i = 0; i < NUM_PROCESS; i++)
    {
        files[i] = fopen(paths[i], "r");
        if (files[i] == NULL)
        {
            for (int j = 0; j < i; j++)
            {
                fclose(files[j]);
            }
            perror("Error opening files");
            return -1;
        }
    }

    int accessesSinceReset = 0;
    int totalAccesses = 0;
    int pageFault = 0;
    int pageNum;
    char accessType;
    int pageToRemove;

    while (fscanf(files[totalAccesses % NUM_PROCESS], "%d %c", &pageNum, &accessType) == 2)
    {
        printf("Accesso: %d %c\n", pageNum + 1, accessType);

        // Check if the page number is valid
        if (pageNum < 0)
        {
            perror("Invalid page number");
            for (int i = 0; i < NUM_PROCESS; i++)
            {
                fclose(files[i]);
            }
            return -1;
        }

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
            pageFault++;
            printf("Page fault: %d\n", pageNum);

            // Select the page to be removed
            pageToRemove = NRU_whichPageToRemove(memory, pageModified, pageReferenced, pageNum);
            if (pageToRemove == -1)
            {
                perror("Error when selecting page to remove");
                for (int i = 0; i < NUM_PROCESS; i++)
                {
                    fclose(files[i]);
                }
                return -1;
            }

            printf("Page to remove: %d\n", memory[pageToRemove]);
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

        if (print)
        {
            imprimiTabelaProcessosNRU(memory, pageReferenced, pageModified);
        }

        totalAccesses++;
    }

    // Closing files
    for (int i = 0; i < NUM_PROCESS; i++)
    {
        fclose(files[i]);
    }

    return pageFault;
}

/************************************************************************************************/
/************************************************************************************************/
/************************************************************************************************/
/************************************************************************************************/
/************************************************************************************************/
/************************************************************************************************/
/*              Second Chance Algorithm         */
int subs_2nCh(char **paths)
{
    // Memory Block Declaration
    int memory[MEMORY_SIZE];
    int reference_bits_of_each_frame[MEMORY_SIZE];
    int modified_bits[MEMORY_SIZE]; // Array to track modified bits
    int circular_queue_pointer = 0;

    // Initialize memory, reference bits, and modified bits
    for (int i = 0; i < MEMORY_SIZE; i++)
    {
        memory[i] = -1;
        reference_bits_of_each_frame[i] = 0;
        modified_bits[i] = 0;
    }

    // Open access log files for each process
    FILE *files[NUM_PROCESS];
    for (int i = 0; i < NUM_PROCESS; i++)
    {
        files[i] = fopen(paths[i], "r");
        if (files[i] == NULL)
        {
            for (int j = 0; j < i; j++)
            {
                fclose(files[j]);
            }
            perror("Error opening files");
            return -1;
        }
    }

    int totalAccesses = 0;
    int pageFault = 0;
    int pageNum;
    char accessType;

    // Process memory accesses
    while (fscanf(files[totalAccesses % NUM_PROCESS], "%d %c", &pageNum, &accessType) == 2)
    {
        printf("Accesso: %d %c\n", pageNum + 1, accessType);

        // Check if page is valid
        if (pageNum < 0)
        {
            perror("Invalid page number");
            for (int i = 0; i < NUM_PROCESS; i++)
            {
                fclose(files[i]);
            }
            return -1;
        }

        int found = 0;

        // Check if page is already in memory
        for (int i = 0; i < MEMORY_SIZE; i++)
        {
            if (memory[i] == pageNum)
            {
                reference_bits_of_each_frame[i] = 1; // Set reference bit
                if (accessType == 'W')
                {
                    modified_bits[i] = 1; // Set modified bit
                }
                found = 1;
                break;
            }
        }

        if (!found)
        {
            // Page fault
            pageFault++;
            printf("Page fault: %d\n", pageNum + 1);

            // Check for a free frame
            int free_frame = -1;
            for (int i = 0; i < MEMORY_SIZE; i++)
            {
                if (memory[i] == -1)
                {
                    free_frame = i;
                    break;
                }
            }

            if (free_frame != -1)
            {
                memory[free_frame] = pageNum;
                reference_bits_of_each_frame[free_frame] = 1;
                modified_bits[free_frame] = (accessType == 'W') ? 1 : 0;
            }
            else
            {
                // Second Chance algorithm to find replacement
                while (1)
                {
                    if (reference_bits_of_each_frame[circular_queue_pointer] == 0)
                    {
                        printf("Page to remove: %d\n", memory[circular_queue_pointer] + 1);
                        if (modified_bits[circular_queue_pointer] == 1)
                        {
                            printf("Dirty page replaced and written back to swap area.\n");
                        }
                        else
                        {
                            printf("Clean page replaced.\n");
                        }

                        memory[circular_queue_pointer] = pageNum;
                        reference_bits_of_each_frame[circular_queue_pointer] = 1;
                        modified_bits[circular_queue_pointer] = (accessType == 'W') ? 1 : 0;
                        circular_queue_pointer = (circular_queue_pointer + 1) % MEMORY_SIZE;
                        break;
                    }
                    else
                    {
                        reference_bits_of_each_frame[circular_queue_pointer] = 0;
                        circular_queue_pointer = (circular_queue_pointer + 1) % MEMORY_SIZE;
                    }
                }
            }
        }

        // Print process table
        printf("Tabela de Processos: \n");
        printf("----------------------------\n");
        printf("| Page | Frame | Ref | Mod |\n");
        for (int i = 0; i < MEMORY_SIZE; i++)
        {
            printf("|  %2d  |   %2d  |  %d  |  %d  |\n",
                   i + 1, memory[i] == -1 ? -1 : memory[i] + 1,
                   reference_bits_of_each_frame[i],
                   modified_bits[i]);
        }
        printf("----------------------------\n");

        totalAccesses++;
    }

    // Close files
    for (int i = 0; i < NUM_PROCESS; i++)
    {
        fclose(files[i]);
    }

    return pageFault;
}

/************************************************************************************************/
/************************************************************************************************/
/************************************************************************************************/
/************************************************************************************************/
/************************************************************************************************/
/************************************************************************************************/
/*              Working Set Algorithm           */
int subs_WS(char **paths, int set)
{
    // Define working set window size (k)
    int k = set;

    // Memory Block Declaration
    int memory[MEMORY_SIZE];
    int modified_bits[MEMORY_SIZE];  // Tracks whether a page is modified
    int reference_bits[MEMORY_SIZE]; // Tracks whether a page is recently accessed
    int working_set_queue[MEMORY_SIZE];
    int working_set_size = 0;

    // Initialize memory, working set queue, modified, and reference bits
    for (int i = 0; i < MEMORY_SIZE; i++)
    {
        memory[i] = -1;
        modified_bits[i] = 0;
        reference_bits[i] = 0;
        working_set_queue[i] = -1;
    }

    // Open access log files for each process
    FILE *files[NUM_PROCESS];
    for (int i = 0; i < NUM_PROCESS; i++)
    {
        files[i] = fopen(paths[i], "r");
        if (files[i] == NULL)
        {
            for (int j = 0; j < i; j++)
            {
                fclose(files[j]);
            }
            perror("Error opening files");
            return -1;
        }
    }

    int totalAccesses = 0;
    int pageFault = 0;
    int pageNum;
    char accessType;

    // Process memory accesses
    while (fscanf(files[totalAccesses % NUM_PROCESS], "%d %c", &pageNum, &accessType) == 2)
    {
        printf("Accesso: %d %c\n", pageNum + 1, accessType);

        // Check if page is valid
        if (pageNum < 0)
        {
            perror("Invalid page number");
            for (int i = 0; i < NUM_PROCESS; i++)
            {
                fclose(files[i]);
            }
            return -1;
        }

        int found = 0;

        // Check if page is already in the working set
        for (int i = 0; i < working_set_size; i++)
        {
            if (working_set_queue[i] == pageNum)
            {
                found = 1;

                // Move page to end of working set queue
                for (int j = i; j < working_set_size - 1; j++)
                {
                    working_set_queue[j] = working_set_queue[j + 1];
                }
                working_set_queue[working_set_size - 1] = pageNum;

                // Update reference and modified bits
                reference_bits[i] = 1;
                if (accessType == 'W')
                {
                    modified_bits[i] = 1;
                }
                break;
            }
        }

        if (!found)
        {
            // Page fault
            pageFault++;
            printf("Page fault: %d\n", pageNum + 1);

            if (working_set_size < k && working_set_size < MEMORY_SIZE)
            {
                int free_frame = -1;
                for (int i = 0; i < MEMORY_SIZE; i++)
                {
                    if (memory[i] == -1)
                    {
                        free_frame = i;
                        break;
                    }
                }

                if (free_frame != -1)
                {
                    memory[free_frame] = pageNum;
                    modified_bits[free_frame] = (accessType == 'W') ? 1 : 0;
                    reference_bits[free_frame] = 1;
                    working_set_queue[working_set_size++] = pageNum;
                }
            }
            else
            {
                // Replace oldest page
                int page_to_remove = working_set_queue[0];

                for (int i = 0; i < MEMORY_SIZE; i++)
                {
                    if (memory[i] == page_to_remove)
                    {
                        printf("Page to remove: %d\n", memory[i] + 1);
                        if (modified_bits[i] == 1)
                        {
                            printf("Dirty page replaced and written back to swap area.\n");
                        }
                        else
                        {
                            printf("Clean page replaced.\n");
                        }
                        memory[i] = pageNum;
                        modified_bits[i] = (accessType == 'W') ? 1 : 0;
                        reference_bits[i] = 1;
                        break;
                    }
                }

                // Shift working set queue
                for (int i = 0; i < working_set_size - 1; i++)
                {
                    working_set_queue[i] = working_set_queue[i + 1];
                }
                working_set_queue[working_set_size - 1] = pageNum;
            }
        }

        // Print process table
        printf("Tabela de Processos: \n");
        printf("----------------------------\n");
        printf("| Page | Frame | Ref | Mod |\n");
        for (int i = 0; i < MEMORY_SIZE; i++)
        {
            printf("|  %2d  |   %2d  |  %d  |  %d  |\n",
                   i + 1,
                   memory[i] == -1 ? -1 : memory[i] + 1,
                   memory[i] == -1 ? 0 : reference_bits[i],
                   memory[i] == -1 ? 0 : modified_bits[i]);
        }
        printf("----------------------------\n");

        totalAccesses++; // Increment the total number of accesses processed
    }

    // Close all files after processing
    for (int i = 0; i < NUM_PROCESS; i++)
    {
        fclose(files[i]);
    }

    return pageFault; // Return the total number of page faults encountered
}

/************************************************************************************************/
/************************************************************************************************/
/************************************************************************************************/
/************************************************************************************************/
/************************************************************************************************/
/************************************************************************************************/
/*          Access Logs Generator               */
int accessLogsGen(char **paths)
{
    FILE *file;
    srand(time(NULL));

    for (int i = 0; i < NUM_PROCESS; i++)
    {
        file = fopen(paths[i], "w");
        if (file == NULL)
        {
            return 1;
        }
        else
        {
            for (int j = 0; j < NUM_ACCESS; j++)
            {
                if (RANDOM_ACCESS())
                { // Read
                    fprintf(file, "%d R\n", RANDOM_PAGE());
                }
                else
                { // Write
                    fprintf(file, "%d W\n", RANDOM_PAGE());
                }
            }
        }

        fclose(file);
    }

    return 0;
}