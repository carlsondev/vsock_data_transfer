import socket


def main():
    ipc_path = "/tmp/vsock-ipc"

    ipc_sock = socket.socket(socket.AF_UNIX, socket.SOCK_DGRAM)
    ipc_sock.connect(ipc_path)

    ipc_sock.send(bytes("Hello World", 'utf8'))

    ipc_sock.close()

if __name__ == "__main__":
    main()