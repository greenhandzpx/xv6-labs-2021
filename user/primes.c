#include "kernel/types.h"
#include "user/user.h"

void func(int*);

int main()
{
    int pipefds[2];
    pipe(pipefds);
    int pid = fork();
    if (pid == 0) {
        func(pipefds);
    } else {
        int num;
        close(pipefds[0]);
        for (int i = 2; i < 36; ++i) {
            num = i;
            write(pipefds[1], &num, 4);
        }
        close(pipefds[1]);
        wait((int*)0);
    }
    exit(0);
}

void func(int* pipefds) 
{
    close(pipefds[1]);
    int p;
    if (read(pipefds[0], &p, 4) <= 0) {
        close(pipefds[0]);
        exit(0);    
    }
    printf("prime %d\n", p);
    int n;
    int pipefds2[2];
    pipe(pipefds2);
    int pid = fork();
    if (pid == 0) {
        func(pipefds2);
        exit(0);
    } else {
        int num;
        close(pipefds2[0]);
        while (read(pipefds[0], &n, 4) > 0) {
            if (n % p) {
                num = n;
                if (write(pipefds2[1], &num, 4) != 4) {
                    printf("write error\n");
                    exit(0);
                }
            }
        }
        close(pipefds[0]);
        close(pipefds2[1]);
        wait((int*)0);
    }
    exit(0);
}