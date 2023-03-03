#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <arpa/inet.h>
#include <sys/un.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/socket.h>
#include <linux/vm_sockets.h>

// HOST is server, GUEST is client

static volatile bool should_run = true;

void sig_int_handler(int signo){
    printf("Recieved SIGINT: %d\n", should_run);
    should_run = false;
}

struct sockaddr_vm create_vm_addr(uint16_t port){
    struct sockaddr_vm vm_addr;
    explicit_bzero(&vm_addr, sizeof(struct sockaddr_vm));

    vm_addr.svm_family = AF_VSOCK;
    vm_addr.svm_port = htons(port);
    vm_addr.svm_cid = VMADDR_CID_ANY;

    return vm_addr;
}

int create_ipc_socket(){

    int sock_fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (sock_fd < 0){
        perror("ipc socket");
        return -1;
    }

    struct sockaddr_un ipc_addr;
    explicit_bzero(&ipc_addr, sizeof(struct sockaddr_un));

    ipc_addr.sun_family = AF_UNIX;
    strcpy(ipc_addr.sun_path, "/tmp/vsock-ipc");

    if (!bind(sock_fd, (struct sockaddr*) &ipc_addr, sizeof(struct sockaddr_un))){
        perror("Host (ipc socket) bind failed");
        close(sock_fd);
        return -1;
    }

    printf("Successfully created IPC socket with FD %d\n", sock_fd);

    return sock_fd;

}

int create_vsocket(uint16_t port){

    int sock_fd = socket(AF_VSOCK, SOCK_DGRAM, 0);
    if (sock_fd < 0){
        perror("vsocket");
        return -1;
    }

    struct sockaddr_vm vm_addr = create_vm_addr(port);

    if (!bind(sock_fd, (struct sockaddr*) &vm_addr, sizeof(struct sockaddr_vm))){
        perror("Host (vsocket) bind failed");
        close(sock_fd);
        return -1;
    }

    printf("Successfully created vsocket with FD %d\n", sock_fd);
    return sock_fd;
}

// TODO: Currently only forwards, doesn't handle return
int run_server(int ipc_sock_fd, int vsock_fd){
    while (should_run){
        uint8_t ipc_data_buf[512];
        //NOTE: Could replace with read
        ssize_t bytes_recv = recv(ipc_sock_fd, ipc_data_buf, 512, 0);
        if (bytes_recv < 0){
            perror("Failed to recieve bytes from IPC socket!");
            return -1;
        }
        if (bytes_recv == 0){
            continue;
        }

        // Recieved some bytes from the IPC socket
        printf("Recieved %ld bytes from IPC socket, forwarding them to Vsock...\n", bytes_recv);
        ssize_t bytes_sent = send(vsock_fd, ipc_data_buf, bytes_recv, 0);
        if (bytes_sent < 0){
            perror("Failed to forward bytes from IPC socket to Vsock!");
            return -1;
        }
        printf("Successfully forwarded %ld bytes to Vsock\n", bytes_sent);

    }
    return 0;
}

int main(int argc, char** argv){
    printf("Hello World\n");

    signal(SIGINT, sig_int_handler);

    int vsocket_fd = create_vsocket(1234);
    if (vsocket_fd < 0) return -1;

    int ipc_socket_fd = create_ipc_socket();
    if (ipc_socket_fd < 0) return -1;

    run_server(ipc_socket_fd, vsocket_fd);

    close(ipc_socket_fd);

    close(vsocket_fd);

    return 0;
}