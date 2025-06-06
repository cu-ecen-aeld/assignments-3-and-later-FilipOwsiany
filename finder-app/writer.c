#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

void log_and_exit(const char *msg, const char *filename, int exit_code) {
    if (filename)
        syslog(LOG_ERR, msg, filename);
    else
        syslog(LOG_ERR, "%s", msg);

    perror(msg);
    closelog();
    exit(exit_code);
}

int main(int argc, char *argv[]) {
    openlog("writer", LOG_PID, LOG_USER);

    if (argc != 3) {
        syslog(LOG_ERR, "Invalid number of arguments: expected 2, got %d", argc - 1);
        fprintf(stderr, "Usage: %s <file> <string>\n", argv[0]);
        closelog();
        return EXIT_FAILURE;
    }

    const char *filename = argv[1];
    const char *text = argv[2];

    FILE *fp = fopen(filename, "w");
    if (!fp)
        log_and_exit("Failed to open file '%s' for writing", filename, EXIT_FAILURE);

    if (fprintf(fp, "%s", text) < 0) {
        fclose(fp);
        log_and_exit("Failed to write to file '%s'", filename, EXIT_FAILURE);
    }

    syslog(LOG_DEBUG, "Writing \"%s\" to \"%s\"", text, filename);

    fclose(fp);
    closelog();
    return EXIT_SUCCESS;
}
