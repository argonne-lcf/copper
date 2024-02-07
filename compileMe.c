#include <stdlib.h>
#include <stdio.h>

/*Use this file to compile full program, then execute*/

int main() {
    int result;

    // system() will invoke the shell (command line)
    // and execute the command that's passed to it
    result = system("gcc -Wall -DDEBUG src/cufuse.c src/testcase.c libs/include/fuse.h  -o project -lm -lfuse");

    if(result == 0) {
        printf("The program compiled successfully.\n");
    } else {
        printf("ERROR: The program did not compile.\n");
    }

    return 0;
}
