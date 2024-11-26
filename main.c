/*  TRABALHO 2 - SISTEMAS OPERACIONAIS                  */
/*  Alunos: Luiz Eduardo Raffaini e Isabela Braconi     */
/*  Professor: Markus Endler                            */

/* Includes */
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>

/* Defines */
#define NUM_PROCESS 4
#define NUM_PAGES 32
#define NUM_ACCESS 120
#define MEMORY_SIZE 16
#define NRU_RESET_INTERVAL 15
#define NUM_ROUNDS 10000
#define WS_WINDOW_SIZE 3  // Window size for Working Set algorithm

/* Macros */
#define RANDOM_PAGE() (rand() % (NUM_PAGES))
#define RANDOM_ACCESS() (rand() % 2)

/* Access Log Generator Function */
int accessLogsGen(char **paths);

/* Page Algorithms Declarations */
int subs_NRU(char **paths);  // Not Recently Used (NRU)
int subs_2nCh(char **paths); // Second Chance
int subs_LRU(char **paths);  // Aging (LRU)
int subs_WS(char **paths);   // Working Set (k)

/* Main Function */
int main(void)
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

    for (int i = 0; i < NUM_ROUNDS; i++)
    {
        // Generating process access logs
        if (accessLogsGen(processesPaths) == 1)
        {
            perror("Error when loading access logs");
            exit(1);
        }

        // Running page replacement algorithms
        pageFaultsLRU[i] = subs_LRU(processesPaths);
        if (pageFaultsLRU[i] == -1)
        {
            perror("Error when running LRU algorithm");
            exit(1);
        }

        pageFaultsNRU[i] = subs_NRU(processesPaths);
        if (pageFaultsNRU[i] == -1)
        {
            perror("Error when running NRU algorithm");
            exit(1);
        }

        pageFaults2nCH[i] = subs_2nCh(processesPaths);
        if (pageFaults2nCH[i] == -1)
        {
            perror("Error when running Second Chance algorithm");
            exit(1);
        }

        pageFaultsWS[i] = subs_WS(processesPaths);
        if (pageFaultsWS[i] == -1)
        {
            perror("Error when running Working Set algorithm");
            exit(1);
        }
    }

    // Calculating and printing average page faults
    int total;

    total = 0;
    for (int i = 0; i < NUM_ROUNDS; i++)
    {
        total += pageFaultsLRU[i];
    }
    printf("Average Page Faults LRU: %d\n", total / NUM_ROUNDS);

    total = 0;
    for (int i = 0; i < NUM_ROUNDS; i++)
    {
        total += pageFaultsNRU[i];
    }
    printf("Average Page Faults NRU: %d\n", total / NUM_ROUNDS);

    total = 0;
    for (int i = 0; i < NUM_ROUNDS; i++)
    {
        total += pageFaults2nCH[i];
    }
    printf("Average Page Faults Second Chance: %d\n", total / NUM_ROUNDS);

    total = 0;
    for (int i = 0; i < NUM_ROUNDS; i++)
    {
        total += pageFaultsWS[i];
    }
    printf("Average Page Faults Working Set (k=%d): %d\n", WS_WINDOW_SIZE, total / NUM_ROUNDS);

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
} LRU_Fila;

// Initializes the queue
void inicializarFila(LRU_Fila *fila)
{
    fila->tamanho = 0;
}

// Checks if a value is in the queue
int contem(LRU_Fila *fila, int valor)
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

// Removes a specific value from the queue
void removerValor(LRU_Fila *fila, int valor)
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
            }
            fila->tamanho--;
            return;
        }
    }
}

// Adds a value to the queue
void adicionar(LRU_Fila *fila, int valor, int *pageFault)
{
    if (contem(fila, valor))
    {
        // Remove the value if it already exists
        removerValor(fila, valor);
    }
    else
    {
        *pageFault += 1;
    }

    // Add the value to the end of the queue
    if (fila->tamanho < MEMORY_SIZE)
    {
        fila->dados[fila->tamanho++] = valor;
    }
    else
    {
        // Remove the first element to make space
        for (int i = 0; i < MEMORY_SIZE - 1; i++)
        {
            fila->dados[i] = fila->dados[i + 1];
        }
        fila->dados[MEMORY_SIZE - 1] = valor;
    }
}

