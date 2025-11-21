#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/stat.h>

#define PORT 8080
#define FILE_TO_SERVE "index.html"

typedef struct {
    int client_fd;
} client_args_t;

void send_file(int client_fd, const char *filename) {
    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        const char *msg =
            "HTTP/1.1 404 Not Found\r\n"
            "Content-Length: 0\r\n"
            "Connection: close\r\n\r\n";
        write(client_fd, msg, strlen(msg));
        return;
    }

    struct stat st;
    stat(filename, &st);
    size_t filesize = st.st_size;

    char header[256];
    int header_len = snprintf(header, sizeof(header),
                              "HTTP/1.1 200 OK\r\n"
                              "Content-Type: text/html\r\n"
                              "Content-Length: %zu\r\n"
                              "Connection: close\r\n\r\n",
                              filesize);

    write(client_fd, header, header_len);

    char buf[4096];
    size_t bytes;
    while ((bytes = fread(buf, 1, sizeof(buf), fp)) > 0) {
        write(client_fd, buf, bytes);
    }

    fclose(fp);
}

void* handle_client(void* arg) {
    client_args_t *args = (client_args_t*)arg;
    int client_fd = args->client_fd;
    free(args);

    char reqbuf[2048];
    read(client_fd, reqbuf, sizeof(reqbuf) - 1);  // discard request data

    printf("handling request\n");

    send_file(client_fd, FILE_TO_SERVE);

    close(client_fd);
    return NULL;
}

int main() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket failed");
        exit(1);
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind failed");
        exit(1);
    }

    if (listen(server_fd, 32) < 0) {
        perror("listen failed");
        exit(1);
    }

    printf("Multithreaded HTTP server listening on port %d...\n", PORT);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t clen = sizeof(client_addr);

        int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &clen);
        if (client_fd < 0) {
            perror("accept failed");
            continue;
        }

        pthread_t tid;
        client_args_t *args = malloc(sizeof(client_args_t));
        args->client_fd = client_fd;

        pthread_create(&tid, NULL, handle_client, args);
        pthread_detach(tid);  // auto-clean thread
    }

    close(server_fd);
    return 0;
}
