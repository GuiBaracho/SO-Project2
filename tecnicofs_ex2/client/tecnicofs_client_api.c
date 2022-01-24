#include "tecnicofs_client_api.h"

static int server_pipe;
char server_path[MAX_NAME_SIZE] = "/tmp/";
static int client_pipe;
char client_path[MAX_NAME_SIZE] = "/tmp/";
static int session_id;

int tfs_mount(char const *client_pipe_path, char const *server_pipe_path) {
    /* TODO: Implement this */
    ssize_t n;
    char op_code = TFS_OP_CODE_MOUNT;
    
    strcat(server_path, server_pipe_path);
    /*printf("%s\n", server_path);*/

    strcat(client_path, client_pipe_path);
    for(int i = (int)strlen(client_path); i < MAX_NAME_SIZE; i++)
        client_path[i] = '\0';
    /*printf("%s\n", client_path);*/
    unlink(client_path);
    if(mkfifo(client_path, 0777) < 0) exit(1);

    if((server_pipe = open(server_path, O_WRONLY)) < 0) exit(1);
    /*printf("open server\n");*/

    n = write(server_pipe, &op_code, sizeof(char));
    if(n <= 0) return -1;
    /*printf("write opcode: %c\n", op_code);*/

    n = write(server_pipe, client_path, MAX_NAME_SIZE);
    if(n <= 0) return -1;
    /*printf("write name\n");*/

    if((client_pipe = open(client_path, O_RDONLY)) < 0) exit(1);
    /*printf("open client\n");*/
    n = read(client_pipe, &session_id, sizeof(int));
    if(n <= 0) return -1;
    /*printf("%d\n", session_id);*/

    return 0;
}

int tfs_unmount() {
    /* TODO: Implement this */
    ssize_t n;
    int return_value;
    char op_code = TFS_OP_CODE_UNMOUNT;

    n = write(server_pipe, &op_code, sizeof(char));
    if(n <= 0) return -1;
    /*printf("opcode\n");*/

    n = write(server_pipe, &session_id, sizeof(int));
    if(n <= 0) return -1;
    /*printf("sessionid\n");*/

    n = read(client_pipe, &return_value, sizeof(int));
    if(n <= 0) return -1;

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
    char op_code = TFS_OP_CODE_OPEN;
    char filename[MAX_NAME_SIZE];

    strcpy(filename, name);
    for(int i = (int)strlen(filename); i < MAX_NAME_SIZE; i++)
        filename[i] = '\0';

    n = write(server_pipe, &op_code, sizeof(char));
    if(n <= 0) return -1;
    /*printf("opcode\n");*/

    n = write(server_pipe, &session_id, sizeof(int));
    if(n <= 0) return -1;
    /*printf("sessionid\n");*/

    n = write(server_pipe, name, MAX_NAME_SIZE);
    if(n <= 0) return -1;
    /*printf("name\n");*/

    n = write(server_pipe, &flags, sizeof(int));
    if(n <= 0) return -1;
    /*printf("flags %d\n", flags);*/

    n = read(client_pipe, &return_value, sizeof(int));
    if(n <= 0) return -1;

    return return_value;
}

int tfs_close(int fhandle) {
    /* TODO: Implement this */
    ssize_t n;
    int return_value;
    char op_code = TFS_OP_CODE_CLOSE;

    n = write(server_pipe, &op_code, sizeof(char));
    if(n <= 0) return -1;

    n = write(server_pipe, &session_id, sizeof(int));
    if(n <= 0) return -1;

    n = write(server_pipe, &fhandle, sizeof(int));
    if(n <= 0) return -1;

    n = read(client_pipe, &return_value, sizeof(int));
    if(n <= 0) return -1;

    return return_value;
}

ssize_t tfs_write(int fhandle, void const *buffer, size_t length) {
    /* TODO: Implement this */
    ssize_t n, return_value;
    char op_code = TFS_OP_CODE_WRITE;

    n = write(server_pipe, &op_code, sizeof(char));
    if(n <= 0) return -1;

    n = write(server_pipe, &session_id, sizeof(int));
    if(n <= 0) return -1;

    n = write(server_pipe, &fhandle, sizeof(int));
    if(n <= 0) return -1;

    n = write(server_pipe, &length, sizeof(size_t));
    if(n <= 0) return -1;
    
    n = write(server_pipe, buffer, length);
    if(n <= 0) return -1;  

    n = read(client_pipe, &return_value, sizeof(ssize_t));
    if(n <= 0) return -1;

    return return_value;
}

ssize_t tfs_read(int fhandle, void *buffer, size_t length) {
    /* TODO: Implement this */
    ssize_t n, return_value;
    char op_code = TFS_OP_CODE_READ;

    n = write(server_pipe, &op_code, sizeof(char));
    if(n <= 0) return -1;

    n = write(server_pipe, &session_id, sizeof(int));
    if(n <= 0) return -1;

    n = write(server_pipe, &fhandle, sizeof(int));
    if(n <= 0) return -1;

    n = write(server_pipe, &length, sizeof(size_t));
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
    char op_code = TFS_OP_CODE_SHUTDOWN_AFTER_ALL_CLOSED;

    n = write(server_pipe, &op_code, sizeof(char));
    if(n <= 0) return -1;

    n = write(server_pipe, &session_id, sizeof(int));
    if(n <= 0) return -1;

    n = read(client_pipe, &return_value, sizeof(int));
    if(n <= 0) return -1;

    return return_value;
}
