#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>

#define BUFFER_SIZE 512
#define MAX_THREADS 128

typedef struct clientData_t 
{
    int newSockFd;
    int clientLen;
    struct sockaddr_in clientAddr;
} clientData_t;

static const char* version = "1.0.0";
static volatile sig_atomic_t stop = 0;

static int serverSockFd = 0;

static pthread_t timestampThread = 0;
static pthread_t clientThreads[MAX_THREADS] = {0};
static int threadCount = 0;

static bool runAsDaemon = false;
static bool timestampThreadStarted = false;

int pipeClientHandler[2]; // [0] for reading, [1] for writing
int pipeTimestampWriterHandler[2]; // [0] for reading, [1] for writing


static pthread_mutex_t fileMutex = PTHREAD_MUTEX_INITIALIZER;

void    logAndExit(const char *msg, const char *filename, int exit_code);
char**  bufferPacketCreate(void);
void    bufferPacketDelete(char **bufferPacket);
void    bufferPacketFree(char **bufferPacket, uint8_t* bufferPacketIndex);
void    cleanupClientHandler(clientData_t* clientData, char** bufferPacket, uint8_t* bufferPacketIndex);
void    cleanupMain(void);
bool    checkForNullCharInString(const char *str, const ssize_t len);
bool    checkForNewlineCharInString(const char *str, const ssize_t len, ssize_t *newlinePosition);
void    SIGINTHandler(int signum, siginfo_t *info, void *extra);
void    SIGTERMHandler(int signum, siginfo_t *info, void *extra);
void    setSignalSIGINTHandler(void);
void    setSignalSIGTERMHandler(void);
void*   timestampWriterHandler(void* arg);
void*   clientHandler(void* arg);

void logAndExit(const char *msg, const char *filename, int exit_code) 
{
    char msgBuffer[256] = {0};
    if (strlen(msg) >= sizeof(msgBuffer) - 64) 
    {
        exit(EXIT_FAILURE);
    }
    snprintf(msgBuffer, sizeof(msgBuffer), "errno: %d msg: %s file: %s", errno, msg, filename ? filename : "unknown");
    syslog(LOG_ERR, "%s", msgBuffer);

    perror(msgBuffer);
    cleanupMain();
    exit(exit_code);
}

char** bufferPacketCreate(void) 
{
    char **packet = (char**) calloc(BUFFER_SIZE, sizeof(char*));
    if (packet == NULL) 
    {
        logAndExit("Failed to allocate buffer packet array", __FILE__, EXIT_FAILURE);
    }
    return packet;
}

void bufferPacketDelete(char **bufferPacket) 
{
    if (bufferPacket != NULL)
    {
        free(bufferPacket);
    }
}

void bufferPacketFree(char **bufferPacket, uint8_t* bufferPacketIndex) 
{
    for (uint8_t i = 0; i < *bufferPacketIndex; i++) 
    {
        if (bufferPacket[i] != NULL) 
        {
            free(bufferPacket[i]);
            bufferPacket[i] = NULL;
        }
    }
    *bufferPacketIndex = 0;
}

void cleanupClientHandler(clientData_t* clientData, char** bufferPacket, uint8_t* bufferPacketIndex) 
{
    bufferPacketFree(bufferPacket, bufferPacketIndex);
    if (clientData->newSockFd > 0) 
    {
        close(clientData->newSockFd);
    }
    if (clientData != NULL)
    {
        free(clientData);
        clientData = NULL;
    }    
    printf("cleanupClientHandler() thread ID: %ld\n", syscall(SYS_gettid));
}

