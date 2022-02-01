#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pthread.h>
#include "operations.h"

#define MAX_SESSIONS 10

static int server_pipe;
char server_path[MAX_NAME_SIZE] = "/tmp/";

typedef struct {
    int id;
    int client_pipe;
    pthread_t tid;
} session;

static pthread_mutex_t session_lock;
static pthread_mutex_t read_lock;
static int active_sessions = 0;
static session sessions[MAX_SESSIONS];

int tfs_mount() {
    /**/printf("mount\n");
    ssize_t n;
    int session_id, client_pipe;
    char pipename[MAX_NAME_SIZE];

    n = read(server_pipe, pipename, 40);
    if(n <= 0) { printf("mount: read pipename\n"); pthread_mutex_unlock(&read_lock); return -1; }
    /**/printf("%s\n", pipename);

    if(pthread_mutex_unlock(&read_lock) < 0) { printf("mount: read unlock\n"); return -1;}

    if(pthread_mutex_lock(&session_lock) < 0) { printf("mount: session lock\n"); return -1;}

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

    if(pthread_mutex_unlock(&session_lock) < 0) { printf("mount: session unlock\n"); return -1;}

    if((client_pipe = open(pipename, O_WRONLY)) < 0) { printf("mount: open client pipe(%s) error(%d)\n", pipename, sessions[session_id].client_pipe); return -1; }

    if(session_id != -1) sessions[session_id].client_pipe = client_pipe;

    n = write(client_pipe, &session_id, sizeof(int));
    if(n <= 0) { printf("mount: write sessionid\n"); return -1; }
    /*printf("write sessionid\n");
    printf("mount: id(%d, %d), pipe(%d)\n", session_id, sessions[session_id].id, sessions[session_id].client_pipe);
    */
    return 0;
}

void* tfs_unmount(void* arg) {
    /**/printf("unmount\n");
    ssize_t n;
    int client_pipe, return_value = 0;
    session* s = (session*)arg;

    if(pthread_mutex_unlock(&read_lock) < 0) { printf("unmount: read unlock\n"); return NULL;}

    if(pthread_mutex_lock(&session_lock) < 0) { printf("unmount: session lock\n"); return NULL; }

    client_pipe = sessions[s->id].client_pipe;
    sessions[s->id].client_pipe = -1;
    active_sessions--;

    if(pthread_mutex_unlock(&session_lock) < 0) { printf("unmount: session unlock\n"); return NULL; }
    
    n = write(client_pipe, &return_value, sizeof(int));
    if(n <= 0) { printf("unmount: return(%d)\n", return_value); return NULL; }

    close(client_pipe);
    return NULL;
}

void* tfs_server_open(void* arg) {
    /**/printf("open\n");
    ssize_t n;
    session* s = (session*)arg;
    int flags, return_value = 0;
    char filename[MAX_NAME_SIZE];

    n = read(server_pipe, filename, MAX_NAME_SIZE);
    if(n <= 0) { printf("open: read filename\n"); pthread_mutex_unlock(&read_lock); return NULL; }
    /*printf("file name: %s\n", filename);*/

    n = read(server_pipe, &flags, sizeof(int));
    if(n <= 0) { printf("open: read flags\n"); pthread_mutex_unlock(&read_lock); return NULL; }
    /*printf("flags: %d\n", flags);*/

    if(pthread_mutex_unlock(&read_lock) < 0) { printf("open: read unlock\n"); return NULL;}

    return_value = tfs_open(filename, flags);

    n = write(s->client_pipe, &return_value, sizeof(int));
    if(n <= 0) { printf("open: write return\n"); return NULL; }

    return NULL;
}

void* tfs_server_close(void* arg) {
    /**/printf("close\n");
    ssize_t n;
    session* s = (session*)arg;
    int fhandle, return_value = 0;

    n = read(server_pipe, &fhandle, sizeof(int));
    if(n <= 0) { printf("close: read fhandle\n"); pthread_mutex_unlock(&read_lock); return NULL; }
    /*printf("fhandle: %d\n", fhandle);*/

    if(pthread_mutex_unlock(&read_lock) < 0) { printf("close: read unlock\n"); return NULL;}

    return_value = tfs_close(fhandle);

    n = write(s->client_pipe, &return_value, sizeof(int));
    if(n <= 0) { printf("close: write return\n"); return NULL; }

    return NULL;
}

