#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pthread.h>
#include "operations.h"

#define MAX_SESSIONS 1

static int server_pipe;

typedef struct {
    int client_pipe;
} session;

static pthread_mutex_t session_lock;
static int active_sessions = 0;
static session sessions[MAX_SESSIONS];

int tfs_mount() {
    ssize_t n;
    int session_id;
    char pipename[MAX_NAME_SIZE];

    n = read(server_pipe, pipename, 40);
    if(n <= 0) { printf("mount: read pipename\n"); return -1; }
    /*printf("%s\n", pipename);*/

    if(pthread_mutex_lock(&session_lock) < 0) { printf("mount: lock\n"); return -1;}

    if(active_sessions >= MAX_SESSIONS) session_id = -1;
    else {
        for(int i = 0; i < MAX_SESSIONS; i++)
            if(sessions[i].client_pipe == -1) {
                session_id = i;
                sessions[session_id].client_pipe = 0;
                break;
            }
        active_sessions++;
    }

    if(pthread_mutex_unlock(&session_lock) < 0) { printf("mount: unlock\n"); return -1;}

    if((sessions[session_id].client_pipe = open(pipename, O_WRONLY)) < 0) { printf("mount: open client pipe\n"); return -1; }

    n = write(sessions[session_id].client_pipe, &session_id, sizeof(int));
    if(n <= 0) { printf("mount: write sessionid\n"); return -1; }
    /*printf("write sessionid\n");*/
    
    return 0;
}

int tfs_unmount() {
    ssize_t n;
    int session_id, client_pipe, return_value = 0;

    n = read(server_pipe, &session_id, sizeof(int));
    if(n <= 0) { printf("unmount: read sessionid\n"); return -1; }

    if(pthread_mutex_lock(&session_lock) < 0) { printf("unmount: lock\n"); return -1; }

    client_pipe = sessions[session_id].client_pipe;
    sessions[session_id].client_pipe = -1;
    active_sessions--;

    if(pthread_mutex_unlock(&session_lock) < 0) { printf("unmount: unlock\n"); return -1; }
    
    n = write(client_pipe, &return_value, sizeof(int));
    if(n <= 0) { printf("unmount: return(%d)\n", return_value); return -1; }

    close(client_pipe);

    return 0;
}

int tfs_server_open() {
    ssize_t n;
    int session_id, flags, return_value = 0;
    char filename[MAX_NAME_SIZE];

    n = read(server_pipe, &session_id, sizeof(int));
    if(n <= 0) { printf("open: read sessionid\n"); return -1; }
    /*printf("session id: %d\n", session_id);*/

    n = read(server_pipe, filename, MAX_NAME_SIZE);
    if(n <= 0) { printf("open: read filename\n"); return -1; }
    /*printf("file name: %s\n", filename);*/

    n = read(server_pipe, &flags, sizeof(int));
    if(n <= 0) { printf("open: read flags\n"); return -1; }
    /*printf("flags: %d\n", flags);*/

    return_value = tfs_open(filename, flags);

    n = write(sessions[session_id].client_pipe, &return_value, sizeof(int));
    if(n <= 0) { printf("open: write return\n"); return -1; }

    return 0;
}

int tfs_server_close() {
    ssize_t n;
    int session_id, fhandle, return_value = 0;

    n = read(server_pipe, &session_id, sizeof(int));
    if(n <= 0) { printf("close: read sessionid\n"); return -1; }
    /*printf("session id: %d\n", session_id);*/

    n = read(server_pipe, &fhandle, sizeof(int));
    if(n <= 0) { printf("close: read fhandle\n"); return -1; }
    /*printf("fhandle: %d\n", fhandle);*/

    return_value = tfs_close(fhandle);

    n = write(sessions[session_id].client_pipe, &return_value, sizeof(int));
    if(n <= 0) { printf("close: write return\n"); return -1; }

    return 0;
}

