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
#define NUM_ROUNDS 1000

/* Macros */
#define RANDOM_PAGE() (rand() % (NUM_PAGES))
#define RANDOM_ACCESS() (rand() % 2)

/* Access Log Generator Function */
int accessLogsGen(char **paths);

/* Page Algorithms Declarations */
int subs_NRU(char **paths);         // Not Recently Used (NRU)
int subs_2nCh(char **paths);        // Second Chance
int subs_LRU(char **paths);         // Aging (LRU)
int subs_WS(char **paths, int set); // Working Set (k)

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

    printf("Algoritmo Escolhido: %s\n", argc[1]);
    if (argv > 2)
    {
        printf("Parâmetro do Working Set: %s\n", argc[2]);
    }
    printf("Numero de rodadas: %d\n\n", NUM_ROUNDS);

    for (int i = 0; i < NUM_ROUNDS; i++)
    {
        printf("Rodada %d\n\n", i + 1);
        // Generating process access logs
        if (accessLogsGen(processesPaths) == 1)
        {
            perror("Error when loading access logs");
            exit(1);
        }

        // Running page replacement algorithms

        if (strcmp(argc[1], "LRU") == 0)
        {
            pageFaultsLRU[i] = subs_LRU(processesPaths);
            if (pageFaultsLRU[i] == -1)
            {
                perror("Error when running LRU algorithm");
                exit(1);
            }
        }
        else if (strcmp(argc[1], "NRU") == 0)
        {
            pageFaultsNRU[i] = subs_NRU(processesPaths);
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
    int modificado[MEMORY_SIZE];
} LRU_Fila;

// Initializes the queue
void inicializarFila(LRU_Fila *fila)
{
    fila->tamanho = 0;
    for (int i = 0; i < MEMORY_SIZE; i++)
    {
        fila->dados[i] = -1;
        fila->modificado[i] = 0;
    }
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

// Returns the index of the value in the queue, or -1 if not found
int indexOf(LRU_Fila *fila, int valor)
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
                fila->modificado[j] = fila->modificado[j + 1];
            }
            fila->tamanho--;
            return;
        }
    }
}

// Adds a value to the queue
void adicionar(LRU_Fila *fila, int valor, int *pageFault, int pageModified)
{
    if (contem(fila, valor))
    {
        // Remove the value if it already exists
        removerValor(fila, valor);
    }
    else
    {
        // Page fault occurs
        (*pageFault)++;
        // If the queue is full, remove the least recently used page
        if (fila->tamanho == MEMORY_SIZE)
        {
            printf("Page fault: %d\n", valor);
            printf("Page to remove: %d\n", fila->dados[0]);
            if (fila->modificado[0] == 1)
            {
                printf("Dirty page replaced and written back to swap area.\n\n");
            }
            else
            {
                printf("Clean page replaced.\n\n");
            }
            // Shift all elements to the left to remove the oldest page
            for (int i = 0; i < MEMORY_SIZE - 1; i++)
            {
                fila->dados[i] = fila->dados[i + 1];
                fila->modificado[i] = fila->modificado[i + 1];
            }
            fila->tamanho--;
        }
    }

    // Add the value to the end of the queue
    fila->dados[fila->tamanho] = valor;
    fila->modificado[fila->tamanho] = pageModified;
    fila->tamanho++;
}

