#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pthread.h>
#include "operations.h"

#define MAX_SESSIONS 50

typedef struct {

    int id;
    char* msg;
    int client_pipe;
    pthread_t tid;
    pthread_mutex_t thread_lock;
    pthread_cond_t canRead;
    
} thread;

static int free_threads[MAX_SESSIONS];
static thread tfs_threads[MAX_SESSIONS];
static pthread_mutex_t free_threads_lock;

static int server_pipe;
char server_path[MAX_NAME_SIZE] = "/tmp/";

int tfs_mount() {
    /*printf("mount\n");*/
    ssize_t n;
    int client_pipe, session_id = -1;
    char pipename[MAX_NAME_SIZE];

    n = read(server_pipe, pipename, 40);
    if(n <= 0) { printf("mount: read pipename\n"); return -1; }
    /*printf("%s\n", pipename);*/

    if(pthread_mutex_lock(&free_threads_lock) != 0) { printf("mount: lock error\n");  return -1; }
    /*printf("mount: inside lock\n");*/
    for(int i = 0; i < MAX_SESSIONS; i++) {
        if(free_threads[i] == -1) {
            free_threads[i] = 0;
            session_id = i;
            break;
        }
    }

    if(pthread_mutex_unlock(&(free_threads_lock)) != 0) return -1;

    /*printf("session id mount (%d)\n", session_id);*/

    if(session_id == -1) {
        if((client_pipe = open(pipename, O_WRONLY)) < 0) { printf("mount: open client pipe(%d)\n", client_pipe); return -1; }
        n = write(client_pipe, &session_id, sizeof(int));
        if(n <= 0) { printf("mount: write sessionid\n"); return -1; }
        /*printf("write sessionid\n");*/

    } else {
        char* msg_buff = malloc(1+MAX_NAME_SIZE);
        msg_buff[0] = TFS_OP_CODE_MOUNT;
        memcpy(&(msg_buff[1]), pipename, MAX_NAME_SIZE);
        /*printf("msg: %s\n", msg_buff);*/
        if(pthread_mutex_lock(&(tfs_threads[session_id].thread_lock)) != 0) return -1;
        tfs_threads[session_id].msg = msg_buff;
        if(pthread_mutex_unlock(&(tfs_threads[session_id].thread_lock)) != 0) return -1;
        pthread_cond_signal(&(tfs_threads[session_id].canRead));
    }

    /*printf("mount end\n");*/
    
    return 0;
}

int tfs_mount_aux(thread* t) {
    /*printf("mount aux\n");*/
    ssize_t n;
    char pipename[MAX_NAME_SIZE];

    memcpy(pipename, &(t->msg[1]), MAX_NAME_SIZE);

    if((t->client_pipe = open(pipename, O_WRONLY)) < 0) { printf("mount aux: open client pipe(%d)\n", t->client_pipe); return -1; }

    n = write(t->client_pipe, &(t->id), sizeof(int));
    if(n <= 0) { printf("mount: write sessionid\n"); return -1; }
    /*printf("write sessionid\n");*/
    return 0;
}

int tfs_unmount(thread* t) {
    /*printf("unmount\n");*/
    ssize_t n;
    int return_value = 0;
    
    n = write(t->client_pipe, &return_value, sizeof(int));
    if(n <= 0) { printf("unmount: return(%d)\n", t->client_pipe); return -1; }

    close(t->client_pipe);

    if(pthread_mutex_lock(&(free_threads_lock)) != 0) return -1;

    free_threads[t->id] = -1;

    if(pthread_mutex_unlock(&(free_threads_lock)) != 0) return -1;

    return 0;
}

int tfs_server_open(char* msg, int client_pipe) {
    /*printf("tfs_open\n");*/
    ssize_t n;
    int flags, return_value = 0;
    char filename[MAX_NAME_SIZE];

    memcpy(filename, &(msg[1+sizeof(int)]), MAX_NAME_SIZE);
    /*printf("file name: %s\n", filename);*/

    memcpy(&flags, &(msg[1+sizeof(int)+MAX_NAME_SIZE]), sizeof(int));
    /*printf("flags: %d\n", flags);*/

    return_value = tfs_open(filename, flags);
    /*printf("open fhandle: %d\n", return_value);*/

    n = write(client_pipe, &return_value, sizeof(int));
    if(n <= 0) { printf("open: write return\n"); return -1; }

    return 0;
}

