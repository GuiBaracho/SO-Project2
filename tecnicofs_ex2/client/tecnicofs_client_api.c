#include "tecnicofs_client_api.h"

static int server_pipe;
char server_path[MAX_NAME_SIZE] = "/tmp/";
static int client_pipe;
char client_path[MAX_NAME_SIZE] = "/tmp/";
static int session_id;

int tfs_mount(char const *client_pipe_path, char const *server_pipe_path) {
    /* TODO: Implement this */
    ssize_t n;
    char msg[MAX_NAME_SIZE + 1];
    msg[0] = TFS_OP_CODE_MOUNT;
    
    strcat(server_path, server_pipe_path);
    /*printf("%s\n", server_path);*/

    strcat(client_path, client_pipe_path);
    for(int i = (int)strlen(client_path); i < MAX_NAME_SIZE; i++)
        client_path[i] = '\0';
    /*printf("%s\n", client_path);*/
    unlink(client_path);
    if(mkfifo(client_path, 0777) < 0) return -1;

    if((server_pipe = open(server_path, O_WRONLY)) < 0) return -1;
    /*printf("open server\n");*/

    memcpy(&(msg[1]), client_path, MAX_NAME_SIZE);

    n = write(server_pipe, msg, MAX_NAME_SIZE + 1);
    if(n <= 0) return -1;
    /*printf("write opcode: %c\n", op_code);*/

    if((client_pipe = open(client_path, O_RDONLY)) < 0) return -1;
    /*printf("open client\n");*/
    n = read(client_pipe, &session_id, sizeof(int));
    if(n <= 0) return -1;
    if(session_id == -1){ printf("max session reached\n"); return -1; }
    /*printf("%d\n", session_id);*/

    return 0;
}

int tfs_unmount() {
    /* TODO: Implement this */
    ssize_t n;
    int return_value;
    char msg[1 + sizeof(session_id)];
    msg[0] = TFS_OP_CODE_UNMOUNT;
    
    memcpy(&(msg[1]), &session_id, sizeof(session_id));
    /*printf("start\n");*/

    n = write(server_pipe, msg, sizeof(char) + sizeof(session_id));
    if(n <= 0) return -1;
    /*printf("opcode\n");*/

    n = read(client_pipe, &return_value, sizeof(int));
    if(n <= 0) return -1;
    /*printf("returnValue\n");*/
    close(client_pipe);
    close(server_pipe);
    unlink(client_path);

    return return_value;
}

int tfs_open(char const *name, int flags) {
    /* TODO: Implement this */
    /*printf("\nopen\n");*/
    ssize_t n;
    int return_value;
    char msg[1 + sizeof(session_id) + MAX_NAME_SIZE + sizeof(flags)];
    msg[0] = TFS_OP_CODE_OPEN;
    char filename[MAX_NAME_SIZE];

    strcpy(filename, name);
    for(int i = (int)strlen(filename); i < MAX_NAME_SIZE; i++)
        filename[i] = '\0';

    memcpy(&(msg[1]), &session_id, sizeof(session_id));
    memcpy(&(msg[1 + sizeof(session_id)]), filename, MAX_NAME_SIZE);
    memcpy(&(msg[1 + sizeof(session_id)]) + MAX_NAME_SIZE, &flags, sizeof(flags));

    n = write(server_pipe, msg, 1 + sizeof(session_id) + MAX_NAME_SIZE + sizeof(flags));
    if(n <= 0) return -1;   

    n = read(client_pipe, &return_value, sizeof(int));
    if(n <= 0) return -1;

    return return_value;
}

int tfs_close(int fhandle) {
    /* TODO: Implement this */
    ssize_t n;
    int return_value;
    char msg[1 + sizeof(session_id) + sizeof(fhandle)];
    msg[0] = TFS_OP_CODE_CLOSE;

    memcpy(&(msg[1]), &session_id, sizeof(session_id));
    memcpy(&(msg[1 + sizeof(session_id)]), &fhandle, sizeof(fhandle));

    n = write(server_pipe, msg, 1 + sizeof(session_id) + sizeof(fhandle));
    if(n <= 0) return -1;

    n = read(client_pipe, &return_value, sizeof(int));
    if(n <= 0) return -1;

    return return_value;
}

ssize_t tfs_write(int fhandle, void const *buffer, size_t length) {
    /* TODO: Implement this */
    ssize_t n, return_value;
    char msg[1 + sizeof(session_id) + sizeof(fhandle) + sizeof(length) + length];
    msg[0] = TFS_OP_CODE_WRITE;

    memcpy(&(msg[1]), &session_id, sizeof(session_id));
    memcpy(&(msg[1 + sizeof(session_id)]), &fhandle, sizeof(fhandle));
    memcpy(&(msg[1 + sizeof(session_id) + sizeof(fhandle)]), &length, sizeof(length));
    memcpy(&(msg[1 + sizeof(session_id) + sizeof(fhandle) + sizeof(length)]), buffer, length);

    n = write(server_pipe, msg, 1 + sizeof(session_id) + sizeof(fhandle) + sizeof(length) + length);
    if(n <= 0) return -1;

    n = read(client_pipe, &return_value, sizeof(ssize_t));
    if(n <= 0) return -1;

    return return_value;
}

ssize_t tfs_read(int fhandle, void *buffer, size_t length) {
    /* TODO: Implement this */
    ssize_t n, return_value;
    char msg[1 + sizeof(session_id) + sizeof(fhandle) + sizeof(length)];
    msg[0] = TFS_OP_CODE_READ;

    memcpy(&(msg[1]), &session_id, sizeof(session_id));
    memcpy(&(msg[1 + sizeof(session_id)]), &fhandle, sizeof(fhandle));
    memcpy(&(msg[1 + sizeof(session_id) + sizeof(fhandle)]), &length, sizeof(length));

    n = write(server_pipe, msg, 1 + sizeof(session_id) + sizeof(fhandle) + sizeof(length));
    if(n <= 0) return -1;

    n = read(client_pipe, &return_value, sizeof(ssize_t));
    if(n <= 0) return -1;

    if(return_value != -1) {
        n = read(client_pipe, buffer, length);
        if(n <= 0) return -1;
    }

    return return_value;
}

int tfs_shutdown_after_all_closed() {
    /* TODO: Implement this */
    ssize_t n;
    int return_value;
    char msg[1 + sizeof(session_id)];
    msg[0] = TFS_OP_CODE_SHUTDOWN_AFTER_ALL_CLOSED;
    
    memcpy(&(msg[1]), &session_id, sizeof(session_id));

    n = write(server_pipe, msg, 1 + sizeof(session_id));
    if(n <= 0) return -1;

    n = read(client_pipe, &return_value, sizeof(int));
    if(n <= 0) return -1;

    close(server_pipe);

    return return_value;
}
