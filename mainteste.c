/* Modificado para usar apenas arq_teste */
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
int accessLogsGen(const char *path);

/* Page Algorithms Declarations */
int subs_NRU(const char *path, int print); // Not Recently Used (NRU)
int subs_2nCh(const char *path);           // Second Chance
int subs_LRU(const char *path, int print); // Aging (LRU)
int subs_WS(const char *path, int set);    // Working Set (k)

/* Main Function */
int main(int argc, char **argv)
{
    int pageFaultsLRU[NUM_ROUNDS];
    int pageFaultsNRU[NUM_ROUNDS];
    int pageFaults2nCH[NUM_ROUNDS];
    int pageFaultsWS[NUM_ROUNDS];

    // Path for single access log file
    const char *arq_teste = "Test_AccessesLog.txt";

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <algorithm> [parameters]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Running page replacement algorithms
    printf("Gerando novo log de acesso...\n");
    if (strcmp(argv[1], "REFRESH") == 0) {
        // Generating access log
        if (accessLogsGen(arq_teste) == 1) {
            perror("Error when loading access logs");
            exit(1);
        }
    } else {
        printf("Algoritmo Escolhido: %s\n", argv[1]);
        if (argc > 2) {
            printf("Parâmetro do Working Set: %s\n", argv[2]);
        }
        printf("Numero de rodadas: %d\n\n", NUM_ROUNDS);

        for (int i = 0; i < NUM_ROUNDS; i++) {
            printf("Rodada %d\n\n", i + 1);

            if (strcmp(argv[1], "LRU") == 0) {
                pageFaultsLRU[i] = subs_LRU(arq_teste, 1);
                if (pageFaultsLRU[i] == -1) {
                    perror("Error when running LRU algorithm");
                    exit(1);
                }
            } else if (strcmp(argv[1], "NRU") == 0) {
                pageFaultsNRU[i] = subs_NRU(arq_teste, 1);
                if (pageFaultsNRU[i] == -1) {
                    perror("Error when running NRU algorithm");
                    exit(1);
                }
            } else if (strcmp(argv[1], "2nCH") == 0) {
                pageFaults2nCH[i] = subs_2nCh(arq_teste);
                if (pageFaults2nCH[i] == -1) {
                    perror("Error when running Second Chance algorithm");
                    exit(1);
                }
            } else if (strcmp(argv[1], "WS") == 0) {
                if (argc < 3) {
                    perror("Missing parameter k for Working Set algorithm");
                    exit(1);
                }
                pageFaultsWS[i] = subs_WS(arq_teste, atoi(argv[2]));
                if (pageFaultsWS[i] == -1) {
                    perror("Error when running Working Set algorithm");
                    exit(1);
                }
            } else {
                perror("Invalid algorithm");
                exit(1);
            }
        }
    }

    // Calculating and printing average page faults
    int total;
    if (strcmp(argv[1], "LRU") == 0) {
        total = 0;
        for (int i = 0; i < NUM_ROUNDS; i++) {
            total += pageFaultsLRU[i];
        }
        printf("Average Page Faults LRU: %d\n", total / NUM_ROUNDS);
    } else if (strcmp(argv[1], "NRU") == 0) {
        total = 0;
        for (int i = 0; i < NUM_ROUNDS; i++) {
            total += pageFaultsNRU[i];
        }
        printf("Average Page Faults NRU: %d\n", total / NUM_ROUNDS);
    } else if (strcmp(argv[1], "2nCH") == 0) {
        total = 0;
        for (int i = 0; i < NUM_ROUNDS; i++) {
            total += pageFaults2nCH[i];
        }
        printf("Average Page Faults Second Chance: %d\n", total / NUM_ROUNDS);
    } else if (strcmp(argv[1], "WS") == 0) {
        total = 0;
        for (int i = 0; i < NUM_ROUNDS; i++) {
            total += pageFaultsWS[i];
        }
        printf("Average Page Faults Working Set (k=%d): %d\n", atoi(argv[2]), total / NUM_ROUNDS);
    }
    return 0;
}