int tfs_server_close(char* msg, int client_pipe) {
    /*printf("close\n");*/
    ssize_t n;
    int fhandle, return_value = 0;

    memcpy(&fhandle, &(msg[1+sizeof(int)]), sizeof(int));
    /*printf("fhandle: %d\n", fhandle);*/

    return_value = tfs_close(fhandle);

    n = write(client_pipe, &return_value, sizeof(int));
    if(n <= 0) { printf("close: write return\n"); return -1; }

    return 0;
}

int tfs_server_write(char* msg, int client_pipe) {
    /*printf("write\n");*/
    ssize_t n, return_value = 0;
    size_t length;
    int fhandle;

    memcpy(&fhandle, &(msg[1+sizeof(int)]), sizeof(int));
    /*printf("fhandle: %d\n", fhandle);*/

    memcpy(&length, &(msg[1+sizeof(int)+sizeof(int)]), sizeof(size_t));
    /*printf("length: %ld\n", length);*/

    char *buffer = (char*)malloc(sizeof(char)*length);
    memcpy(buffer, &(msg[1+sizeof(int)+sizeof(int)+sizeof(size_t)]), length);
    /*printf("buffer: %s\n", buffer);*/

    return_value = tfs_write(fhandle, buffer, length);

    n = write(client_pipe, &return_value, sizeof(ssize_t));
    if(n <= 0) { printf("write: write return\n"); return -1; }

    free(buffer);

    return 0;
}

int tfs_server_read(char* msg, int client_pipe) {
    /*printf("thread read\n");*/
    ssize_t n, return_value = 0;
    size_t length;
    int fhandle;

    memcpy(&fhandle, &(msg[1+sizeof(int)]), sizeof(int));
    /*printf("fhandle: %d\n", fhandle);*/

    memcpy(&length, &(msg[1+sizeof(int)+sizeof(int)]), sizeof(size_t));
    /*printf("length: %ld\n", length);*/

    char *buffer = (char*)malloc(sizeof(char)*length);
    return_value = tfs_read(fhandle, buffer, length);
    /*printf("amount read: %ld\n", return_value);*/

    n = write(client_pipe, &return_value, sizeof(ssize_t));
    if(n <= 0) { printf("read: write return\n"); return -1; }

    if(return_value != -1) {
        n = write(client_pipe, buffer, (size_t)return_value);
        if(n <= 0) { printf("read: write buffer\n"); return -1; }
    }

    free(buffer);

    return 0;
}

int tfs_shutdown_after_all_closed(char* msg, int client_pipe) {
    /*printf("shutdown\n");*/
    ssize_t n;
    int return_value = 0;

    return_value = tfs_destroy_after_all_closed();

    n = write(client_pipe, &return_value, sizeof(int));
    if(n <= 0) { printf("shutdown: write return\n"); return -1; }

    if(return_value == -1) return -1;

    for(int i = 0; i < MAX_SESSIONS; i++) {
        if(pthread_mutex_destroy(&(tfs_threads[i].thread_lock)) != 0) exit(1);
    }

    if(pthread_mutex_destroy(&free_threads_lock) != 0) exit(1);

    free(msg);
    close(client_pipe);
    close(server_pipe);
    unlink(server_path);
    printf("shutdown success\n");
    exit(1);
        
}

void* thread_main(void* arg) {
    thread* t = (thread*)arg;
    /*printf("thread_main(%d)\n", t->id);*/
    /*printf(t->msg == NULL ? "outside true\n" : "outside false\n");*/

    while(1) {
        
        if(pthread_mutex_lock(&(t->thread_lock)) != 0) return NULL;
        /*printf(t->msg == NULL ? "intside true\n" : "inside false\n");*/
        while(t->msg == NULL) {
            pthread_cond_wait(&(t->canRead), &(t->thread_lock));
        }

        /*printf(t->msg[0] == TFS_OP_CODE_MOUNT ? "true\n" : "false\n");*/
        switch(t->msg[0]) {
        case TFS_OP_CODE_MOUNT:
        /*printf("thread mount\n");*/
            tfs_mount_aux(t);
            break;

        case TFS_OP_CODE_UNMOUNT:
        /*printf("unmount\n");*/
            tfs_unmount(t);
            break;

        case TFS_OP_CODE_OPEN:
        /*printf("open\n");*/
            tfs_server_open(t->msg, t->client_pipe);
            break;

        case TFS_OP_CODE_CLOSE:
        /*printf("close\n");*/
            tfs_server_close(t->msg, t->client_pipe);
            break;

        case TFS_OP_CODE_WRITE:
        /*printf("write\n");*/
            tfs_server_write(t->msg, t->client_pipe);
            break;

        case TFS_OP_CODE_READ:
        /*printf("read\n");*/
            tfs_server_read(t->msg, t->client_pipe);
            break;

        case TFS_OP_CODE_SHUTDOWN_AFTER_ALL_CLOSED:
        /*printf("shutdown\n");*/
            tfs_shutdown_after_all_closed(t->msg, t->client_pipe);
            break;
        
        default:
            break;
        }


        /*printf("thread_main: inside lock\n");*/
        

        free(t->msg);
        t->msg = NULL;

        if(pthread_mutex_unlock(&(t->thread_lock)) != 0) return NULL;
    }

    return NULL;
}

