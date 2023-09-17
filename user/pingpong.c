
#include "kernel/types.h"
#include "user/user.h"

int main(int argc,int *argv[]){
    int p[2];
    
    char buf[512];

    if(pipe(p) == -1){
        fprintf(2,"pipe failed\n");
        exit(1);
    }


    int c_pid = fork();
    if(c_pid == -1){
        fprintf(2,"fork failed\n");
        exit(1);
    }

    if(c_pid == 0){
        read(p[0],buf,sizeof(buf));
        printf("%d: received ping\n",getpid());
        close(p[0]);
        write(p[1],"world",5);
        close(p[1]);
        exit(0);
    }else{
        write(p[1],"hello",5);
        close(p[1]);
        wait(0);
        read(p[0],buf,sizeof(buf));
        printf("%d: received pong\n",getpid());
        close(p[0]);
        exit(0);
    }
}