#include <stdio.h>


void oi2(int* a){
    *a +=1;
}

void oi(int* a){
    oi2(&a);
}

int main(void){
    int a = 0;
    for (size_t i = 0; i < 10; i++)
    {
        oi(&a);
    }

    printf("%d\n", a);
    
}