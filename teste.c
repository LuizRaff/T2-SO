#include <stdio.h>

void oi(int* a){
    *a +=1;
}

int main(void){
    int a = 0;
    for (size_t i = 0; i < 10; i++)
    {
        oi(&a);
    }

    printf("%d\n", a);
    
}