int subs_LRU(char **paths)
{
    // Memory Block Declaration
    LRU_Fila lru_Fila;
    inicializarFila(&lru_Fila);

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

        // Add the page to the queue
        adicionar(&lru_Fila, pageNum, &pageFault);
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

int subs_NRU(char **paths)
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
                pageReferenced[i] = 1;
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

            // Replace the page in memory and update bits
            memory[pageToRemove] = pageNum;
            pageReferenced[pageToRemove] = 1;
            pageModified[pageToRemove] = (accessType == 'W') ? 1 : 0;
        }

        accessesSinceReset++;
        if (accessesSinceReset >= NRU_RESET_INTERVAL)
        {
            for (int i = 0; i < MEMORY_SIZE; i++)
            {
                pageReferenced[i] = 0;
            }
            accessesSinceReset = 0;
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
    int reference_bits[MEMORY_SIZE];
    int hand = 0; // Pointer for the circular queue

    // Initialize memory and reference bits
    for (int i = 0; i < MEMORY_SIZE; i++)
    {
        memory[i] = -1; // Indicates empty frame
        reference_bits[i] = 0;
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

    int totalAccesses = 0;
    int pageFault = 0;
    int pageNum;
    char accessType;

    while (fscanf(files[totalAccesses % NUM_PROCESS], "%d %c", &pageNum, &accessType) == 2)
    {
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

        int found = 0;
        // Check if the page is already in memory
        for (int i = 0; i < MEMORY_SIZE; i++)
        {
            if (memory[i] == pageNum)
            {
                // Page hit
                reference_bits[i] = 1; // Set reference bit
                found = 1;
                break;
            }
        }

        if (!found)
        {
            // Page fault
            pageFault++;

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
                // Use the free frame
                memory[free_frame] = pageNum;
                reference_bits[free_frame] = 1;
            }
            else
            {
                // Second Chance Algorithm
                while (1)
                {
                    if (reference_bits[hand] == 0)
                    {
                        // Replace the page at hand
                        memory[hand] = pageNum;
                        reference_bits[hand] = 1;
                        hand = (hand + 1) % MEMORY_SIZE;
                        break;
                    }
                    else
                    {
                        // Give a second chance
                        reference_bits[hand] = 0;
                        hand = (hand + 1) % MEMORY_SIZE;
                    }
                }
            }
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
/*              Working Set Algorithm           */
int subs_WS(char **paths)
{
    // Define working set window size
    int k = WS_WINDOW_SIZE;

    // Memory Block Declaration
    int memory[MEMORY_SIZE];
    for (int i = 0; i < MEMORY_SIZE; i++)
    {
        memory[i] = -1; // Indicates empty frame
    }

    // Working set queue
    int working_set_queue[MEMORY_SIZE];
    int working_set_size = 0;

    // Initialize working set queue
    for (int i = 0; i < MEMORY_SIZE; i++)
    {
        working_set_queue[i] = -1;
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

    int totalAccesses = 0;
    int pageFault = 0;
    int pageNum;
    char accessType;

    while (fscanf(files[totalAccesses % NUM_PROCESS], "%d %c", &pageNum, &accessType) == 2)
    {
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

        int found = 0;
        // Check if the page is in memory
        for (int i = 0; i < MEMORY_SIZE; i++)
        {
            if (memory[i] == pageNum)
            {
                found = 1;
                // Move the page to the end of the working set queue
                // Remove pageNum from working_set_queue
                int index = -1;
                for (int j = 0; j < working_set_size; j++)
                {
                    if (working_set_queue[j] == pageNum)
                    {
                        index = j;
                        break;
                    }
                }
                if (index != -1)
                {
                    // Shift elements to the left
                    for (int j = index; j < working_set_size - 1; j++)
                    {
                        working_set_queue[j] = working_set_queue[j + 1];
                    }
                    working_set_queue[working_set_size - 1] = pageNum;
                }
                break;
            }
        }

        if (!found)
        {
            // Page fault
            pageFault++;

            if (working_set_size < k && working_set_size < MEMORY_SIZE)
            {
                // There is space in the working set and memory
                // Find a free frame in memory
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
                    working_set_queue[working_set_size++] = pageNum;
                }
                else
                {
                    // Should not reach here if MEMORY_SIZE >= k
                    perror("Memory full, but working set not full");
                    for (int i = 0; i < NUM_PROCESS; i++)
                    {
                        fclose(files[i]);
                    }
                    return -1;
                }
            }
            else
            {
                // Working set is full, need to replace a page
                // Remove the oldest page from the working set queue
                int page_to_remove = working_set_queue[0];

                // Remove page_to_remove from memory
                for (int i = 0; i < MEMORY_SIZE; i++)
                {
                    if (memory[i] == page_to_remove)
                    {
                        memory[i] = pageNum;
                        break;
                    }
                }
                // Shift working_set_queue left
                for (int i = 0; i < working_set_size - 1; i++)
                {
                    working_set_queue[i] = working_set_queue[i + 1];
                }
                working_set_queue[working_set_size - 1] = pageNum;
            }
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