void cleanupMain(void) 
{
    if (timestampThreadStarted && pipeTimestampWriterHandler[1] > 0) 
    {
        ssize_t res = write(pipeTimestampWriterHandler[1], "x", 1);
        (void)res; // Unused variable
        pthread_join(timestampThread, NULL);
    }
    
    if (threadCount > 0 && pipeClientHandler[1] > 0) 
    {
        for (int i = 0; i < threadCount; i++) 
        {
            ssize_t res = write(pipeClientHandler[1], "x", 1);
            (void)res; // Unused variable
        }
    }

    if (threadCount > 0) 
    {
        for (int i = 0; i < threadCount; i++) 
        {
            pthread_join(clientThreads[i], NULL);
        }
    }

    if (serverSockFd > 0) 
    {
        close(serverSockFd);
    }
    syslog(LOG_INFO, "Server shutting down");
    remove("/var/tmp/aesdsocketdata");
    closelog();
    printf("Server shutting down\n");
}

bool checkForNullCharInString(const char *str, const ssize_t len) 
{
    if (str == NULL || len <= 0) 
    {
        syslog(LOG_WARNING, "Received NULL string or invalid length");
        return true;
    }
    for (ssize_t i = 0; i < len; i++) 
    {
        if (str[i] == '\0') 
        {
            syslog(LOG_WARNING, "String contains null character at position %zd", i);
            return true;
        }
    }
    return false;
}

bool checkForNewlineCharInString(const char *str, const ssize_t len, ssize_t *newlinePosition) 
{
    if (str == NULL || len <= 0) 
    {
        syslog(LOG_DEBUG, "Received NULL string or invalid length");
        return true;
    }
    for (ssize_t i = 0; i < len; i++) 
    {
        if (str[i] == '\n') 
        {
            if (newlinePosition != NULL) 
            {
                *newlinePosition = i;
            }
            syslog(LOG_DEBUG, "String contains newline character at position %zd", i);
            return true;
        }
    }
    return false;
}

void SIGINTHandler(int signum, siginfo_t *info, void *extra)
{
    (void)signum; // Unused parameter
    (void)info;   // Unused parameter
    (void)extra;  // Unused parameter
    printf("Handler SIGINT, thread ID: %ld\n", syscall(SYS_gettid));
    stop = 1;
}

void SIGTERMHandler(int signum, siginfo_t *info, void *extra)
{
    (void)signum; // Unused parameter
    (void)info;   // Unused parameter
    (void)extra;  // Unused parameter
    printf("Handler SIGTERM, thread ID: %ld\n", syscall(SYS_gettid));
    stop = 1;
}

void setSignalSIGINTHandler(void)
{
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_flags = SIGINT;     
    action.sa_sigaction = SIGINTHandler;
    sigaction(SIGINT, &action, NULL);
}

void setSignalSIGTERMHandler(void)
{
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_flags = SIGTERM;     
    action.sa_sigaction = SIGTERMHandler;
    sigaction(SIGTERM, &action, NULL);
}

void* timestampWriterHandler(void* arg)
{
    (void)arg; // Unused parameter
    while (1)
    {
        struct pollfd pfd;
        pfd.fd = pipeTimestampWriterHandler[0];
        pfd.events = POLLIN;

        int ret = poll(&pfd, 1, 10000);

        if (ret == -1)
        {
            perror("poll");
            break;
        }
        else if (ret == 0)
        {
            time_t now = time(NULL);
            struct tm *tm_info = localtime(&now);
            char time_str[128];

            strftime(time_str, sizeof(time_str), "%a, %d %b %Y %H:%M:%S %z", tm_info);

            char final_line[256];
            snprintf(final_line, sizeof(final_line), "timestamp:%s\n", time_str);

            pthread_mutex_lock(&fileMutex);
            FILE* f = fopen("/var/tmp/aesdsocketdata", "a");
            if (f != NULL)
            {
                fputs(final_line, f);
                fclose(f);
            }
            pthread_mutex_unlock(&fileMutex);
        }
        else if (pfd.revents & POLLIN)
        {
            char buf[1];
            ssize_t res = read(pipeTimestampWriterHandler[0], buf, 1);
            (void)res; // Unused variable
            printf("Timestamp writer thread terminating...\n");
            break;
        }
    }

    pthread_exit(NULL);
}