int tfs_server_write() {
    ssize_t n, return_value = 0;
    size_t length;
    int session_id, fhandle;

    n = read(server_pipe, &session_id, sizeof(int));
    if(n <= 0) { printf("write: read sessionid\n"); return -1; }
    /*printf("session id: %d\n", session_id);*/

    n = read(server_pipe, &fhandle, sizeof(int));
    if(n <= 0) { printf("write: read fhandle\n"); return -1; }
    /*printf("fhandle: %d\n", fhandle);*/

    n = read(server_pipe, &length, sizeof(size_t));
    if(n <= 0) { printf("write: read length\n"); return -1; }
    /*printf("length: %ld\n", length);*/

    char *buffer = (char*)malloc(sizeof(char)*length);
    n = read(server_pipe, buffer, length);
    if(n <= 0) { printf("write: read buffer\n"); return -1; }
    /*printf("buffer: %s\n", buffer);*/

    return_value = tfs_write(fhandle, buffer, length);

    n = write(sessions[session_id].client_pipe, &return_value, sizeof(ssize_t));
    if(n <= 0) { printf("write: write return\n"); return -1; }

    return 0;
}

int tfs_server_read() {
    ssize_t n, return_value = 0;
    size_t length;
    int session_id, fhandle;

    n = read(server_pipe, &session_id, sizeof(int));
    if(n <= 0) { printf("read: read sessionid\n"); return -1; }
    /*printf("session id: %d\n", session_id);*/

    n = read(server_pipe, &fhandle, sizeof(int));
    if(n <= 0) { printf("read: read fhandle\n"); return -1; }
    /*printf("fhandle: %d\n", fhandle);*/

    n = read(server_pipe, &length, sizeof(size_t));
    if(n <= 0) { printf("read: read length\n"); return -1; }
    /*printf("length: %ld\n", length);*/

    char *buffer = (char*)malloc(sizeof(char)*length);
    return_value = tfs_read(fhandle, buffer, length);

    n = write(sessions[session_id].client_pipe, &return_value, sizeof(ssize_t));
    if(n <= 0) { printf("read: write return\n"); return -1; }

    if(return_value != -1) {
        n = write(sessions[session_id].client_pipe, buffer, length);
        if(n <= 0) { printf("read: write buffer\n"); return -1; }
    }

    return 0;
}

int tfs_shutdown_after_all_closed(char *server_path) {
    ssize_t n;
    int session_id, return_value = 0;

    n = read(server_pipe, &session_id, sizeof(int));
    if(n <= 0) { printf("shutdown: read sessionid\n"); return -1; }
    /*printf("session id: %d\n", session_id);*/

    return_value = tfs_destroy_after_all_closed();

    n = write(sessions[session_id].client_pipe, &return_value, sizeof(int));
    if(n <= 0) { printf("shutdown: write return\n"); return -1; }

    if(return_value != -1) {
        for(int i = 0; i < MAX_SESSIONS; i++)
            close(sessions[i].client_pipe);

        if(pthread_mutex_destroy(&session_lock) < 0) { printf("shutdown: mutex destroy\n"); return -1; }
        close(server_pipe);
        unlink(server_path);
        printf("shutdown success\n");
        return 0;
    }

    return -1;    
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
    if(pthread_mutex_init(&session_lock, NULL)) { printf("main: mutex init\n"); return -1; }

    for(int i = 0; i < MAX_SESSIONS; i++)
        sessions[i].client_pipe = -1;

    if(tfs_init() == -1) { printf("main: tfs init\n"); return -1; }

    strcat(server_path, pipename);
    unlink(server_path);
    if(mkfifo(server_path, 0777) < 0) { printf("main: mkfifo\n"); return -1; }
    if((server_pipe = open(server_path, O_RDONLY)) < 0) { printf("main: open server pipe\n"); return -1; }
            

    for(;;) {
        n = read(server_pipe, &op_code, 1);
        if(n < 0) { printf("main: read opcode\n"); return -1; }
        else if(n == 0) {
            close(server_pipe);
            unlink(server_path);
            if(mkfifo(server_path, 0777) < 0) { printf("main: mkfifo\n"); return -1; }
            if((server_pipe = open(server_path, O_RDONLY)) < 0) { printf("main: open server pipe\n"); return -1; }
        }

        switch(op_code) {
        case TFS_OP_CODE_MOUNT:
        /*printf("mount\n");*/
            tfs_mount();
            break;

        case TFS_OP_CODE_UNMOUNT:
        /*printf("unmount\n");*/
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

        case TFS_OP_CODE_SHUTDOWN_AFTER_ALL_CLOSED:
        /*printf("shutdown\n");*/
            tfs_shutdown_after_all_closed(server_path);
            break;
        
        default:
            break;
        }
    }


    return 0;
}