#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/* Defines */
#define NUM_PROCESS 4
#define NUM_PAGES 32
#define NUM_ACCESS 120

/* Macros */
#define RANDOM_PAGE() (rand() % (NUM_PAGES))
#define RANDOM_ACCESS() (rand() % 2)

int main() {
    FILE* file;
    srand(time(NULL));
    char*  PATH_Processes[NUM_PROCESS] = {"/Users/eduardoraffaini/Dudu/T2-SO/AccessesLogs/P1_AccessesLog.txt", 
                                          "/Users/eduardoraffaini/Dudu/T2-SO/AccessesLogs/P2_AccessesLog.txt", 
                                          "/Users/eduardoraffaini/Dudu/T2-SO/AccessesLogs/P3_AccessesLog.txt", 
                                          "/Users/eduardoraffaini/Dudu/T2-SO/AccessesLogs/P4_AccessesLog.txt"};

    for (int i = 0; i < NUM_PROCESS; i++)
    {
        file = fopen(PATH_Processes[i], "w");
        if (file == NULL) {
            fprintf(stderr, "Erro opening: %s\n", PATH_Processes[i]);
            continue;
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
            printf("Page accesses for process generated.\n", PATH_Processes[i]);
            printf("Now closing!\n\n");
        }
        
        fclose(file);
    }

    return 0;
}