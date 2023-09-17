#include "kernel/types.h"
#include "kernel/param.h"
#include "user/user.h"

void xargs(char *path,char *argv[]){
    if(fork() == 0){
        exec(path,argv);
        exit(0);
    }
    else{
        wait(0);
    }
}

int main(int argc,char *argv[]){
    char after_argv[MAXARG];
    memset(after_argv,0,sizeof(after_argv));
    char buf[MAXARG];
    int i;
    char *new_argv[MAXARG];
    char *q = after_argv;
    int num;
    while((num = read(0,q,after_argv + MAXARG - q)) > 0){
        q = q + num;
    }
    char *path;
    path = argv[1];
    for(i=1;i<argc;i++){
        new_argv[i-1] = argv[i];
    }
    char *p;
    p = after_argv;
    q = after_argv;
    while(*q){
        p =  strchr(q,'\n');
        int len = p - q;
        memcpy(buf,q,len);
        buf[len] = 0;
        q = p + 1;
        new_argv[i-1] = buf;
        xargs(path,new_argv);
    }
    exit(0);
}