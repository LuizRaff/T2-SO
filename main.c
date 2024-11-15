/*  TRABALHO 2 - SISTEMAS OPERACIONAIS                  */
/*  Alunos: Luiz Eduardo Raffaini e Isabela Braconi     */
/*  Professor: Markus Endler                            */

/* Includes */  /* Includes */
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>

/* Defines */   /* Defines */
#define NUM_PROCESS 4
#define NUM_PAGES 32
#define NUM_ACCESS 120
#define MEMOR_SIZE 16

/* Macros */    /* Macros */
#define RANDOM_PAGE() (rand() % (NUM_PAGES))
#define RANDOM_ACCESS() (rand() % 2)


/* Access Log Generator Function */
int accessLogsGen(char** paths);

/* Page Algorithms Declarations */
int subs_NRU();  // Not recently used (NRU)
int subs_2nCh(); // Second chance
int subs_LRU();  // Aging (LRU)
int subs_WS();  // Working set (k = 3 -> 5)

int main() {
    // Path for access logs file //
    char*  PATH_Processes[NUM_PROCESS] = {"AccessesLogs/P1_AccessesLog.txt", 
                                          "AccessesLogs/P2_AccessesLog.txt", 
                                          "AccessesLogs/P3_AccessesLog.txt", 
                                          "AccessesLogs/P4_AccessesLog.txt"};

    // Generating px access logs //
    if (accessLogsGen(PATH_Processes) == 1) { 
        perror("Error when loading access logs");
        exit(1);
    }

    return 0;
}

int accessLogsGen(char** paths){
    FILE* file;
    srand(time(NULL));

    for (int i = 0; i < NUM_PROCESS; i++)
    {
        file = fopen(paths[i], "w");
        if (file == NULL) {
            return 1;
        }else{
            printf("P%d_AccessesLog opened.\n", i + 1);
            for (int j = 0; j < NUM_ACCESS; j++)
            {
                if(RANDOM_ACCESS()) {   // Read
                    fprintf(file, "%d R\n", RANDOM_PAGE());
                } else {                // Write
                    fprintf(file, "%d W\n", RANDOM_PAGE());
                }
            }
            printf("Page accesses for process generated.\n", paths[i]);
            printf("Now closing!\n\n");
        }
        
        fclose(file);
    }

    return 0;
}