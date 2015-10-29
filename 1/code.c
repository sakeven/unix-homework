#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

char cmd[256];

int main(void){
    int ret;
    
    printf("Hello world, this is Linux!");
    
    while(1){
        printf(">");
        fgets(cmd, 256, stdin);
        cmd[strlen(cmd)-1] = 0;
        
        if(fork() == 0){
            execlp(cmd, cmd, NULL);
            perror(cmd);
            exit(errno);
        } else {
            wait(&ret);
            printf("child process return %d \n", ret);
        }
    
    }
}
