#define PORT 9000
#define BACKLOG 5
#define BUFFER_SIZE 1<<16
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
#include <sys/stat.h>
#include <syslog.h>
#include <unistd.h>

volatile sig_atomic_t should_exit = 0;

volatile int serverfd, connfd, epollfd, nfds, fdcounter;
static struct sockaddr_in *serv_addr, *client_addr;
FILE *outfile_ptr;

volatile int daemon_mode = 0;

void catch_function(int signo) {
    if(signo == SIGINT || signo == SIGTERM) {
        should_exit = 1;
        syslog(LOG_INFO, "Caught signal, exiting");
    }
}

void start_daemon() {
    pid_t pid;
    pid = fork();
    if (pid < 0) exit(EXIT_FAILURE);
    if (pid > 0) exit(EXIT_SUCCESS);
    if (setsid() < 0) exit(EXIT_FAILURE);
    signal(SIGCHLD, SIG_IGN);
    signal(SIGHUP, SIG_IGN);
    pid = fork();
    if (pid < 0) exit(EXIT_FAILURE);
    if (pid > 0) exit(EXIT_SUCCESS);
    umask(0);
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

    struct linger so_linger;
    so_linger.l_onoff = 1;
    so_linger.l_linger = 0;

    if((serverfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        syslog(LOG_ERR, "socket creation failed: %s", strerror(errno));
        return(ret);
    }

    setsockopt(serverfd, SOL_SOCKET, SO_LINGER, &so_linger, sizeof(so_linger));

    serv_addr->sin_family = AF_INET;
    serv_addr->sin_addr.s_addr = INADDR_ANY;
    serv_addr->sin_port = htons(PORT);

    if(bind(serverfd, (struct sockaddr *)serv_addr, sizeof(struct sockaddr_in)) < 0) {
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
        return(ret);
    }

    if(daemon_mode) {
        start_daemon();
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
                }
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
                }
                continue;
            }

            int client_fd = events[fdcounter].data.fd;

            if((outfile_ptr = fopen(OUTFILE_PATH, "a")) == NULL) {
                syslog(LOG_ERR, "Error opening file: %s", strerror(errno));
                return(ret);
            }

            do {
                ssize_t bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);
                buffer[bytes_read] = '\0';
                printf("Incoming message: %s\n", buffer);
                syslog(LOG_INFO, ">> %s", buffer);
                fprintf(outfile_ptr, "%s", buffer);
            } while(bytes_read > 0);

            fflush(outfile_ptr);
            fsync(fileno(outfile_ptr));
            fclose(outfile_ptr);

            // Sending back the full file
            if((outfile_ptr = fopen(OUTFILE_PATH, "r")) == NULL) {
                continue;
                // syslog(LOG_ERR, "Error opening file");
                // return(ret);
            }
            do {
                bytes_read = fread(buffer, 1, BUFFER_SIZE, outfile_ptr);
                printf("sandback: %ld\n", bytes_read);
                if (bytes_read > 0 /*&& client_fd >= 0*/) {
                    // send(connfd, out_buffer, file_size, 0);
                    if(send(client_fd, buffer, bytes_read, 0) < 0) {
                        syslog(LOG_ERR, "Failed to send back message: %s", strerror(errno));
                    }
                }
            } while (bytes_read > 0);
            fclose(outfile_ptr);
            syslog(
                LOG_INFO,
                "Closed connection from %s",
                client_ip
            );
            epoll_ctl(epollfd, EPOLL_CTL_DEL, client_fd, NULL);
            close(client_fd);
        }
    }

    ret = 0;
    return ret;
}

int main(int args, char** argv) {
    signal(SIGINT, catch_function);
    signal(SIGTERM, catch_function);

    if(args > 1) {
        if(!strcmp(argv[1], "-d")) {
            daemon_mode = 1;
        }
    }

    int rv = run();

    remove(OUTFILE_PATH);

    close(serverfd);
    close(connfd);
    close(epollfd);
    free(serv_addr);
    free(client_addr);

    closelog();
    return rv;
}

