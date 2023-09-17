#include "kernel/types.h"
#include "user/user.h"


__attribute__((noreturn))
void prime(int *pipe_left){
    int pipe_right[2];
    if(pipe(pipe_right) == -1){
        fprintf(2,"create pipe failed\n");
        exit(1);
    }
    int cur;
    if(read(pipe_left[0],&cur,sizeof(int)) == 0){
        fprintf(2,"read EOF\n");
        close(pipe_right[0]);
        close(pipe_right[1]);
        exit(1);
    }else{
        printf("prime %d\n",cur);
    }
    int c_pid = fork();
    if(c_pid == -1){
        fprintf(2,"fork failed\n");
        close(pipe_right[0]);
        close(pipe_right[1]);
        exit(1);
    }
    if(c_pid == 0){
        //child
        close(pipe_right[1]);
        prime(pipe_right);
        close(pipe_right[0]);
        exit(0);
    }else{
        close(pipe_right[0]);
        int next;
        while(read(pipe_left[0],&next,sizeof(int)) != 0 ){
             if(next % cur !=0){
                int count = write(pipe_right[1],&next,sizeof(int));
                if(count == -1){
                    fprintf(2,"write failed\n");
                    close(pipe_right[1]);
                    exit(1);
                }
            }
        }
        close(pipe_right[1]);
        wait(0);
        exit(0);
    }
}

int main(int argc,int*argv[]){
    int p[2];
    if(pipe(p) == -1){
        fprintf(2,"create pipe failed\n");
        exit(1);
    }
    int c_pid = fork();
    if(c_pid == -1){
        fprintf(2,"fork failed\n");
        exit(1);
    }
    if(c_pid > 0){
        close(p[0]);
        for(int i=2;i<=35;i++){
            int count = write(p[1],&i,sizeof(int));
            if(count == -1){
                fprintf(2,"write failed\n");
                close(p[1]);
                exit(1);
            }
        }
        close(p[1]);
        wait(0);
        exit(0);
    }else{
        close(p[1]);
        prime(p);
        close(p[0]);
        exit(0);
    }
}