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
#include <errno.h>

#define SERVER_ADDR "127.0.0.1"
#define SERVER_PORT 9000
#define BUFSIZE 256
#define TMPFILE "/var/tmp/aesdsocketdata"

static int sock;
static int client_conn;
FILE *fp_tmpfile;

void cleanup(void)
{
    close(client_conn);
    close(sock);
    fclose(fp_tmpfile);
    remove(TMPFILE);
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

    int optval = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0)
    {
        syslog(LOG_ERR, "setsockopt: %s", strerror(errno));
        log_and_exit("Failed to set socket options");
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(SERVER_PORT);
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

    printf("Server listening on %s:%d\n", SERVER_ADDR, SERVER_PORT);
    syslog(LOG_INFO, "Listening on %s:%d", SERVER_ADDR, SERVER_PORT);

    fp_tmpfile = fopen(TMPFILE, "a+");
    if (!fp_tmpfile) {
        log_and_exit("Failed to open output file");
    }

    while (1)
    {
        // start accepting new clients until a signal is received
        socklen_t socklen = sizeof addr;
        client_conn = accept(sock, (struct sockaddr *)&addr, &socklen);
        if (client_conn == -1)
        {
            log_and_exit("Failed to accept connection");
        }

        syslog(LOG_INFO, "Accepted connection from %s", inet_ntoa(addr.sin_addr));

        char buf[BUFSIZE];
        while (1)
        {
            ssize_t bytes_received = recv(client_conn, buf, BUFSIZE, 0);
            if (bytes_received < 0)
            {
                syslog(LOG_ERR, "Failed to receive data");
                break;
            }
            else if (bytes_received == 0)
            {
                // finished receiving data from client
                break;
            }

            ssize_t bytes_written = fwrite(buf, 1, bytes_received, fp_tmpfile);
            if (bytes_written < bytes_received) {
                syslog(LOG_ERR, "fwrite: partial write");
                break;
            }

            if (memchr(buf, '\n', bytes_received)) {
                fflush(fp_tmpfile);
                break;
            }
        }

        fflush(fp_tmpfile);
        fseek(fp_tmpfile, 0, SEEK_SET);

        while (!feof(fp_tmpfile))
        {
            size_t bytes_read = fread(buf, 1, BUFSIZE, fp_tmpfile);
            if (ferror(fp_tmpfile))
            {
                syslog(LOG_ERR, "Failed to read from output file");
                break;
            }
            
            size_t bytes_sent = 0;
            while (bytes_sent < bytes_read)
            {
                ssize_t sent_now = send(client_conn, buf+bytes_sent, bytes_read-bytes_sent, 0);
                if (sent_now < 0)                {
                    syslog(LOG_ERR, "Failed to send data to client");
                    break;
                }
                bytes_sent += sent_now;
            }
        }

        close(client_conn);
        rewind(fp_tmpfile);
        syslog(LOG_INFO, "Closed connection from %s", inet_ntoa(addr.sin_addr));
    }

    cleanup();

    return 0;
}