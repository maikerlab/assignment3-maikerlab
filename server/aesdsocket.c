#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <syslog.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>

#define SOCK_ADDRESS "127.0.0.1"
#define SOCK_PORT 9000
#define BUFSIZE 512
#define SOCKET_DATA_FILE "/var/tmp/aesdsocketdata"

static int sock;
static int client_sock;
static int output_file;

void cleanup(void)
{
    close(client_sock);
    close(sock);
    close(output_file);
    remove(SOCKET_DATA_FILE);
    closelog();
}

static void my_sig_handler(int signo)
{
    if (signo == SIGINT || signo == SIGTERM)
    {
        syslog(LOG_INFO, "Caught signal, exiting");
        cleanup();
        exit(0);
    }
}

void log_and_exit(const char *msg)
{
    printf("%s\n", msg);
    syslog(LOG_ERR, "%s", msg);
    cleanup();
    exit(-1);
}

void daemonize(void)
{
    pid_t pid = fork();
    if (pid < 0)
    {
        log_and_exit("Failed to fork");
    }
    if (pid > 0)
    {
        exit(0); // parent exits
    }

    // child continues
    if (setsid() < 0)
    {
        log_and_exit("Failed to create new session");
    }
}

int main(int argc, char *argv[])
{
    openlog("aesdsocket", LOG_PID, LOG_USER);

    bool run_as_daemon = false;
    if (argc > 1 && strcmp(argv[1], "-d") == 0)
    {
        run_as_daemon = true;
    }

    struct sigaction sa;
    memset(&sa, 0, sizeof(struct sigaction));
    sa.sa_handler = &my_sig_handler;
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        log_and_exit("Failed to create socket");
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(SOCK_PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        log_and_exit("Failed to bind socket");
    }

    if (run_as_daemon)
        daemonize();

    if (listen(sock, 5) < 0)
    {
        log_and_exit("Failed to listen on socket");
    }

    printf("Server listening on %s:%d\n", SOCK_ADDRESS, SOCK_PORT);
    syslog(LOG_INFO, "Listening on %s:%d", SOCK_ADDRESS, SOCK_PORT);

    output_file = open(SOCKET_DATA_FILE, O_RDWR | O_CREAT | O_APPEND, 0644);
    if (output_file < 0)
    {
        log_and_exit("Failed to open output file");
    }

    while (1)
    {
        // start accepting new clients until a signal is received
        socklen_t socklen = sizeof addr;
        client_sock = accept(sock, (struct sockaddr *)&addr, &socklen);
        if (client_sock == -1)
        {
            log_and_exit("Failed to accept connection");
        }

        syslog(LOG_INFO, "Accepted connection from %s", inet_ntoa(addr.sin_addr));

        char buf[BUFSIZE];
        size_t data_len = 0;
        bool receiving = true;
        while (receiving)
        {
            ssize_t bytes_received = recv(client_sock, buf, BUFSIZE, 0);
            if (bytes_received < 0)
            {
                syslog(LOG_ERR, "Failed to receive data");
                close(client_sock);
                receiving = false;
            }
            else if (bytes_received == 0)
            {
                receiving = false;
            }
            else
            {
                syslog(LOG_INFO, "Received %zd bytes of data", bytes_received);
                data_len += bytes_received;

                char *newline = memchr(buf, '\n', data_len);
                if (newline != NULL)
                {
                    size_t line_len = newline - buf + 1;
                    ssize_t bytes_written = write(output_file, buf, line_len);
                    if (bytes_written < 0)
                    {
                        syslog(LOG_ERR, "Failed to write to output file");
                        cleanup();
                        return -1;
                    }

                    memmove(buf, buf + line_len, data_len - line_len);
                    data_len -= line_len;
                }

                if (data_len == BUFSIZE)
                {
                    syslog(LOG_ERR, "Line too long!");
                    data_len = 0; // reset buffer
                }
            }
        }

        syslog(LOG_INFO, "Sending back file contents to client");
        lseek(output_file, 0, SEEK_SET);
        ssize_t n_bytes;
        memset(buf, 0, BUFSIZE);
        while ((n_bytes = read(output_file, buf, BUFSIZE)) > 0)
        {
            syslog(LOG_INFO, "Sending '%.*s' (%zd  bytes) back to client", (int)n_bytes, buf, n_bytes);
            ssize_t bytes_sent = send(client_sock, buf, n_bytes, 0);
            if (bytes_sent < 0)
            {
                syslog(LOG_ERR, "Failed to send data to client");
                break;
            }
        }
        syslog(LOG_INFO, "Closed connection from %s", inet_ntoa(addr.sin_addr));
        close(client_sock);
    }

    cleanup();

    return 0;
}