void* tfs_server_write(void* arg) {
    /**/printf("write\n");
    ssize_t n, return_value = 0;
    size_t length;
    session* s = (session*)arg;
    int fhandle;

    n = read(server_pipe, &fhandle, sizeof(int));
    if(n <= 0) { printf("write: read fhandle\n"); pthread_mutex_unlock(&read_lock); return NULL; }
    /*printf("fhandle: %d\n", fhandle);*/

    n = read(server_pipe, &length, sizeof(size_t));
    if(n <= 0) { printf("write: read length\n"); pthread_mutex_unlock(&read_lock); return NULL; }
    /*printf("length: %ld\n", length);*/

    char *buffer = (char*)malloc(sizeof(char)*length);
    n = read(server_pipe, buffer, length);
    if(n <= 0) { printf("write: read buffer\n"); pthread_mutex_unlock(&read_lock); return NULL; }
    /*printf("buffer: %s\n", buffer);*/

    if(pthread_mutex_unlock(&read_lock) < 0) { printf("write: read unlock\n"); return NULL;}

    return_value = tfs_write(fhandle, buffer, length);

    n = write(s->client_pipe, &return_value, sizeof(ssize_t));
    if(n <= 0) { printf("write: write return\n"); return NULL; }

    free(buffer);

    return NULL;
}

void* tfs_server_read(void* arg) {
    /**/printf("read\n");
    ssize_t n, return_value = 0;
    size_t length;
    session* s = (session*)arg;
    int fhandle;

    n = read(server_pipe, &fhandle, sizeof(int));
    if(n <= 0) { printf("read: read fhandle\n"); pthread_mutex_unlock(&read_lock); return NULL; }
    /*printf("fhandle: %d\n", fhandle);*/

    n = read(server_pipe, &length, sizeof(size_t));
    if(n <= 0) { printf("read: read length\n"); pthread_mutex_unlock(&read_lock); return NULL; }
    /*printf("length: %ld\n", length);*/

    if(pthread_mutex_unlock(&read_lock) < 0) { printf("read: read unlock\n"); return NULL;}

    char *buffer = (char*)malloc(sizeof(char)*length);
    return_value = tfs_read(fhandle, buffer, length);

    n = write(s->client_pipe, &return_value, sizeof(ssize_t));
    if(n <= 0) { printf("read: write return\n"); return NULL; }

    if(return_value != -1) {
        n = write(s->client_pipe, buffer, (size_t)return_value);
        if(n <= 0) { printf("read: write buffer\n"); return NULL; }
    }

    free(buffer);

    return NULL;
}

void* tfs_shutdown_after_all_closed(void* arg) {
    /**/printf("shutdown\n");
    ssize_t n;
    session* s = (session*)arg;
    int return_value = 0;

    if(pthread_mutex_unlock(&read_lock) < 0) { printf("shutdown: read unlock\n"); return NULL;}

    return_value = tfs_destroy_after_all_closed();

    n = write(s->client_pipe, &return_value, sizeof(int));
    if(n <= 0) { printf("shutdown: write return\n"); return NULL; }

    //sleep(10);

    if(return_value != -1) {
        for(int i = 0; i < MAX_SESSIONS; i++)
            close(s->client_pipe);

        if(pthread_mutex_destroy(&read_lock) < 0) { printf("shutdown: read mutex destroy\n"); return NULL; }
        if(pthread_mutex_destroy(&session_lock) < 0) { printf("shutdown: session mutex destroy\n"); return NULL; }
        close(server_pipe);
        unlink(server_path);
        printf("shutdown success\n");
        return NULL;
    }

    return NULL;    
}

