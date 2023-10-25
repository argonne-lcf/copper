#include <stdbool.h>
//ADD FILE NOT FOUND ERROR RETURN
typedef struct {
    int rank;
    int branch;
} Coordinate;

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

static Coordinate findFile(char** fileName) {
    bool fileFound = false; 
    Coordinate fileLocation;
    fileLocation.rank = 0;
    fileLocation.branch = 0; //NOTE: (0,0) is the coordinate of storage 
    char nodePath[] = getNode();
    if (/*Read node cache method (returns true if file is there)*/) {
        //read file from node cache
        fileLocation = getCoord(nodePath[]);
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

static void returnFile (Coordinate fileLocation) {
    if (fileLocation.rank == 0) {
        //code to send file up tree from storage
    } else {
        int size = getCoord().rank - fileLocation.rank;
        for (int i = size; i < 0 ; i--) {
            Coordinate sendNode; 
            sendNode.rank = getCoord().rank - i -1;
            sendNode.branch = getCoord().branch / (2 * i-1); //fix: when i =0, branch/0
            Coordinate recieveNode;
            recieveNode.rank = getCoord().rank -i;
            recieveNode.branch = getCoord().branch / (2 * i); //same issue
        }
    }
}
