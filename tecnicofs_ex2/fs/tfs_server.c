#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "operations.h"

#define MAX_SESSIONS 1

static int server_pipe;

typedef struct {
    int client_pipe;
} session;

static int active_sessions = 0;
static session single;

int tfs_mount() {
    ssize_t n;
    int session_id = 1;
    char pipename[MAX_NAME_SIZE];

    n = read(server_pipe, pipename, 40);
    if(n <= 0) return -1;
    /*printf("%s\n", pipename);*/

    if((single.client_pipe = open(pipename, O_WRONLY)) < 0) return -1;
    /*printf("open client\n");*/

    if(++active_sessions > MAX_SESSIONS) {
        close(single.client_pipe);
        return -1;
    }

    n = write(single.client_pipe, &session_id, sizeof(int));
    if(n <= 0) {
        close(single.client_pipe);
        return -1;
    }
    /*printf("write sessionid\n");*/
    
    return 0;
}

int tfs_unmount() {
    ssize_t n;
    int session_id, return_value = 0;

    n = read(server_pipe, &session_id, sizeof(int));
    if(n <= 0) return -1;

    active_sessions--;
    /* something more complex */
    
    n = write(single.client_pipe, &return_value, sizeof(int));
    if(n <= 0) return -1;

    close(single.client_pipe);

    return 0;
}

int tfs_server_open() {
    ssize_t n;
    int session_id, flags, return_value = 0;
    char filename[MAX_NAME_SIZE];

    n = read(server_pipe, &session_id, sizeof(int));
    if(n <= 0) return -1;
    /*printf("session id: %d\n", session_id);*/

    n = read(server_pipe, filename, MAX_NAME_SIZE);
    if(n <= 0) return -1;
    /*printf("file name: %s\n", filename);*/

    n = read(server_pipe, &flags, sizeof(int));
    if(n <= 0) return -1;
    /*printf("flags: %d\n", flags);*/

    return_value = tfs_open(filename, flags);

    n = write(single.client_pipe, &return_value, sizeof(int));
    if(n <= 0) return -1;

    return 0;
}

int tfs_server_close() {
    ssize_t n;
    int session_id, fhandle, return_value = 0;

    n = read(server_pipe, &session_id, sizeof(int));
    if(n <= 0) return -1;
    /*printf("session id: %d\n", session_id);*/

    n = read(server_pipe, &fhandle, sizeof(int));
    if(n <= 0) return -1;
    /*printf("fhandle: %d\n", fhandle);*/

    return_value = tfs_close(fhandle);

    n = write(single.client_pipe, &return_value, sizeof(int));
    if(n <= 0) return -1;

    return 0;
}

int tfs_server_write() {
    ssize_t n, return_value = 0;
    size_t length;
    int session_id, fhandle;

    n = read(server_pipe, &session_id, sizeof(int));
    if(n <= 0) return -1;
    /*printf("session id: %d\n", session_id);*/

    n = read(server_pipe, &fhandle, sizeof(int));
    if(n <= 0) return -1;
    /*printf("fhandle: %d\n", fhandle);*/

    n = read(server_pipe, &length, sizeof(size_t));
    if(n <= 0) return -1;
    /*printf("length: %ld\n", length);*/

    char *buffer = (char*)malloc(sizeof(char)*length);
    n = read(server_pipe, buffer, length);
    if(n <= 0) return -1;
    /*printf("buffer: %s\n", buffer);*/

    return_value = tfs_write(fhandle, buffer, length);

    n = write(single.client_pipe, &return_value, sizeof(ssize_t));
    if(n <= 0) return -1;

    return 0;
}

int tfs_server_read() {
    ssize_t n, return_value = 0;
    size_t length;
    int session_id, fhandle;

    n = read(server_pipe, &session_id, sizeof(int));
    if(n <= 0) return -1;
    /*printf("session id: %d\n", session_id);*/

    n = read(server_pipe, &fhandle, sizeof(int));
    if(n <= 0) return -1;
    /*printf("fhandle: %d\n", fhandle);*/

    n = read(server_pipe, &length, sizeof(size_t));
    if(n <= 0) return -1;
    /*printf("length: %ld\n", length);*/

    char *buffer = (char*)malloc(sizeof(char)*length);
    return_value = tfs_read(fhandle, buffer, length);

    n = write(single.client_pipe, &return_value, sizeof(ssize_t));
    if(n <= 0) return -1;

    if(return_value != -1) {
        n = write(single.client_pipe, buffer, length);
        if(n <= 0) return -1;
    }

    return 0;
}

int main(int argc, char **argv) {

    ssize_t n;
    char op_code;
    char server_path[MAX_NAME_SIZE] = "/tmp/";

    if (argc < 2) {
        printf("Please specify the pathname of the server's pipe.\n");
        return 1;
    }

    char *pipename = argv[1];
    printf("Starting TecnicoFS server with pipe called %s\n", pipename);

    /* TO DO */

    if(tfs_init() == -1) exit(1);

    strcat(server_path, pipename);
    unlink(server_path);
    if(mkfifo(server_path, 0777) < 0) exit(1);
    if((server_pipe = open(server_path, O_RDONLY)) < 0) exit(1);
            

    for(;;) {
        n = read(server_pipe, &op_code, 1);
        if(n <= 0) exit(1);

        switch(op_code) {
        case TFS_OP_CODE_MOUNT:
        /*printf("mount\n");*/
            tfs_mount();
            break;

        case TFS_OP_CODE_UNMOUNT:
        /**/printf("unmount\n");
            tfs_unmount();
            break;

        case TFS_OP_CODE_OPEN:
        /*printf("open\n");*/
            tfs_server_open();
            break;

        case TFS_OP_CODE_CLOSE:
        /*printf("close\n");*/
            tfs_server_close();
            break;

        case TFS_OP_CODE_WRITE:
        /*printf("write\n");*/
            tfs_server_write();
            break;

        case TFS_OP_CODE_READ:
        /*printf("read\n");*/
            tfs_server_read();
            break;
        
        default:
            break;
        }
    }


    return 0;
}