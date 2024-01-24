#define FUSE_USE_VERSION 31
#define _GNU_SOURCE
#define _FILE_OFFSET_BITS 64

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <math.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>
#include "cufuse.h"

#define BUF_SIZE 8192

typedef struct {
    int rank;
    int branch;
} Coordinate;


bool fileRecieved;
char* fileName;
char* homeDir;


bool doesFileExist(char* filename, char* nodePath) {
    char* path; 
    asprintf(&path, "%s%s", nodePath, filename); //combines nodePath and filename into filepath
    int res = cu_access(path, R_OK);  //uses cu_acces to check of a files existance and that it is able to be read from
    if (res == 0) {
        return true;
    } else {
        return false;
    }
    
}

bool copyFile(const char* filename, const char* srcDir, const char* trgtDir) {
    char* srcPath;
    char* trgtPath;
    int srcFd, trgtFd;
    char buf[BUF_SIZE];
    int readSize;

    asprintf(&srcPath, "%s%s", srcDir, filename); //creates full path of source and target files
    asprintf(&trgtPath, "%s%s", trgtDir, filename);


    srcFd = cu_open(srcPath, O_RDONLY);
    if (srcFd == -1) {
        perror("cu_open"); //print error @ cu_open
        return false;
    }

    struct fuse_file_info fi;
    memset(&fi, 0, sizeof(fi));
    trgtFd = cu_create(trgtPath, S_IRUSR | S_IWUSR, &fi);

    if (trgtFd == -1) {
        perror("cu_create"); //print error @ cu_create
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
    return true;
}


char* getNode(Coordinate input) {
    FILE *file = fopen("../lib/binTreeDef.txt", "r");
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
    FILE *file = fopen("../lib/binTreeDef.txt", "r");
    Coordinate temp;
    char str[64];

    if (file == NULL){
        printf("Could not open file\n");
    }

    while (fscanf(file, "%d %d %s", &temp.rank, &temp.branch, str) != EOF) {
        if (strcmp(str, homeDir) == 0) {
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
        bool errorCheck = copyFile(fileName, getNode(sendNode), getNode(recieveNode));
        if (!errorCheck) {
            printf("Error: populateTree");
        }
        if (doesFileExist(fileName, homeDir)) {
        fileRecieved = true;
        }
    }
}

void returnFile (char* fileName, Coordinate fileLocation) {
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
}

void findFile(char* fileName) {
    bool fileFound = false; 
    Coordinate fileLocation;

    fileLocation.rank = 0;
    fileLocation.branch = 0; //NOTE: (0,0) is the coordinate of storage 
    Coordinate test = getCoord("Hello");
    printf("( %d , %d )", test.rank, test.branch);
    printf("\nhello, World!\n");
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
    }
    if (fileFound) {
        returnFile(fileName, fileLocation);
    }
}

int main(char* iFileName, char* targetDir) {
    //system("./cufuse");
    fileRecieved = false;
    fileName = iFileName;
    homeDir = targetDir;
    Coordinate outputCoord = {0,0};
    findFile(fileName);
    if (!fileRecieved) {
        printf("Error: File not recieved");
    }
    return 0;
}