void* clientHandler(void* arg) 
{
    clientData_t* clientData = (clientData_t*)arg;
    struct pollfd fds[2];
    uint8_t bufferPacketIndex = 0;
    char **bufferPacket = bufferPacketCreate();

    fds[0].fd = clientData->newSockFd;
    fds[0].events = POLLIN;

    fds[1].fd = pipeClientHandler[0];
    fds[1].events = POLLIN;

    printf("Client file descriptor: %d\n", clientData->newSockFd);
    printf("clientHandler() thread ID: %ld\n", syscall(SYS_gettid));

    while (1) 
    {
        bufferPacketFree(bufferPacket, &bufferPacketIndex);
        char *buffer = (char*) calloc(BUFFER_SIZE, sizeof(char));

        if (buffer == NULL) 
        {
            logAndExit("Failed to allocate memory for buffer", __FILE__, EXIT_FAILURE);
        }

        if (bufferPacketIndex >= 128) 
        {
            bufferPacketFree(bufferPacket, &bufferPacketIndex);
            logAndExit("Buffer packet index exceeded limit", __FILE__, EXIT_FAILURE);
        }

        bufferPacket[bufferPacketIndex++] = buffer;

        int ret = poll(fds, 2, -1);
        if (ret == -1) 
        {
            perror("poll");
            break;
        }

        if (fds[1].revents & POLLIN) {
            char tmp;
            ssize_t res = read(pipeClientHandler[0], &tmp, 1);
            (void)res; // Unused variable
            printf("Receiver thread terminating...\n");
            break;
        }

        if (fds[0].revents & POLLIN) 
        {
            printf("Received data from client\n");
            ssize_t recvLen = recv(clientData->newSockFd, buffer, BUFFER_SIZE - 1, 0);
            if (recvLen >= 0 && recvLen < BUFFER_SIZE) 
            {
                buffer[recvLen] = '\0';
            }

            if (recvLen < 0) 
            {
                bufferPacketFree(bufferPacket, &bufferPacketIndex);
                logAndExit("Failed to receive data", __FILE__, EXIT_FAILURE);
            } 
            else if (recvLen == 0) 
            {
                syslog(LOG_INFO, "Closed connection from %s", inet_ntoa(clientData->clientAddr.sin_addr));
                printf("Closed connection from %s\n", inet_ntoa(clientData->clientAddr.sin_addr));
                bufferPacketFree(bufferPacket, &bufferPacketIndex);
                close(clientData->newSockFd);
                break;
            }      
            
            printf("Received %zd bytes from %s\n", recvLen, inet_ntoa(clientData->clientAddr.sin_addr));

            if (checkForNullCharInString(buffer, recvLen)) 
            {
                bufferPacketFree(bufferPacket, &bufferPacketIndex);
                printf("Received data contains null character\n");
            }

            ssize_t newlinePosition = -1;
            if (checkForNewlineCharInString(buffer, recvLen, &newlinePosition)) 
            {
                pthread_mutex_lock(&fileMutex);
                FILE *fptr;
                fptr = fopen("/var/tmp/aesdsocketdata", "a");

                printf("Received data contains newline character\n");
                for (size_t i = 0; i < bufferPacketIndex; i++)
                {
                    if (bufferPacket[i] == NULL) 
                    {
                        continue;
                    }
                    printf("Writing to file (file descriptor %d): %s\n", clientData->newSockFd, bufferPacket[i]);
                    fwrite(bufferPacket[i], 1, strlen(bufferPacket[i]), fptr);
                }

                fclose(fptr);
                char bufferSend[BUFFER_SIZE] = {0};
                memset(bufferSend, 0, sizeof(bufferSend));

                fptr = fopen("/var/tmp/aesdsocketdata", "r");
                while (1)
                {
                    size_t bytesRead = fread(bufferSend, 1, sizeof(bufferSend), fptr);
                    if (bytesRead == 0) 
                    {
                        if (feof(fptr)) 
                        {
                            break;
                        } 
                        else 
                        {
                            logAndExit("Failed to read from file", __FILE__, EXIT_FAILURE);
                        }
                    }
                    send(clientData->newSockFd, bufferSend, bytesRead, 0);
                    memset(bufferSend, 0, sizeof(bufferSend));
                }
                fclose(fptr);
                pthread_mutex_unlock(&fileMutex);
            }
        }
    }

    cleanupClientHandler(clientData, bufferPacket, &bufferPacketIndex);
    bufferPacketDelete(bufferPacket);
    printf("Close client handler thread ID: %ld\n", syscall(SYS_gettid));
    pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
    printf("Server version: %s\n", version);
    openlog("Server", LOG_PID, LOG_USER);

    if (argc == 2 && strcmp(argv[1], "-d") == 0) 
    {
        runAsDaemon = true;
    }

    if (runAsDaemon) 
    {
        printf("Running as daemon\n");
        syslog(LOG_INFO, "Running as daemon");
        pid_t pid = fork();

        if (pid < 0) 
        {
            logAndExit("Failed to fork for daemon", __FILE__, EXIT_FAILURE);
        }

        if (pid > 0) 
        {
            exit(EXIT_SUCCESS);
        }

        if (setsid() < 0) 
        {
            logAndExit("Failed to setsid", __FILE__, EXIT_FAILURE);
        }

        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);

        int nullfd = open("/dev/null", O_RDWR);
        if (nullfd != -1) 
        {
            dup2(nullfd, STDIN_FILENO);
            dup2(nullfd, STDOUT_FILENO);
            dup2(nullfd, STDERR_FILENO);
            if (nullfd > STDERR_FILENO) 
            {
                close(nullfd);
            }
        }
    }

    setSignalSIGINTHandler();
    setSignalSIGTERMHandler();

    serverSockFd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (serverSockFd < 0) 
    {
        logAndExit("Failed to create socket", __FILE__, EXIT_FAILURE);
    }

    int optval = 1;
    if (setsockopt(serverSockFd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) 
    {
        logAndExit("Failed to set socket options", __FILE__, EXIT_FAILURE);
    }

    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;  
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);  
    serverAddr.sin_port = htons(9000);  

    if (bind(serverSockFd, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) 
    {
        logAndExit("Failed to bind socket", __FILE__, EXIT_FAILURE);
    }

    listen(serverSockFd, 5);

    if (pthread_create(&timestampThread, NULL, timestampWriterHandler, NULL) == 0) 
    {
        timestampThreadStarted = true;
    }

    if (pipe(pipeClientHandler) < 0) 
    {
        logAndExit("Failed to create pipe for client handler", __FILE__, EXIT_FAILURE);
    }

    if (pipe(pipeTimestampWriterHandler) < 0) 
    {
        logAndExit("Failed to create pipe for timestamp writer handler", __FILE__, EXIT_FAILURE);
    }

    while (!stop)
    {
        struct sockaddr_in clientAddr;
        int clientLen = sizeof(clientAddr);
        int newSockFd = 0;

        printf("Waiting for a connection...\n");
        newSockFd = accept(serverSockFd, (struct sockaddr *) &clientAddr, (socklen_t *) &clientLen);
        if (newSockFd < 0)
        {
            if (errno == EINTR && stop)
            {
                break;
            }
            logAndExit("Failed to accept connection", __FILE__, EXIT_FAILURE);
        }
        printf("File descriptor for new connection: %d\n\n", newSockFd);
        syslog(LOG_INFO, "Accepted connection from %s", inet_ntoa(clientAddr.sin_addr));
        printf("Accepted connection from %s\n", inet_ntoa(clientAddr.sin_addr));

        clientData_t* clientDataContainer = malloc(sizeof(clientData_t));
        memset(clientDataContainer, 0, sizeof(clientData_t));
        clientDataContainer->clientLen = sizeof(struct sockaddr_in);
        clientDataContainer->newSockFd = newSockFd;
        clientDataContainer->clientAddr = clientAddr;

        pthread_create(&clientThreads[threadCount++], NULL, clientHandler, clientDataContainer);
    }

    cleanupMain();    
    syslog(LOG_INFO, "Caught signal, exiting");
    printf("Caught signal, exiting\n");
    return EXIT_SUCCESS;
}
