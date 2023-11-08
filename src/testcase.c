#define FUSE_USE_VERSION 31

//#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <math.h>
#include <sys/stat.h>
#include <stdlib.h>

typedef struct {
    int rank;
    int branch;
} Coordinate;

typedef struct {
    char* filename;
    bool exists;
} FileCheck;

bool fileRecieved;
char* fileName;


/*
//based on user3558391's code on stack overflow: https://stackoverflow.com/questions/23208634/writing-a-simple-filesystem-in-c-using-fuse
static int fuse_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi) {
    int res = 0;
    memset(stbuf, 0, sizeof(struct stat));
    if (strcmp(path, "/") == 0) {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
    } else if (strcmp(path, ((FileCheck*)fi->fh)->filename) == 0) {
        stbuf->st_mode = S_IFREG | 0444;
        stbuf->st_nlink = 1;
        stbuf->st_size = strlen("Hello World!\n");
        ((FileCheck*)fi->fh)->exists = true;
    } else
        res = -ENOENT;

    return res;
}
*/
bool doesFileExist(char* targetFilename, char* nodePath) {
    struct stat stbuf;
    // Construct the full path
    char fullPath[1024];
    snprintf(fullPath, sizeof(fullPath), "%s/%s", nodePath, targetFilename);
    // Check if the file exists
    if (stat(fullPath, &stbuf) == 0) {
        return true;
    } else {
        return false;
    }
}

char* thisNode() {
    //NOTE This method will be different, requiring looking into the system info
    return "Hello";
}

char* getNode(Coordinate input) {
    FILE *file = fopen("../libs/binTreeDef.txt", "r");
    Coordinate temp;
    char str[64];
    static char output[64];  // Declare output as a static array

    if (file == NULL){
        printf("Error: Could not open tree definition file");
    }

    while (fscanf(file, "%d %d %s", &temp.rank, &temp.branch, str) != EOF) {
        if (temp.rank == input.rank && temp.branch == input.branch) {
            strcpy(output, str);  
            break;
        }
    }
    fclose(file);
    return output; 
}


Coordinate thisCoord() { 
    static Coordinate output;
    char* input = thisNode();
    FILE *file = fopen("../libs/binTreeDef.txt", "r");
    Coordinate temp;
    char str[64];

    if (file == NULL){
        printf("Could not open file\n");
    }

    while (fscanf(file, "%d %d %s", &temp.rank, &temp.branch, str) != EOF) {
        if (strcmp(str, input) == 0) {
            output.rank = temp.rank;
            output.branch = temp.branch;
            break;
        }
    }

    fclose(file);
    return output;
}
Coordinate getCoord(char input[]) {
    static Coordinate output;
    FILE *file = fopen("../libs/binTreeDef.txt", "r");
    Coordinate temp;
    char str[64];

    if (file == NULL){
        printf("Could not open file\n");
    }

    while (fscanf(file, "%d %d %s", &temp.rank, &temp.branch, str) != EOF) {
        if (strcmp(str, input) == 0) {
            output.rank = temp.rank;
            output.branch = temp.branch;
            break;
        }
    }

    fclose(file);
    return output;
}

void populateTree (char* fileName, Coordinate fileLocation) {
    int size = thisCoord().rank - fileLocation.rank;
    for (int i = size; i > 0 ; i--) {
        Coordinate coord = thisCoord();
        Coordinate sendNode; 
        sendNode.rank = coord.rank - i;
        sendNode.branch = coord.branch * pow(0.5, i-1); 

        Coordinate recieveNode;
        recieveNode.rank = coord.rank -i + 1;
        recieveNode.branch = coord.branch * pow(0.5, (i-1)); 

        printf("Send: ( %d , %d ) Recieve: ( %d , %d )\n", sendNode.rank, sendNode.branch, recieveNode.rank, recieveNode.branch);
        //code to send file from sendNode to recieve node (also uses getNode function to get node path)
        if (doesFileExist(fileName, thisNode())) {
        fileRecieved = true;
        }
    }
}

void returnFile (char* fileName, Coordinate fileLocation) {
    if (fileLocation.rank == 0) {
        if (doesFileExist(fileName, "/path/toStorage")) {  //<----- Replace "/path/toStorage"
        //write file to node one
        fileLocation.rank = 1;
        fileLocation.branch = 0;
        populateTree(fileName, fileLocation);
        } else {
            printf("Error: File not found");
        }
    } else {
        populateTree(fileName, fileLocation);
    }
}

void findFile(char* fileName) {
    bool fileFound = false; 
    Coordinate fileLocation;

    fileLocation.rank = 0;
    fileLocation.branch = 0; //NOTE: (0,0) is the coordinate of storage 
    Coordinate test = getCoord("Hello");
    printf("( %d , %d )", test.rank, test.branch);
    char* nodePath = thisNode();
    printf("\nhello, World!\n");
    if (doesFileExist(fileName, thisNode())) {
        fileRecieved = true;
    } else {
        Coordinate coord = getCoord(nodePath);
        bool loop = true;
        while(loop) {
            if (coord.rank-1<1) {
                loop = false;
            }
            coord.rank -= 1;
            coord.branch = coord.branch / 2;
            if (doesFileExist(fileName, getNode(coord))) {
                loop = false;
                fileLocation = coord;
            } 
        }
    }
    if (!fileRecieved) {
        returnFile(fileName, fileLocation);
    }
}

int main(char* iFileName) {
    //system("./cufuse");
    fileRecieved = false;
    fileName = iFileName;
    Coordinate outputCoord = {0,0};
    findFile(fileName);
    if (!fileRecieved) {
        printf("Error: File not recieved");
    }
    return 0;
}