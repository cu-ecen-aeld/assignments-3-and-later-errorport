#define PORT 9000
#define BACKLOG 5
#define BUFFER_SIZE 1024
#define OUTFILE_PATH "/var/tmp/aesdsocketdata"
#define CONN_TIMEOUT_ms 500
#define MAX_EVENTS 10

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <syslog.h>
#include <unistd.h>

volatile sig_atomic_t should_exit = 0;

volatile int serverfd, connfd, epollfd, nfds, fdcounter;
static struct sockaddr_in *serv_addr, *client_addr;
FILE *outfile_ptr;

void catch_function(int signo) {
    if(signo == SIGINT || signo == SIGTERM) {
        should_exit = 1;
        syslog(LOG_INFO, "Caught signal, exiting");
    }
}

int run() {
    int ret = -1;
    struct epoll_event ev, events[MAX_EVENTS];

    if((serv_addr = malloc(sizeof(struct sockaddr_in))) == NULL) {
        syslog(LOG_ERR, "failed to allocate server address: %s", strerror(errno));
        return(ret);
    }
    if((client_addr = malloc(sizeof(struct sockaddr_in))) == NULL) {
        syslog(LOG_ERR, "failed to allocate client address: %s", strerror(errno));
        return(ret);
    }

    memset(serv_addr, 0, sizeof(struct sockaddr_in));
    memset(client_addr, 0, sizeof(struct sockaddr_in));

    socklen_t client_len;
    char client_ip[INET_ADDRSTRLEN];
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read = 0;

    openlog("SocketServer", LOG_PID | LOG_CONS, LOG_INFO);

    if((serverfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        syslog(LOG_ERR, "socket creation failed: %s", strerror(errno));
        return(ret);
    }

    serv_addr->sin_family = AF_INET;
    serv_addr->sin_addr.s_addr = INADDR_ANY;
    serv_addr->sin_port = htons(PORT);

    if(bind(serverfd, (struct sockaddr *)serv_addr, sizeof(struct sockaddr_in)) == -1) {
        syslog(LOG_ERR, "bind failed on port %d: %s", PORT, strerror(errno));
        return(ret);
    }

    if(listen(serverfd, BACKLOG) == -1) {
        syslog(LOG_ERR, "listen failed: %s", strerror(errno));
        return(ret);
    }

    if((epollfd = epoll_create1(0)) == -1) {
        syslog(LOG_ERR, "epoll_create1 failed");
        return(ret);
    }
    ev.events = EPOLLIN;
    ev.data.fd = serverfd;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, serverfd, &ev) == -1) {
        syslog(LOG_ERR, "epoll_ctl: listen_sock: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    client_len = sizeof(client_addr);

    while(!should_exit) {
        nfds = epoll_wait(epollfd, events, MAX_EVENTS, CONN_TIMEOUT_ms);
        switch(nfds) {
            case 0:
            continue;
            case -1:
            syslog(LOG_ERR, "epoll_wait failed: %s", strerror(errno));
            return(ret);
        }

        for (fdcounter = 0; fdcounter < nfds; fdcounter++) {
            if (events[fdcounter].data.fd == serverfd) {
                // New connection request on the listening socket
                connfd = accept(serverfd, (struct sockaddr *)client_addr, &client_len);
                if (connfd == -1) {
                    syslog(LOG_ERR, "accept error: %s", strerror(errno));
                } else {
                    //setnonblocking(connfd);
                    ev.events = EPOLLIN | EPOLLET;
                    ev.data.fd = connfd;
                    // Add new client socket to epoll
                    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, connfd, &ev) == -1) {
                        syslog(LOG_ERR, "epoll_wait failed: %s", strerror(errno));
                        return(ret);
                    }
                    if(inet_ntop(AF_INET, &(client_addr->sin_addr), client_ip, INET_ADDRSTRLEN)) {
                        syslog(LOG_INFO, "Accepted connection from %s", client_ip);
                    } else {
                        continue;
                    }
                }
            }
            int client_fd = events[fdcounter].data.fd;
            ssize_t bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);
            if (bytes_read <= 0) {
                if (bytes_read == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                }
                if (bytes_read == 0) {
                    syslog(
                        LOG_INFO,
                        "Closed connection from %s",
                        client_ip
                    );
                    epoll_ctl(epollfd, EPOLL_CTL_DEL, client_fd, NULL);
                    close(client_fd);
                }
                continue;
            }
            buffer[bytes_read] = '\0';
            printf("Incoming message: %s\n", buffer);
            if((outfile_ptr = fopen(OUTFILE_PATH, "a")) == NULL) {
                syslog(LOG_ERR, "Error opening file");
                return(ret);
            }
            fprintf(outfile_ptr, "%s", buffer);
            fclose(outfile_ptr);

            // Sending back the full file
            if((outfile_ptr = fopen(OUTFILE_PATH, "r")) == NULL) {
                syslog(LOG_ERR, "Error opening file");
                return(ret);
            }
            if (fseek(outfile_ptr, 0, SEEK_END) != 0) {
                syslog(LOG_ERR, "Error seeking to end of file");
                fclose(outfile_ptr);
                return(ret);
            }
            long file_size = ftell(outfile_ptr);
            if (file_size == -1L) {
                syslog(LOG_ERR, "Error getting file size");
                fclose(outfile_ptr);
                return(ret);
            }
            rewind(outfile_ptr);
            char *out_buffer = (char *)malloc(file_size + 1);
            out_buffer[file_size] = '\0';
            bytes_read = fread(out_buffer, 1, file_size, outfile_ptr);
            printf("Return:\n---BEGIN---\n%s\n---END---\n[%ld bytes]\n", out_buffer, bytes_read);
            if (bytes_read > 0) {
                send(connfd, out_buffer, file_size, 0);
            }
            fclose(outfile_ptr);

        }
    }

    ret = 0;
    return ret;
}

int main() {
    signal(SIGINT, catch_function);
    signal(SIGTERM, catch_function);

    int rv = run();

    remove(OUTFILE_PATH);

    close(serverfd);
    close(connfd);
    free(serv_addr);
    free(client_addr);

    closelog();
    return rv;
}

