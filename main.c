/*  TRABALHO 2 - SISTEMAS OPERACIONAIS                  */
/*  Alunos: Luiz Eduardo Raffaini e Isabela Braconi     */
/*  Professor: Markus Endler                            */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/* Defines */
#define MEMOR_SIZE 16

/* Macros */

/* Page Algorithms Declarations */
int subs_NRU();  // Not recently used (NRU)
int subs_2nCh(); // Second chance
int subs_LRU();  // Aging (LRU)
int subs_WS();  // Working set (k = 3 -> 5)

int main() {
    char *args[] = {"./pagesGenerator", NULL};
    if (execvp(args[0], args) == -1) {  // Generating px access logs
        perror("Erro ao executar o programa");
    }

    return 0;
}