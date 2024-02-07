#include <stdlib.h>
#include <stdio.h>

/*Use this file to compile full program while in copper directory (but no lower), then execute*/

int main() {
    int result;
    char response;

    // system() will invoke the shell (command line)
    // and execute the command that's passed to it
    result = system("gcc -DDEBUG -D_FILE_OFFSET_BITS=64 src/testcase.c src/passthrough_fh.c `pkg-config fuse3 --cflags --libs` -lulockmgr -o project -lm");

    if(result == 0) {
        printf("The program compiled successfully.\n");
    } else {
        printf("ERROR: The program did not compile.\n");
    }

    printf("Would you like to continue? (y/n): ");
    scanf("%c", &response);

    if(response == 'y' || response == 'Y') {
        system("./project 'test.txt' 'node3'");
    } else if(response == 'n' || response == 'N') {
        printf("Exiting...\n");
    } else {
        printf("Invalid input! Please enter y or n.\n");
    }

    return 0;
}
