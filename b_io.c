/**************************************************************
* Class::  CSC-415-01 Summer 2024
* Name:: Siarhei Pushkin
* Student ID:: 922907437
* GitHub-Name:: spushkin
* Project:: Assignment 5 – Buffered I/O read
*
* File:: b_io.c
*
* Description:: A simple buffered I/O implementation in C
* includes functions for opening (b_open), reading (b_read), 
* and closing (b_close) files. It uses a fixed buffer size
* of 512 (B_CHUNK_SIZE) and supports up to up to 20 files open
* at a time. The b_read function loads data into the buffer, 
* while b_close releases resources and resets the buffer.
**************************************************************/
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

#include "b_io.h"
#include "fsLowSmall.h"

#define MAXFCBS 20	//The maximum number of files open at one time


// This structure is all the information needed to maintain an open file
// It contains a pointer to a fileInfo strucutre and any other information
// that you need to maintain your open file.
typedef struct b_fcb {
    fileInfo * fi; //holds the low level systems file info
    char *buffer; // buffer for file I/O
    int bufferPosition; // current position in the buffer
    int filePosition; // current position in the file
} b_fcb;
	
//static array of file control blocks
b_fcb fcbArray[MAXFCBS];

// Indicates that the file control block array has not been initialized
int startup = 0;	

// Method to initialize our file system / file control blocks
// Anything else that needs one time initialization can go in this routine
void b_init ()
	{
	if (startup)
		return;			//already initialized

	//init fcbArray to all free
	for (int i = 0; i < MAXFCBS; i++)
		{
		fcbArray[i].fi = NULL; //indicates a free fcbArray
		}
		
	startup = 1;
	}

//Method to get a free File Control Block FCB element
b_io_fd b_getFCB ()
	{
	for (int i = 0; i < MAXFCBS; i++)
		{
		if (fcbArray[i].fi == NULL)
			{
			fcbArray[i].fi = (fileInfo *)-2; // used but not assigned
			return i;		//Not thread safe but okay for this project
			}
		}

	return (-1);  //all in use
	}

// b_open is called by the "user application" to open a file.  This routine is 
// similar to the Linux open function.  	
// You will create your own file descriptor which is just an integer index into an
// array of file control blocks (fcbArray) that you maintain for each open file.  
// For this assignment the flags will be read only and can be ignored.

b_io_fd b_open (char * filename, int flags) 
	{

    if (startup == 0) b_init();	  //Initialize our system
	// get a free fcb
    b_io_fd fd = b_getFCB();
	// error if no fcb is available
    if (fd == -1) return -1;
	// retrieve file info
    fileInfo *fi = GetFileInfo(filename);
    if (fi == NULL) {
        fcbArray[fd].fi = NULL;
        return -1;
    }

	// assign file info to the fcb
    fcbArray[fd].fi = fi;
	// allocate buffer
    fcbArray[fd].buffer = malloc(B_CHUNK_SIZE);
    if (fcbArray[fd].buffer == NULL) {
        fcbArray[fd].fi = NULL;
        return -1;
    }
	// initialize buffer position
    fcbArray[fd].bufferPosition = 0;
	// initialize file position
    fcbArray[fd].filePosition = 0;

	// return file descriptor
    return fd;
}

// b_read functions just like its Linux counterpart read.  The user passes in
// the file descriptor (index into fcbArray), a buffer where thay want you to 
// place the data, and a count of how many bytes they want from the file.
// The return value is the number of bytes you have copied into their buffer.
// The return value can never be greater then the requested count, but it can
// be less only when you have run out of bytes to read.  i.e. End of File	
int b_read (b_io_fd fd, char * buffer, int count)
 	{
    if (startup == 0) b_init();	  //Initialize our system

    if ((fd < 0) || (fd >= MAXFCBS) || (fcbArray[fd].fi == NULL)) {
        return -1;
    }
	// total bytes read
    int bytesRead = 0;
    while (count > 0) {
		// read a new chunk from the file
        if (fcbArray[fd].bufferPosition >= B_CHUNK_SIZE || fcbArray[fd].bufferPosition == 0) {
			// number of blocks to read
            int blocksToRead = 1;
            if (fcbArray[fd].filePosition + B_CHUNK_SIZE > fcbArray[fd].fi->fileSize) {
				// adjust blocksToRead if goes beyond the EOF
                blocksToRead = (fcbArray[fd].fi->fileSize - fcbArray[fd].filePosition + B_CHUNK_SIZE - 1) / B_CHUNK_SIZE;
            }
            uint64_t blocksRead = LBAread(fcbArray[fd].buffer, blocksToRead, 
						fcbArray[fd].fi->location + (fcbArray[fd].filePosition / B_CHUNK_SIZE));
			// no more data
            if (blocksRead == 0) break;
			// reset buffer position
            fcbArray[fd].bufferPosition = 0;
        }

        int bytesToCopy = B_CHUNK_SIZE - fcbArray[fd].bufferPosition;
        if (bytesToCopy > count) bytesToCopy = count;
        if (bytesToCopy > fcbArray[fd].fi->fileSize - fcbArray[fd].filePosition) {
			// adjust bytesToCopy if goes beyond the EOF
            bytesToCopy = fcbArray[fd].fi->fileSize - fcbArray[fd].filePosition;
        }

        memcpy(buffer, fcbArray[fd].buffer + fcbArray[fd].bufferPosition, bytesToCopy);
        buffer += bytesToCopy;
        count -= bytesToCopy;
        bytesRead += bytesToCopy;
        fcbArray[fd].bufferPosition += bytesToCopy;
        fcbArray[fd].filePosition += bytesToCopy;
		// end of file
        if (fcbArray[fd].filePosition >= fcbArray[fd].fi->fileSize) break;
    }

    return bytesRead;
}

// b_close frees and allocated memory and places the file control block back 
// into the unused pool of file control blocks.
int b_close (b_io_fd fd) 
{
    if ((fd < 0) || (fd >= MAXFCBS) || (fcbArray[fd].fi == NULL)) {
        return -1;
    }
	// free buffer
    free(fcbArray[fd].buffer);
	// mark fcb as free
    fcbArray[fd].fi = NULL;

    return 0;
}
	