/************************************************************************************************/
/* Access Logs Generator Function */
int accessLogsGen(const char *path) {
    FILE *file;
    srand(time(NULL));

    file = fopen(path, "w");
    if (file == NULL) {
        return 1;
    }

    for (int j = 0; j < NUM_ACCESS; j++) {
        if (RANDOM_ACCESS()) { // Read
            fprintf(file, "%d R\n", RANDOM_PAGE());
        } else { // Write
            fprintf(file, "%d W\n", RANDOM_PAGE());
        }
    }

    fclose(file);
    return 0;
}

/************************************************************************************************/
/* Modificações feitas nas funções dos algoritmos */


// Faça alterações semelhantes para `subs_NRU`, `subs_2nCh` e `subs_WS`, utilizando `path` para abrir e manipular `arq_teste`.
int subs_LRU(const char *path, int print) {
    // Declaração da memória e variáveis auxiliares
    int memory[MEMORY_SIZE];
    int reference[MEMORY_SIZE];
    int modified[MEMORY_SIZE];
    int pageFaults = 0;
    int currentSize = 0;

    // Inicializando as estruturas
    for (int i = 0; i < MEMORY_SIZE; i++) {
        memory[i] = -1;
        reference[i] = 0;
        modified[i] = 0;
    }

    FILE *file = fopen(path, "r");
    if (!file) {
        perror("Error opening access log");
        return -1;
    }

    int pageNum;
    char accessType;

    while (fscanf(file, "%d %c", &pageNum, &accessType) == 2) {
        int found = 0;

        // Verifica se a página já está na memória
        for (int i = 0; i < currentSize; i++) {
            if (memory[i] == pageNum) {
                found = 1;
                // Atualiza os bits de referência e modificação
                reference[i] = 1;
                if (accessType == 'W') {
                    modified[i] = 1;
                }
                break;
            }
        }

        if (!found) {
            // Page Fault
            pageFaults++;

            // Substituição se a memória estiver cheia
            if (currentSize == MEMORY_SIZE) {
                int indexToRemove = 0;
                for (int i = 0; i < MEMORY_SIZE; i++) {
                    if (reference[i] == 0) {
                        indexToRemove = i;
                        break;
                    }
                    reference[i] = 0; // Reseta os bits de referência
                }

                // Remove a página escolhida
                if (modified[indexToRemove] == 1) {
                    printf("Dirty page replaced: %d\n", memory[indexToRemove]);
                } else {
                    printf("Clean page replaced: %d\n", memory[indexToRemove]);
                }
                memory[indexToRemove] = pageNum;
                modified[indexToRemove] = (accessType == 'W') ? 1 : 0;
                reference[indexToRemove] = 1;
            } else {
                // Insere a página diretamente se houver espaço
                memory[currentSize] = pageNum;
                modified[currentSize] = (accessType == 'W') ? 1 : 0;
                reference[currentSize] = 1;
                currentSize++;
            }
        }

        // Imprime o estado da memória
        if (print) {
            printf("Tabela de Páginas (LRU):\n");
            printf("--------------------------------\n");
            printf("| Page | Ref | Mod |\n");
            for (int i = 0; i < MEMORY_SIZE; i++) {
                if (memory[i] != -1) {
                    printf("|  %2d  |  %d  |  %d  |\n", memory[i], reference[i], modified[i]);
                } else {
                    printf("|  --   |  -  |  -  |\n");
                }
            }
            printf("--------------------------------\n");
        }
    }

    fclose(file);
    return pageFaults;
}

