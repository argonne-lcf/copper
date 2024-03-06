
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <math.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>
#include "passthrough_fh.h"


#define BUF_SIZE 8192


typedef struct {
    int rank;
    int branch;
} Coordinate;


bool fileRecieved;
char* fileName;
char* homeDir;


bool doesFileExist(char* filename, char* nodePath) {
    #ifdef DEBUG
        printf("doesFileExist() called\n");
    #endif
    char* path = malloc(strlen(filename) + strlen(nodePath) + 1); //allocates memory for complete filename
    strcpy(path, nodePath); 
    strcat(path, filename);
    int res = passthrough_fh_access(path, R_OK);  //uses access to check of a files existance and that it is able to be read from
    if (res == 0) {
        free(path);
        #ifdef DEBU
            printf("doesFileExist() closed\n");
        #endif
        return true;
    } else {
        free(path);
        #ifdef DEBUG
            printf("doesFileExist() closed\n");
        #endif
        return false;
    }
    
}

bool copyFile(const char* filename, const char* srcDir, const char* trgtDir) {
    #ifdef DEBUG
        printf("copyFile() called\n");
    #endif
    int srcFd, trgtFd;
    char buf[BUF_SIZE];
    int readSize;

    char* srcPath = malloc(strlen(filename) + strlen(srcDir) + 1); //allocates memory for complete filename
    strcpy(srcPath, srcDir); 
    strcat(srcPath, filename);
    char* trgtPath = malloc(strlen(filename) + strlen(trgtDir) + 1); //allocates memory for complete filename
    strcpy(trgtPath, trgtDir); 
    strcat(trgtPath, filename);

    srcFd = passthrough_fh_open(srcPath, O_RDONLY);
    if (srcFd == -1) {
        perror("xmp_open"); //print error @ xmp_open
        return false;
    }

    struct fuse_file_info fi;
    memset(&fi, 0, sizeof(fi));
    trgtFd = -passthrough_fh_create(trgtPath, S_IRUSR | S_IWUSR, &fi);

    if (trgtFd == -1) {
        perror("xmp_create"); //print error @ xmp_create
        close(srcFd);
        return false;
    }
    //copy the file
    while ((readSize = pread(srcFd, buf, sizeof(buf), 0))>0) {
        if (pwrite(trgtFd, buf, readSize, 0) != readSize) {
            perror("pwrite");
            close(srcFd);
            close(trgtFd);
            return false;
        }
    }

    close(srcFd);
    close(trgtFd);
    #ifdef DEBUG
        printf("copyFile() closed\n");
    #endif
    return true;
}


char* getNode(Coordinate input) {
    #ifdef DEBUG
        printf("getNode() called\n");
    #endif
    FILE *file = fopen("libs/binTreeDef.txt", "r");
    if (file == NULL){
        printf("Error: Could not open tree definition file\n");
    }

    Coordinate temp;
    static char output[64];  // Declare output as a static array

    while (fscanf(file, "%d %d %s", &temp.rank, &temp.branch, output) != EOF) {
        if (temp.rank == input.rank && temp.branch == input.branch) {
            break;
        }
    }
    printf("output: %s\n", output);
    fclose(file);
    #ifdef DEBUG
        printf("getNode() closed\n");
    #endif
    return output; 
}


Coordinate thisCoord() { 
    #ifdef DEBUG
        printf("thisCoord() called\n");
    #endif
    static Coordinate output;
    FILE *file = fopen("libs/binTreeDef.txt", "r");
    Coordinate temp;
    char str[64];

    if (file == NULL){
        printf("Unable to open binTreeDef.txt\n");
    }

    while (fscanf(file, "%d %d %s", &temp.rank, &temp.branch, str) != EOF) {
        if (strcmp(str, homeDir) == 0) {
            output.rank = temp.rank;
            output.branch = temp.branch;
            break;
        }
    }

    fclose(file);
    printf("output: %d, %d\n", output.rank, output.branch);
    #ifdef DEBUG
        printf("thisCoord() closed\n");
    #endif
    return output;
}

Coordinate getCoord(char input[]) {
    #ifdef DEBUG
        printf("getCoord() called\n");
    #endif
    static Coordinate output;
    FILE *file = fopen("../lib/binTreeDef.txt", "r");
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
    #ifdef DEBUG
        printf("getCoord() closed\n");
    #endif
    return output;
}

void populateTree (char* fileName, Coordinate fileLocation) {

    #ifdef DEBUG
        printf("populateTree() called\n");
    #endif

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
        bool errorCheck = copyFile(fileName, getNode(sendNode), getNode(recieveNode));
        if (!errorCheck) {
            printf("Error: populateTree");
        }
        if (doesFileExist(fileName, homeDir)) {
        fileRecieved = true;
        }
    }
    #ifdef DEBUG
        printf("populateTree() closed\n");
    #endif
}

void returnFile (char* fileName, Coordinate fileLocation) {

    #ifdef DEBUG
        printf("returnFile() called\n");
    #endif

    if (fileLocation.rank == 0) {
        Coordinate storage = {0,0};
        fileLocation.rank = 1;
        fileLocation.branch = 0;
        if (doesFileExist(fileName, getNode(storage))) {  
            bool errorCheck = copyFile(fileName, getNode(storage), getNode(fileLocation));
            if (!errorCheck) {
                printf("Error: returnFile");
            }
            populateTree(fileName, fileLocation);
        } else {
            printf("Error: File not found in storage");
        }
    } else {
        populateTree(fileName, fileLocation);
    }
    #ifdef DEBUG
        printf("returnFile() closed\n");
    #endif
}

void findFile(char* fileName) {

    #ifdef DEBUG
        printf("findFile() called\n");
    #endif

    bool fileFound = false; 
    Coordinate fileLocation;

    fileLocation.rank = 0;
    fileLocation.branch = 0; //NOTE: (0,0) is the coordinate of storage 

    if (doesFileExist(fileName, homeDir)) {
        fileRecieved = true;
    } else {
        Coordinate coord = thisCoord();
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
                fileFound = true;
            } 
        }
        if (!loop && !fileFound) {
            printf("File not found\n");
        }
    }
    if (fileFound) {
        returnFile(fileName, fileLocation);
    }
    #ifdef DEBUG
        printf("findFile() closed\n");
    #endif
}

int main(int argc, char *argv[]) {
    
    if(argc < 4) {
        fprintf(stderr, "Usage: %s <mount_point> <input file> <target directory>\n", argv[0]); //checks correct number of arguments
        return 1;
    }
    char *my_argv[2];
    my_argv[0] = argv[0];
    my_argv[1] = argv[1];

    passthrough_fh_main(3, my_argv);

    char* iFileName = argv[2];
    char* targetDir = argv[3];

    #ifdef DEBUG
        printf("Executed\n");
    #endif

    fileRecieved = false;
    fileName = iFileName;
    homeDir = targetDir;
    findFile(fileName);
    if (!fileRecieved) {
        printf("Error: File not recieved");
    }

    return 0;
}