int main(int argc, char **argv) {

    ssize_t n;
    int session_id;
    char op_code;

    if (argc < 2) {
        printf("Please specify the pathname of the server's pipe.\n");
        return 1;
    }

    char *pipename = argv[1];
    printf("Starting TecnicoFS server with pipe called %s\n", pipename);

    /* TO DO */
    if(pthread_mutex_init(&read_lock, NULL)) { printf("main: read mutex init\n"); return -1; }
    if(pthread_mutex_init(&session_lock, NULL)) { printf("main: session mutex init\n"); return -1; }

    for(int i = 0; i < MAX_SESSIONS; i++) {
        sessions[i].id = i;
        sessions[i].client_pipe = -1;
    }

    if(tfs_init() == -1) { printf("main: tfs init\n"); return -1; }

    strcat(server_path, pipename);
    unlink(server_path);
    if(mkfifo(server_path, 0777) < 0) { printf("main: mkfifo\n"); return -1; }
    if((server_pipe = open(server_path, O_RDONLY)) < 0) { printf("main: open server pipe\n"); return -1; }
            

    for(;;) {

        if(pthread_mutex_lock(&read_lock) < 0) { printf("main: read lock\n"); return -1;}
        
        n = read(server_pipe, &op_code, sizeof(char));
        if(n < 0) { printf("main: read opcode\n"); return -1; }
        else if(n == 0) {
            printf("main: n == 0\n");
            close(server_pipe);
            unlink(server_path);
            if(mkfifo(server_path, 0777) < 0) { printf("main: mkfifo\n"); return -1; }
            if((server_pipe = open(server_path, O_RDONLY)) < 0) { printf("main: open server pipe\n"); return -1; }
            if(pthread_mutex_unlock(&read_lock) < 0) { printf("main: read unlock\n"); return -1;}
            continue;
        }

        switch(op_code) {
        case TFS_OP_CODE_MOUNT:
        /*printf("mount\n");*/
            tfs_mount();
            break;

        case TFS_OP_CODE_UNMOUNT:
        /*printf("unmount\n");*/
            n = read(server_pipe, &session_id, sizeof(int));
            if(n <= 0) { printf("unmount: read sessionid\n"); pthread_mutex_unlock(&read_lock); return -1; }
            if(pthread_create(&sessions[session_id].tid, NULL, tfs_unmount, &sessions[session_id]) != 0) exit(1);
            break;

        case TFS_OP_CODE_OPEN:
        /*printf("open\n");*/
            n = read(server_pipe, &session_id, sizeof(int));
            if(n <= 0) { printf("open: read sessionid\n"); pthread_mutex_unlock(&read_lock); return -1; }
            /*printf("session id: %d\n", session_id);*/
            if(pthread_create(&sessions[session_id].tid, NULL, tfs_server_open, &sessions[session_id]) != 0) exit(1);
            break;

        case TFS_OP_CODE_CLOSE:
        /*printf("close\n");*/
            n = read(server_pipe, &session_id, sizeof(int));
            if(n <= 0) { printf("close: read sessionid\n"); pthread_mutex_unlock(&read_lock); return -1; }
            /*printf("session id: %d\n", session_id);*/
            if(pthread_create(&sessions[session_id].tid, NULL, tfs_server_close, &sessions[session_id]) != 0) exit(1);
            break;

        case TFS_OP_CODE_WRITE:
        /*printf("write\n");*/
            n = read(server_pipe, &session_id, sizeof(int));
            if(n <= 0) { printf("write: read sessionid\n"); pthread_mutex_unlock(&read_lock); return -1; }
            /*printf("session id: %d\n", session_id);*/
            if(pthread_create(&sessions[session_id].tid, NULL, tfs_server_write, &sessions[session_id]) != 0) exit(1);
            break;

        case TFS_OP_CODE_READ:
        /*printf("read\n");*/
            n = read(server_pipe, &session_id, sizeof(int));
            if(n <= 0) { printf("read: read sessionid\n"); pthread_mutex_unlock(&read_lock); return -1; }
            /*printf("session id: %d\n", session_id);*/
            if(pthread_create(&sessions[session_id].tid, NULL, tfs_server_read, &sessions[session_id]) != 0) exit(1);
            break;

        case TFS_OP_CODE_SHUTDOWN_AFTER_ALL_CLOSED:
        /*printf("shutdown\n");*/
            n = read(server_pipe, &session_id, sizeof(int));
            if(n <= 0) { printf("shutdown: read sessionid\n"); pthread_mutex_unlock(&read_lock); return -1; }
            /*printf("session id: %d\n", session_id);*/
            if(pthread_create(&sessions[session_id].tid, NULL, tfs_shutdown_after_all_closed, &sessions[session_id]) != 0) exit(1);
            pthread_join(sessions[session_id].tid, NULL);
            exit(0);
            break;
        
        default:
            break;
        }
    }


    return 0;
}