void imprimirFila(LRU_Fila *fila)
{
    printf("Fila: \n");
    for (int i = 0; i < fila->tamanho; i++)
    {
        printf("%d %d\n", fila->dados[i], fila->modificado[i]);
    }
    printf("\n");
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

        int pageModified = (accessType == 'W') ? 1 : 0;

        // Add the page to the queue
        adicionar(&lru_Fila, pageNum, &pageFault, pageModified);

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
            int whichCase = (pageReferenced[i] << 1) | pageModified[i];
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
                printf("Dirty page replaced and written back to swap area.\n\n");
            }
            else
            {
                printf("Clean page replaced.\n\n");
            }

            // Replace the page in memory and update bits
            memory[pageToRemove] = pageNum;
            pageReferenced[pageToRemove] = (accessType == 'R') ? 1 : 0;
            pageModified[pageToRemove] = (accessType == 'W') ? 1 : 0;
        }

        // accessesSinceReset++;
        // if (accessesSinceReset >= NRU_RESET_INTERVAL)
        // {
        //     for (int i = 0; i < MEMORY_SIZE; i++)
        //     {
        //         pageReferenced[i] = 0;
        //     }
        //     accessesSinceReset = 0;
        // }

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
    int memory[MEMORY_SIZE];                       // Array to simulate physical memory frames
    int reference_bits_of_each_frame[MEMORY_SIZE]; // Array to keep track of reference bits for each frame
    int circular_queue_pointer = 0;                // Pointer for the circular queue (the "circular_queue_pointer" of the clock)

    // Initialize memory and reference bits
    for (int i = 0; i < MEMORY_SIZE; i++)
    {
        memory[i] = -1;                      // Initialize all memory frames to -1, indicating they are empty
        reference_bits_of_each_frame[i] = 0; // Initialize all reference bits to 0
    }

    // Opening access log files for each process
    FILE *files[NUM_PROCESS];
    for (int i = 0; i < NUM_PROCESS; i++)
    {
        files[i] = fopen(paths[i], "r"); // Open the file corresponding to process i
        if (files[i] == NULL)
        {
            // If opening any file fails, close all previously opened files and return an error
            for (int j = 0; j < i; j++)
            {
                fclose(files[j]);
            }
            perror("Error opening files");
            return -1;
        }
    }

    int totalAccesses = 0; // Total number of memory accesses processed
    int pageFault = 0;     // Total number of page faults encountered
    int pageNum;           // Variable to store the page number read from the file
    char accessType;       // Variable to store the access type ('R' for read, 'W' for write)

    // Main loop to process memory accesses
    while (fscanf(files[totalAccesses % NUM_PROCESS], "%d %c", &pageNum, &accessType) == 2)
    {
        // Check if the page number is valid (non-negative)
        if (pageNum < 0)
        {
            perror("Invalid page number");
            // Close all open files before exiting due to error
            for (int i = 0; i < NUM_PROCESS; i++)
            {
                fclose(files[i]);
            }
            return -1;
        }

        int found = 0; // Flag to indicate whether the page is found in memory

        // Check if the page is already in memory (Page Hit)
        for (int i = 0; i < MEMORY_SIZE; i++)
        {
            if (memory[i] == pageNum)
            {
                // Page is found in memory
                reference_bits_of_each_frame[i] = 1; // Set the reference bit to 1 (page has been recently used)
                found = 1;                           // Set the found flag
                break;                               // Exit the loop since the page is found
            }
        }

        if (!found)
        {
            // Page Fault occurs since the page is not in memory
            pageFault++;

            // Check for a free frame in memory
            int free_frame = -1;
            for (int i = 0; i < MEMORY_SIZE; i++)
            {
                if (memory[i] == -1)
                {
                    free_frame = i; // Found an empty frame
                    break;
                }
            }

            if (free_frame != -1)
            {
                // There is a free frame available
                memory[free_frame] = pageNum;                 // Load the new page into the free frame
                reference_bits_of_each_frame[free_frame] = 1; // Set the reference bit
            }
            else
            {
                // No free frames available; apply the Second Chance algorithm
                while (1)
                {
                    // Check the reference bit of the page at the current 'circular_queue_pointer' position
                    if (reference_bits_of_each_frame[circular_queue_pointer] == 0)
                    {
                        // Reference bit is 0; replace this page
                        memory[circular_queue_pointer] = pageNum;                            // Replace the page in memory with the new page
                        reference_bits_of_each_frame[circular_queue_pointer] = 1;            // Set the reference bit for the new page
                        circular_queue_pointer = (circular_queue_pointer + 1) % MEMORY_SIZE; // Move the circular_queue_pointer to the next position
                        break;                                                               // Exit the loop after replacement
                    }
                    else
                    {
                        // Reference bit is 1; give a second chance
                        reference_bits_of_each_frame[circular_queue_pointer] = 0;            // Reset the reference bit to 0
                        circular_queue_pointer = (circular_queue_pointer + 1) % MEMORY_SIZE; // Move the circular_queue_pointer to the next position
                        // Continue the loop to check the next page
                    }
                }
            }
        }

        totalAccesses++; // Increment the total number of accesses processed
    }

    // Closing all open files after processing is complete
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
/*              Working Set Algorithm           */
int subs_WS(char **paths, int set)
{
    // Define working set window size (k)
    int k = set; // This value determines how many pages can be in the working set at once

    // Memory Block Declaration
    int memory[MEMORY_SIZE];
    for (int i = 0; i < MEMORY_SIZE; i++)
    {
        memory[i] = -1; // Initialize all memory frames to -1, indicating they are empty
    }

    // Working set queue to keep track of pages in the working set
    int working_set_queue[MEMORY_SIZE];
    int number_of_pages_WS = 0; // Current number of pages in the working set

    // Initialize working set queue
    for (int i = 0; i < MEMORY_SIZE; i++)
    {
        working_set_queue[i] = -1; // Set all positions to -1, indicating no pages are loaded yet
    }

    // Opening access log files for each process
    FILE *files[NUM_PROCESS];
    for (int i = 0; i < NUM_PROCESS; i++)
    {
        files[i] = fopen(paths[i], "r"); // Open the file corresponding to process i
        if (files[i] == NULL)
        {
            // If opening any file fails, close all previously opened files and return an error
            for (int j = 0; j < i; j++)
            {
                fclose(files[j]);
            }
            perror("Error opening files");
            return -1;
        }
    }

    int totalAccesses = 0; // Total number of memory accesses processed
    int pageFault = 0;     // Total number of page faults encountered
    int pageNum;           // Variable to store the page number read from the file
    char accessType;       // Variable to store the access type ('R' for read, 'W' for write)

    // Main loop to process memory accesses
    while (fscanf(files[totalAccesses % NUM_PROCESS], "%d %c", &pageNum, &accessType) == 2)
    {
        // Check if the page number is valid (non-negative)
        if (pageNum < 0)
        {
            perror("Invalid page number");
            // Close all open files before exiting due to error
            for (int i = 0; i < NUM_PROCESS; i++)
            {
                fclose(files[i]);
            }
            return -1;
        }

        int found = 0; // Flag to indicate whether the page is found in memory

        // Check if the page is already loaded in memory
        for (int i = 0; i < MEMORY_SIZE; i++)
        {
            if (memory[i] == pageNum)
            {
                found = 1; // Page is found in memory

                // Move the page to the end of the working set queue (most recently used)
                // First, find the index of the page in the working set queue
                int index = -1;
                for (int j = 0; j < number_of_pages_WS; j++)
                {
                    if (working_set_queue[j] == pageNum)
                    {
                        index = j; // Found the page's index in the working set queue
                        break;
                    }
                }
                if (index != -1)
                {
                    // Shift all pages after the found index to the left
                    for (int j = index; j < number_of_pages_WS - 1; j++)
                    {
                        working_set_queue[j] = working_set_queue[j + 1];
                    }
                    // Place the accessed page at the end of the working set queue
                    working_set_queue[number_of_pages_WS - 1] = pageNum;
                }
                break; // Exit the loop since the page is found
            }
        }

        if (!found)
        {
            // Page fault occurs since the page is not in memory
            pageFault++;

            // Check if there is space in the working set and memory
            if (number_of_pages_WS < k && number_of_pages_WS < MEMORY_SIZE)
            {
                // There is space to load the new page

                // Find a free frame in memory
                int free_frame = -1;
                for (int i = 0; i < MEMORY_SIZE; i++)
                {
                    if (memory[i] == -1)
                    {
                        free_frame = i; // Found an empty frame
                        break;
                    }
                }
                if (free_frame != -1)
                {
                    // Load the new page into the free memory frame
                    memory[free_frame] = pageNum;

                    // Add the new page to the working set queue
                    working_set_queue[number_of_pages_WS++] = pageNum; // Increment the working set size
                }
                else
                {
                    // This situation should not occur if MEMORY_SIZE >= k
                    perror("Memory full, but working set not full");
                    // Close all files before exiting due to error
                    for (int i = 0; i < NUM_PROCESS; i++)
                    {
                        fclose(files[i]);
                    }
                    return -1;
                }
            }
            else // The working set is full; need to replace a page
            {

                // Remove the oldest page from the working set queue
                int page_to_remove = working_set_queue[0]; // Oldest page

                // Remove the page to remove from memory
                for (int i = 0; i < MEMORY_SIZE; i++)
                {
                    if (memory[i] == page_to_remove)
                    {
                        // Replace the old page with the new page in memory
                        memory[i] = pageNum;
                        break;
                    }
                }

                // Shift the working set queue to the left to remove the oldest page
                for (int i = 0; i < number_of_pages_WS - 1; i++)
                {
                    working_set_queue[i] = working_set_queue[i + 1];
                }

                // Place the new page at the end of the working set queue
                working_set_queue[number_of_pages_WS - 1] = pageNum;
            }
        }

        totalAccesses++; // Increment the total number of accesses processed
    }

    // Closing all open files after processing is complete
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