#define FUSE_USE_VERSION 31

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <math.h>

//ADD FILE NOT FOUND ERROR RETURN if not located in tree or storage

typedef struct {
    int rank;
    int branch;
} Coordinate;

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
        ((FileCheck*)fi->fh)->exists = 1;
    } else
        res = -ENOENT;

    return res;
}

static char** getNode() {
    //code to find current node path
}

static char** getNode(Coordinate path) {
    //library to convert coordinate to string
}

static Coordinate getCoord() {
    char nodePath[] = getNode();
    //library to convert string to coordinates
}
static Coordinate getCoord(char nodePath[]) {
    //library to convert string to coordinates
}

bool doesFileExist(char* targetFilename, nodePath[]) {
    struct fuseFileInfo fileInfo;
    int res;

    res = fuse_getattr(path, NULL, &fileInfo);
    if (res == 0) {
        return true;
    } else {
        return false;
    }
}
static Coordinate findFile(char** fileName) {
    bool fileFound = false; 
    Coordinate fileLocation;

    fileLocation.rank = 0;
    fileLocation.branch = 0; //NOTE: (0,0) is the coordinate of storage 

    char nodePath[] = getNode();
    if (doesFileExist(char** fileName, getNode())) {
        //read file from node cache
        fileRecieved = true;
    } else {
        
        Coordinate coord = getCoord(nodePath[]);
        bool loop = true;
        while(loop) {
            if (coord.rank-1<1) {
                loop = false;
            }
            coord.rank -= 1;
            coord.branch = coord.branch / 2;
            if (/*reads node cache from new coords by getNode and return bool if file is there*/) {
                loop = false;
                fileLocation = coord;
            } 
        }
    }
    return fileLocation;
}

static void returnFile (char**, fileName, Coordinate fileLocation) {
    if (fileLocation.rank == 0) {
        //code to send file up to first node (1,0) from storage nodes
        fileLocation.rank = 1;
        fileLocation.branch = 0;
        populateTree(fileName, fileLocation);
    } else {
        populateTree(fileName, fileLocation);
    }
}

static void populateTree (char** fileName, Coordinate fileLocation) {
    int size = getCoord().rank - fileLocation.rank;
    for (int i = size; i > 0 ; i--) {
        Coordinate sendNode; 
        sendNode.rank = getCoord().rank - i;
        sendNode.branch = getCoord().branch * pow(0.5, i-1); 
        Coordinate recieveNode;
        recieveNode.rank = getCoord().rank -i + 1;
        recieveNode.branch = getCoord().branch * pow(0.5, (i-1)); 
        printf("Send: ( %d , %d ) Recieve: ( %d , %d )\n", sendNode.rank, sendNode.branch, recieveNode.rank, recieveNode.branch);
        //code to send file from sendNode to recieve node (also uses getNode function to get node path)
    }

}

int main(char** fileName) {
    bool fileRecieved = false;
    findFile(fileName);
    if (!fileRecieved) {
        returnFile(fileName);
    }
    return 0;
}