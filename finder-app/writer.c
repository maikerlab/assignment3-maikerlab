#include <syslog.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <dirent.h>
#include <stdlib.h>
#include <limits.h>

bool is_directory(const char *path)
{
    struct stat st;

    if (stat(path, &st) != 0)
        return false;

    return S_ISDIR(st.st_mode);
}

int count_matching_lines(const char *filepath, const char *search)
{
    FILE *fp = fopen(filepath, "r");
    if (!fp)
        return 0;

    int count = 0;
    char buffer[1024];

    while (fgets(buffer, sizeof(buffer), fp))
    {
        if (strstr(buffer, search))
        {
            count++;
        }
    }

    fclose(fp);
    return count;
}

int main(int argc, char **argv)
{
    openlog("writer", LOG_PID, LOG_USER);

    if (argc != 3)
    {
        syslog(LOG_ERR, "Exactly 2 arguments must be provided. Actual: %d", argc - 1);
        printf("Usage: writer <writefile> <writestr>\n");
        closelog();
        return 1;
    }

    const char *filename = argv[1];
    const char *content = argv[2];

    syslog(LOG_DEBUG, "Writing \"%s\" to %s", content, filename);

    FILE *fp = fopen(filename, "w");
    if (fp == NULL)
    {
        syslog(LOG_ERR, "Error opening file %s", filename);
        perror("fopen");
        closelog();
        return 1;
    }

    if (fprintf(fp, "%s", content) < 0)
    {
        syslog(LOG_ERR, "Error writing to file %s", filename);
        perror("fprintf");
        fclose(fp);
        closelog();
        return 1;
    }

    fclose(fp);
    closelog();

    return 0;
}