#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "operations.h"

int main(int argc, char **argv) {

    int server_pipe, n;
    char op_code[1], buffer[100];
    char server_path[MAX_NAME_SIZE] = "/tmp/";

    char client_path[MAX_NAME_SIZE] = "/tmp/";

    if (argc < 2) {
        printf("Please specify the pathname of the server's pipe.\n");
        return 1;
    }

    char *pipename = argv[1];
    printf("Starting TecnicoFS server with pipe called %s\n", pipename);

    /* TO DO */
    strcat(server_path, pipename);

    unlink(server_path);
    if(mkfifo(server_path, 0777) < 0) exit(1);
    if((server_pipe = open(server_path, O_RDONLY)) < 0) exit(1);

    for(;;) {
        n = read(server_pipe, op_code, 1);
        if(n < 0) exit(1);
        printf("%c -> ", op_code[0]);
        printf(op_code[0] == '7' ? "true\n" : "false\n");
        break;
    }


    return 0;
}