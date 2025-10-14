#include "simpleshell.h"

int main(int argc, char *argv[]) {
    FILE* fptr = NULL;
    char line_buf[BUFSIZ]; // lines are not expected to be greater than BUFSIZ bytes
    int latest_exit_status = 0; // stores exit status value of last spawned command
    char *token_ptr = NULL;
    if (argc > 1) {
        // more than 1 argument implies our shell is being called through an interpreter
        if ((fptr = fopen(argv[1], "r")) == NULL) {
            fprintf(stderr, "Failed to fopen file: %s. %s \n", argv[1], strerror(errno));
            exit(1);
        }
    } else {
        if((fptr = fdopen(0, "r")) == NULL) {
            fprintf(stderr, "Failed to fdopen standard input. %s", strerror(errno));
            exit(1);
        }
    }

    for (;;) {
        if (fgets(line_buf, sizeof(line_buf), fptr) == NULL) {
            if (feof(fptr) != 0) {
                // EOF detected, exit using status value of last spawned command
                exit(latest_exit_status);
            }
            // if fgets fails, exit the program
            fprintf(stderr, "Failed to read line using fgets. %s", strerror(errno));
            exit(1);
        }

        token_ptr = strtok(line_buf, " ");
        while (token_ptr != NULL) {
            printf("Argument detected: %s \n", token_ptr);
            token_ptr = strtok(NULL, " ");
        }
        memset(line_buf, '\0', sizeof(line_buf));
    }

}