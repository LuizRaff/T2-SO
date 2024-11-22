/*  TRABALHO 2 - SISTEMAS OPERACIONAIS                  */
/*  Alunos: Luiz Eduardo Raffaini e Isabela Braconi     */
/*  Professor: Markus Endler                            */

/* Includes */ /* Includes */
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>

/* Defines */ /* Defines */
#define NUM_PROCESS 4
#define NUM_PAGES 32
#define NUM_ACCESS 100
#define MEMORY_SIZE 16
#define NRU_RESET_INTERVAL 15
#define NUM_ROUNDS 1000

/* Macros */ /* Macros */
#define RANDOM_PAGE() (rand() % (NUM_PAGES))
#define RANDOM_ACCESS() (rand() % 2)

/* Access Log Generator Function */
int accessLogsGen(char **paths);

/* Page Algorithms Declarations */
int subs_NRU();  // Not recently used (NRU)
int subs_2nCh(); // Second chance
int subs_LRU();  // Aging (LRU)
int subs_WS();   // Working set (k = 3 -> 5)

int main()
{
    int pageFaultsLRU[NUM_ROUNDS];
    int pageFaultsNRU[NUM_ROUNDS];
    // Path for access logs file //
    char *processesPaths[NUM_PROCESS] = {"AccessesLogs/P1_AccessesLog.txt",
                                         "AccessesLogs/P2_AccessesLog.txt",
                                         "AccessesLogs/P3_AccessesLog.txt",
                                         "AccessesLogs/P4_AccessesLog.txt"};

    for (int i = 0; i < NUM_ROUNDS; i++)
    {

        // Generating px access logs //
        if (accessLogsGen(processesPaths) == 1)
        {
            perror("Error when loading access logs");
            exit(1);
        }

        // printf("Access logs generated successfully!\n");

        // printf("Running page replacement algorithms...\n");
        // printf("Please wait...\n");
        // printf("\nLRU:\n");
        // Page replacement algorithm LRU //
        pageFaultsLRU[i] = subs_LRU(processesPaths);
        if (pageFaultsLRU[i] == -1)
        {
            perror("Error when running NRU algorithm");
            exit(1);
        }
        // printf("\nNRU:\n");
        // Page replacement algorithm NRU //
        pageFaultsNRU[i] = subs_NRU(processesPaths);
        if (pageFaultsNRU[i] == -1)
        {
            perror("Error when running NRU algorithm");
            exit(1);
        }
    }

    int total;
    total = 0;
    printf("Page Faults LRU:\n");
    for (int i = 0; i < NUM_ROUNDS; i++)
    {
        total += pageFaultsLRU[i];
    }
    printf("Total: %d\n", total / NUM_ROUNDS);

    total = 0;
    printf("Page Faults NRU:\n");
    for (int i = 0; i < NUM_ROUNDS; i++)
    {
        total += pageFaultsNRU[i];
    }
    printf("Total: %d\n", total / NUM_ROUNDS);

    return 0;
}

typedef struct
{
    int dados[MEMORY_SIZE];
    int tamanho;
} LRU_Fila;

// Inicializa a fila
void inicializarFila(LRU_Fila *fila)
{
    fila->tamanho = 0;
}

// Verifica se um valor já está na fila
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

// Remove um valor específico da fila
void removerValor(LRU_Fila *fila, int valor)
{
    int i, j;
    for (i = 0; i < fila->tamanho; i++)
    {
        if (fila->dados[i] == valor)
        {
            // Remove o valor deslocando os elementos
            for (j = i; j < fila->tamanho - 1; j++)
            {
                fila->dados[j] = fila->dados[j + 1];
            }
            fila->tamanho--;
            return;
        }
    }
}

// Adiciona um valor na fila
void adicionar(LRU_Fila *fila, int valor, int *pageFault)
{
    if (contem(fila, valor))
    {
        // Remove o valor se já existir
        removerValor(fila, valor);
    }
    else
    {
        *pageFault += 1;
    }

    // Adiciona o valor no final da fila
    if (fila->tamanho < MEMORY_SIZE)
    {
        fila->dados[fila->tamanho++] = valor;
    }
    else
    {
        // Remove o primeiro elemento para abrir espaço
        for (int i = 0; i < MEMORY_SIZE - 1; i++)
        {
            fila->dados[i] = fila->dados[i + 1];
        }
        fila->dados[MEMORY_SIZE - 1] = valor;
    }
}

