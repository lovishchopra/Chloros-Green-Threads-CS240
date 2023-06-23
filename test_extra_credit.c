// Test Case for Extra Credit

#include "chloros.h"
#include <stdio.h>
#include <stdlib.h>

void worker(void* arg) {
    int num = *((int*) arg);
    int i = 0;
    while(i < 35){
        printf("(%d %d)\n", num, i);
        i++;
        if (i % 15 == 0) {
            printf("Thread %d yielding voluntarily\n", num);
            thread_yield();
        }

    }
    printf("Thread %d ending operation.\n", num);
    free(arg);
}

int main() {
    thread_init();
    register_alarm(20);
    for (int i = 1; i <= 4; i++) {
        int* num = malloc(sizeof(int));
        *num = i;
        thread_spawn(&worker, num);
    }
    thread_wait();
    return 0;
}