int subs_NRU(const char *path, int print) {
    int memory[MEMORY_SIZE];
    int modified[MEMORY_SIZE];
    int referenced[MEMORY_SIZE];
    int pageFaults = 0;

    for (int i = 0; i < MEMORY_SIZE; i++) {
        memory[i] = -1;
        modified[i] = 0;
        referenced[i] = 0;
    }

    FILE *file = fopen(path, "r");
    if (!file) {
        perror("Error opening access log");
        return -1;
    }

    int pageNum;
    char accessType;

    while (fscanf(file, "%d %c", &pageNum, &accessType) == 2) {
        int found = 0;

        // Verifica se a página está na memória
        for (int i = 0; i < MEMORY_SIZE; i++) {
            if (memory[i] == pageNum) {
                found = 1;
                referenced[i] = 1;
                if (accessType == 'W') {
                    modified[i] = 1;
                }
                break;
            }
        }

        if (!found) {
            pageFaults++;

            // Identifica página a ser removida baseado nas classes
            int toRemove = -1;
            for (int priority = 0; priority < 4; priority++) {
                for (int i = 0; i < MEMORY_SIZE; i++) {
                    if (memory[i] != -1) {
                        int currentPriority = (modified[i] << 1) | referenced[i];
                        if (currentPriority == priority) {
                            toRemove = i;
                            break;
                        }
                    } else {
                        toRemove = i; // Posição vazia
                        break;
                    }
                }
                if (toRemove != -1) break;
            }

            // Substituição
            if (memory[toRemove] != -1) {
                if (modified[toRemove] == 1) {
                    printf("Dirty page replaced: %d\n", memory[toRemove]);
                } else {
                    printf("Clean page replaced: %d\n", memory[toRemove]);
                }
            }
            memory[toRemove] = pageNum;
            modified[toRemove] = (accessType == 'W') ? 1 : 0;
            referenced[toRemove] = 1;
        }

        // Reset dos bits de referência (simula intervalos periódicos)
        static int resetCounter = 0;
        resetCounter++;
        if (resetCounter >= NRU_RESET_INTERVAL) {
            for (int i = 0; i < MEMORY_SIZE; i++) {
                referenced[i] = 0;
            }
            resetCounter = 0;
        }

        // Imprime estado da memória
        if (print) {
            printf("Tabela de Páginas (NRU):\n");
            printf("--------------------------------\n");
            printf("| Page | Ref | Mod |\n");
            for (int i = 0; i < MEMORY_SIZE; i++) {
                if (memory[i] != -1) {
                    printf("|  %2d  |  %d  |  %d  |\n", memory[i], referenced[i], modified[i]);
                } else {
                    printf("|  --   |  -  |  -  |\n");
                }
            }
            printf("--------------------------------\n");
        }
    }

    fclose(file);
    return pageFaults;
}

int subs_2nCh(const char *path) {
    int memory[MEMORY_SIZE];
    int reference[MEMORY_SIZE];
    int modified[MEMORY_SIZE];
    int pointer = 0;
    int pageFaults = 0;

    // Inicializa memória e bits auxiliares
    for (int i = 0; i < MEMORY_SIZE; i++) {
        memory[i] = -1;
        reference[i] = 0;
        modified[i] = 0;
    }

    FILE *file = fopen(path, "r");
    if (!file) {
        perror("Error opening access log");
        return -1;
    }

    int pageNum;
    char accessType;

    while (fscanf(file, "%d %c", &pageNum, &accessType) == 2) {
        int found = 0;

        // Verifica se a página está na memória
        for (int i = 0; i < MEMORY_SIZE; i++) {
            if (memory[i] == pageNum) {
                found = 1;
                reference[i] = 1; // Atualiza o bit de referência
                if (accessType == 'W') {
                    modified[i] = 1; // Atualiza o bit de modificação
                }
                break;
            }
        }

        if (!found) {
            pageFaults++;

            // Substituição usando Second Chance
            while (reference[pointer] == 1) {
                reference[pointer] = 0; // Dá segunda chance, reseta o bit de referência
                pointer = (pointer + 1) % MEMORY_SIZE;
            }

            // Substitui a página
            if (memory[pointer] != -1) {
                if (modified[pointer] == 1) {
                    printf("Dirty page replaced: %d\n", memory[pointer]);
                } else {
                    printf("Clean page replaced: %d\n", memory[pointer]);
                }
            }
            memory[pointer] = pageNum;
            modified[pointer] = (accessType == 'W') ? 1 : 0;
            reference[pointer] = 1;

            // Avança o ponteiro circular
            pointer = (pointer + 1) % MEMORY_SIZE;
        }

        // Imprime estado da memória
        printf("Tabela de Páginas (Second Chance):\n");
        printf("--------------------------------\n");
        printf("| Page | Ref | Mod |\n");
        for (int i = 0; i < MEMORY_SIZE; i++) {
            if (memory[i] != -1) {
                printf("|  %2d  |  %d  |  %d  |\n", memory[i], reference[i], modified[i]);
            } else {
                printf("|  --   |  -  |  -  |\n");
            }
        }
        printf("--------------------------------\n");
    }

    fclose(file);
    return pageFaults;
}