// Exibe os elementos da fila
void imprimirFila(LRU_Fila *fila)
{
    printf("Fila: ");
    for (int i = 0; i < fila->tamanho; i++)
    {
        printf("%d ", fila->dados[i]);
    }
    printf("\n");
}

int subs_LRU(char **paths)
{
    // Memory Block Declaration //
    LRU_Fila lru_Fila;
    inicializarFila(&lru_Fila);

    // Opening files //
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
            return 1;
        }
    }

    int totalAccesses = 0;
    int pageNum;
    char accessType;
    int pageFault = 0;
    while (fscanf(files[totalAccesses % NUM_PROCESS], "%d %c", &pageNum, &accessType) == 2)
    {
        // Verifica se o numero da pagina e valida
        if (pageNum < 0)
        {
            perror("Invalid page number");
            for (int i = 0; i < NUM_PROCESS; i++)
            {
                fclose(files[i]);
            }
            return 1;
        }

        // Adiciona a pagina na fila
        adicionar(&lru_Fila, pageNum, &pageFault);
        // imprimirFila(&lru_Fila);
        totalAccesses++;
    }

    // printf("Total Accesses: %d\n", totalAccesses);
    // printf("Page Faults: %d\n", pageFault);

    // Closing files //
    for (int i = 0; i < NUM_PROCESS; i++)
    {
        fclose(files[i]);
    }

    return pageFault;
}

int NRU_whichPageToRemove(int *memory, int *pageModified, int *pageReferenced, int pageNum)
{
    /*
    Prioridades das classes de páginas no algoritmo NRU:
                                             | p | M | R |
    Caso 0: não modificada, não referenciada | p | 0 | 0 |
    Caso 1: não modificada, referenciada     | p | 0 | 1 |
    Caso 2: modificada, não referenciada     | p | 1 | 0 |
    Caso 3: modificada, referenciada         | p | 1 | 1 |
    */

    // Busca por um espaço vazio na memória
    for (int i = 0; i < MEMORY_SIZE; i++)
    {
        if (memory[i] == -1)
        {
            return i;
        }
    }

    // Verifica se a página já está na memória
    for (int i = 0; i < MEMORY_SIZE; i++)
    {
        if (memory[i] == pageNum)
        {
            return i;
        }
    }

    // Identifica a página a ser removida com base nos casos de prioridade
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
    // Memory Block Declaration //
    int memory[MEMORY_SIZE];
    int pageModified[MEMORY_SIZE];
    int pageReferenced[MEMORY_SIZE];
    for (int i = 0; i < MEMORY_SIZE; i++)
    {
        memory[i] = -1;
        pageModified[i] = 0;
        pageReferenced[i] = 0;
    }

    // Opening files //
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
        // Verifica se o número da página é válido
        if (pageNum < 0)
        {
            perror("Invalid page number");
            for (int i = 0; i < NUM_PROCESS; i++)
            {
                fclose(files[i]);
            }
            return -1;
        }

        // Seleciona a pagina a ser removida
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

        // Altera pagina da memoria pela nova pagina e atualiza os bits de modificacao e referencia de acordo com o acesso
        if (memory[pageToRemove] == pageNum)
        {
            // Page hit
            if (accessType == 'W')
            {
                pageModified[pageToRemove] = 1;
            }
            else
            {
                pageReferenced[pageToRemove] = 1;
            }
        }
        else
        {
            // Page fault
            pageFault++;
            memory[pageToRemove] = pageNum;
            pageReferenced[pageToRemove] = 1;
            pageModified[pageToRemove] = (accessType == 'W') ? 1 : 0;
            pageModified[pageToRemove] = (accessType == 'R') ? 1 : 0;
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

    // Closing files //
    for (int i = 0; i < NUM_PROCESS; i++)
    {
        fclose(files[i]);
    }

    return pageFault;
}

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