int main(int argc, char **argv) {

    ssize_t n;
    int id;
    char op_code;
    char msg[1041];
    char* msg_buff;

    if (argc < 2) {
        printf("Please specify the pathname of the server's pipe.\n");
        return 1;
    }

    char *pipename = argv[1];
    printf("Starting TecnicoFS server with pipe called %s\n", pipename);

    /* TO DO */

    if(tfs_init() == -1) { printf("main: tfs init\n"); return -1; }

    for(int i = 0; i < MAX_SESSIONS; i++) {
        free_threads[i] = -1;
        tfs_threads[i].id = i;
        tfs_threads[i].msg = NULL;
        if(pthread_mutex_init(&(tfs_threads[i].thread_lock), NULL) != 0) { printf("thread mutex init\n"); exit(1); }
        if(pthread_create(&(tfs_threads[i].tid), 0, thread_main, &(tfs_threads[i])) != 0) { printf("thread create\n"); exit(1); }
    }

    if(pthread_mutex_init(&(free_threads_lock), NULL) != 0) { printf("free threads mutex init\n"); exit(1); }

    strcat(server_path, pipename);
    unlink(server_path);
    if(mkfifo(server_path, 0777) < 0) { printf("main: mkfifo\n"); return -1; }
    if((server_pipe = open(server_path, O_RDONLY)) < 0) { printf("main: open server pipe\n"); return -1; }
            

    for(;;) {
        
        n = read(server_pipe, &op_code, sizeof(char));
        if(n < 0) { printf("main: read opcode\n"); return -1; }
        else if(n == 0) {
            /*printf("main: n == 0\n");*/
            close(server_pipe);
            unlink(server_path);
            if(mkfifo(server_path, 0777) < 0) { printf("main: mkfifo\n"); return -1; }
            if((server_pipe = open(server_path, O_RDONLY)) < 0) { printf("main: open server pipe\n"); return -1; }
            continue;
        }

        switch(op_code) {
        case TFS_OP_CODE_MOUNT:
        /*printf("mount\n");*/
            tfs_mount();
            break;

        case TFS_OP_CODE_UNMOUNT:
        /*printf("unmount\n");*/
            msg[0] = TFS_OP_CODE_UNMOUNT;
            n = read(server_pipe, &(msg[1]), sizeof(int));
            if(n <= 0) { printf("open: unmount error\n"); return -1; }

            msg_buff = malloc(1 + sizeof(int));
            memcpy(msg_buff, msg, 1 + sizeof(int));

            memcpy(&id, &(msg_buff[1]), sizeof(int));
        /*printf("open: id(%d)\n", id);*/
            if(pthread_mutex_lock(&(tfs_threads[id].thread_lock)) != 0) return -1;
        /*printf("open: inside lock\n");*/
            tfs_threads[id].msg = msg_buff;
            if(pthread_mutex_unlock(&(tfs_threads[id].thread_lock)) != 0) return -1;
            pthread_cond_signal(&(tfs_threads[id].canRead));

            break;

        case TFS_OP_CODE_OPEN:
        /*printf("open\n");*/
            msg[0] = TFS_OP_CODE_OPEN;
            n = read(server_pipe, &(msg[1]), sizeof(int) + MAX_NAME_SIZE + sizeof(int));
            if(n <= 0) { printf("open: open error\n"); return -1; }

            msg_buff = malloc(1 + sizeof(int) + MAX_NAME_SIZE + sizeof(int));
            memcpy(msg_buff, msg, 1 + sizeof(int) + MAX_NAME_SIZE + sizeof(int));

            memcpy(&id, &(msg_buff[1]), sizeof(int));
        /*printf("open: id(%d)\n", id);*/
            if(pthread_mutex_lock(&(tfs_threads[id].thread_lock)) != 0) return -1;
        /*printf("open: inside lock\n");*/
            tfs_threads[id].msg = msg_buff;
            if(pthread_mutex_unlock(&(tfs_threads[id].thread_lock)) != 0) return -1;
            pthread_cond_signal(&(tfs_threads[id].canRead));

            break;

        case TFS_OP_CODE_CLOSE:
        /*printf("close\n");*/
            msg[0] = TFS_OP_CODE_CLOSE;
            n = read(server_pipe, &(msg[1]), sizeof(int) + sizeof(int));
            if(n <= 0) { printf("open: close error\n"); return -1; }

            msg_buff = malloc(1 + sizeof(int) + sizeof(int));
            memcpy(msg_buff, msg, 1 + sizeof(int) + sizeof(int));

            memcpy(&id, &(msg_buff[1]), sizeof(int));
        /*printf("open: id(%d)\n", id);*/
            if(pthread_mutex_lock(&(tfs_threads[id].thread_lock)) != 0) return -1;
        /*printf("open: inside lock\n");*/
            tfs_threads[id].msg = msg_buff;
            if(pthread_mutex_unlock(&(tfs_threads[id].thread_lock)) != 0) return -1;
            pthread_cond_signal(&(tfs_threads[id].canRead));

            break;

        case TFS_OP_CODE_WRITE:
        /*printf("write\n");*/
            msg[0] = TFS_OP_CODE_WRITE;
            n = read(server_pipe, &(msg[1]), sizeof(int) + sizeof(int) + sizeof(size_t) + BLOCK_SIZE);
            if(n <= 0) { printf("open: write error\n"); return -1; }

            msg_buff = malloc(1 + sizeof(int) + sizeof(int) + sizeof(size_t) + BLOCK_SIZE);
            memcpy(msg_buff, msg, 1 + sizeof(int) + sizeof(int) + sizeof(size_t) + BLOCK_SIZE);

            memcpy(&id, &(msg_buff[1]), sizeof(int));
        /*printf("open: id(%d)\n", id);*/
            if(pthread_mutex_lock(&(tfs_threads[id].thread_lock)) != 0) return -1;
        /*printf("open: inside lock\n");*/
            tfs_threads[id].msg = msg_buff;
            if(pthread_mutex_unlock(&(tfs_threads[id].thread_lock)) != 0) return -1;
            pthread_cond_signal(&(tfs_threads[id].canRead));

            break;

        case TFS_OP_CODE_READ:
        /*printf("read\n");*/
            msg[0] = TFS_OP_CODE_READ;
            n = read(server_pipe, &(msg[1]), sizeof(int) + sizeof(int) + sizeof(size_t));
            if(n <= 0) { printf("open: read error\n"); return -1; }

            msg_buff = malloc(1 + sizeof(int) + sizeof(int) + sizeof(size_t));
            memcpy(msg_buff, msg, 1 + sizeof(int) + sizeof(int) + sizeof(size_t));

            memcpy(&id, &(msg_buff[1]), sizeof(int));
        /*printf("open: id(%d)\n", id);*/
            if(pthread_mutex_lock(&(tfs_threads[id].thread_lock)) != 0) return -1;
        /*printf("open: inside lock\n");*/
            tfs_threads[id].msg = msg_buff;
            if(pthread_mutex_unlock(&(tfs_threads[id].thread_lock)) != 0) return -1;
            pthread_cond_signal(&(tfs_threads[id].canRead));

            break;

        case TFS_OP_CODE_SHUTDOWN_AFTER_ALL_CLOSED:
        /*printf("shutdown\n");*/
            msg[0] = TFS_OP_CODE_SHUTDOWN_AFTER_ALL_CLOSED;
            n = read(server_pipe, &(msg[1]), sizeof(int));
            if(n <= 0) { printf("open: shutdown error\n"); return -1; }

            msg_buff = malloc(1 + sizeof(int));
            memcpy(msg_buff, msg, 1 + sizeof(int));

            memcpy(&id, &(msg_buff[1]), sizeof(int));
        /*printf("open: id(%d)\n", id);*/
            if(pthread_mutex_lock(&(tfs_threads[id].thread_lock)) != 0) return -1;
        /*printf("open: inside lock\n");*/
            tfs_threads[id].msg = msg_buff;
            if(pthread_mutex_unlock(&(tfs_threads[id].thread_lock)) != 0) return -1;
            pthread_cond_signal(&(tfs_threads[id].canRead));

            break;
        
        default:
            break;
        }
    }


    return 0;
}