int subs_WS(const char *path, int k) {
    int memory[MEMORY_SIZE];
    int modified[NUM_PAGES];
    int referenced[NUM_PAGES];
    int workingSetQueue[MEMORY_SIZE];
    int workingSetSize = 0;
    int pageFaults = 0;

    // Inicializa a memória e bits auxiliares
    for (int i = 0; i < MEMORY_SIZE; i++) {
        memory[i] = -1;
        workingSetQueue[i] = -1;
    }
    for (int i = 0; i < NUM_PAGES; i++) {
        modified[i] = 0;
        referenced[i] = 0;
    }

    FILE *file = fopen(path, "r");
    if (!file) {
        perror("Error opening access log");
        return -1;
    }

    int pageNum;
    char accessType;

    while (fscanf(file, "%d %c", &pageNum, &accessType) == 2) {
        int found = 0;

        // Verifica se a página está no conjunto de trabalho
        for (int i = 0; i < workingSetSize; i++) {
            if (workingSetQueue[i] == pageNum) {
                found = 1;

                // Move a página para o final do conjunto de trabalho
                for (int j = i; j < workingSetSize - 1; j++) {
                    workingSetQueue[j] = workingSetQueue[j + 1];
                }
                workingSetQueue[workingSetSize - 1] = pageNum;

                // Atualiza os bits
                referenced[pageNum] = 1;
                if (accessType == 'W') {
                    modified[pageNum] = 1;
                }
                break;
            }
        }

        if (!found) {
            pageFaults++;

            // Verifica se há espaço no conjunto de trabalho
            if (workingSetSize < k && workingSetSize < MEMORY_SIZE) {
                for (int i = 0; i < MEMORY_SIZE; i++) {
                    if (memory[i] == -1) {
                        memory[i] = pageNum;
                        modified[pageNum] = (accessType == 'W') ? 1 : 0;
                        referenced[pageNum] = 1;
                        workingSetQueue[workingSetSize++] = pageNum;
                        break;
                    }
                }
            } else {
                // Substitui a página mais antiga
                int pageToRemove = workingSetQueue[0];

                for (int i = 0; i < MEMORY_SIZE; i++) {
                    if (memory[i] == pageToRemove) {
                        if (modified[pageToRemove] == 1) {
                            printf("Dirty page replaced: %d\n", pageToRemove);
                        } else {
                            printf("Clean page replaced: %d\n", pageToRemove);
                        }
                        memory[i] = pageNum;
                        modified[pageNum] = (accessType == 'W') ? 1 : 0;
                        referenced[pageNum] = 1;
                        break;
                    }
                }

                // Remove do conjunto de trabalho
                for (int i = 0; i < workingSetSize - 1; i++) {
                    workingSetQueue[i] = workingSetQueue[i + 1];
                }
                workingSetQueue[workingSetSize - 1] = pageNum;
            }
        }

        // Imprime estado da memória
        printf("Tabela de Páginas (Working Set, k=%d):\n", k);
        printf("--------------------------------\n");
        printf("| Page | Ref | Mod |\n");
        for (int i = 0; i < MEMORY_SIZE; i++) {
            if (memory[i] != -1) {
                printf("|  %2d  |  %d  |  %d  |\n", memory[i], referenced[memory[i]], modified[memory[i]]);
            } else {
                printf("|  --   |  -  |  -  |\n");
            }
        }
        printf("--------------------------------\n");
    }

    fclose(file);
    return pageFaults;
}