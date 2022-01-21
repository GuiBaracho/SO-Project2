#include "tecnicofs_client_api.h"

int tfs_mount(char const *client_pipe_path, char const *server_pipe_path) {
    /* TODO: Implement this */
    int server_pipe, n;
    char server_path[MAX_NAME_SIZE] = "/tmp/";
    char buff[20];

    strcat(server_path, server_pipe_path);
    printf("%s\n", server_path);
    if((server_pipe = open(server_path, O_WRONLY)) < 0) exit(1);

    strcpy(buff, client_pipe_path);
    n = write(server_pipe, buff, 5);
    printf("%d\n", n);
    if(n <= 0) return -1;
    
    return 0;
}

int tfs_unmount() {
    /* TODO: Implement this */
    return -1;
}

int tfs_open(char const *name, int flags) {
    /* TODO: Implement this */
    return -1;
}

int tfs_close(int fhandle) {
    /* TODO: Implement this */
    return -1;
}

ssize_t tfs_write(int fhandle, void const *buffer, size_t len) {
    /* TODO: Implement this */
    return -1;
}

ssize_t tfs_read(int fhandle, void *buffer, size_t len) {
    /* TODO: Implement this */
    return -1;
}

int tfs_shutdown_after_all_closed() {
    /* TODO: Implement this */
    return -1;
}
