#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/*Use this file to compile full program while in copper directory (but no lower), then execute*/

int main(int argc, char *argv[]) {
    int result;
    char response;

    if(argc < 2) {
        fprintf(stderr, "Usage: %s <target directory name>\n", argv[0]); //checks correct number of arguments
        return 1;
    }
    // system() will invoke the shell (command line)
    // and execute the command that's passed to it
    result = system("gcc -DDEBUG -D_FILE_OFFSET_BITS=64 src/passthrough_fh.c src/testcase.c `pkg-config fuse3 --cflags --libs` -lulockmgr -o copper -lm");

    if(result == 0) {
        printf("The program compiled successfully.\n");
    } else {
        printf("ERROR: The program did not compile.\n");
    }

    printf("Would you like to continue? (y/n): ");
    scanf("%c", &response);

    char *dirName = argv[1];
    char* command = malloc(strlen("./copper /tmp/") + strlen(dirName) + strlen(" 'test.txt' 'node3'") + 1); //allocates memory for complete filename
    strcpy(command, "./copper /tmp/"); 
    strcat(command, dirName);
    strcat(command, " 'test.txt' 'node3'");
    if(response == 'y' || response == 'Y') {
        system(command);
    } else if(response == 'n' || response == 'N') {
        printf("Exiting...\n");
    } else {
        printf("Invalid input! Please enter y or n.\n");
    }

    return 0;
}
