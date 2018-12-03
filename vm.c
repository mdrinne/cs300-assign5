#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <alloca.h>

#define FRAME_SIZE 256        // size of the frame
#define TOTAL_NUMBER_OF_FRAMES 256  // total number of frames in physical memory
#define ADDRESS_MASK  0xFFFF  //mask all but the address
#define OFFSET_MASK  0xFF //mask all but the offset
#define TLB_SIZE 16       // size of the TLB
#define PAGE_TABLE_SIZE 256  // size of the page table

int pageTableNumbers[PAGE_TABLE_SIZE];  // array to hold the page numbers in the page table
int pageTableFrames[PAGE_TABLE_SIZE];   // array to hold the frame numbers in the page table

int TLBPageNumber[TLB_SIZE];  // array to hold the page numbers in the TLB
int TLBFrameNumber[TLB_SIZE]; // array to hold the frame numbers in the TLB

int physicalMemory[TOTAL_NUMBER_OF_FRAMES][FRAME_SIZE]; // physical memory 2D array

int pageFaults = 0;   // counter to track page faults
int TLBHits = 0;      // counter to track TLB hits
int firstAvailableFrame = 0;  // counter to track the first available frame
int firstAvailablePageTableNumber = 0;  // counter to track the first available page table entry
int numberOfTLBEntries = 0;             // counter to track the number of entries in the TLB

// number of characters to read for each line from input file
#define BUFFER_SIZE         10

// number of bytes to read
#define CHUNK               256

// input file and backing store
FILE    *address_file;
FILE    *backing_store;

// how we store reads from input file
char    address[BUFFER_SIZE];
int     logical_address;

// the buffer containing reads from backing store
signed char     buffer[CHUNK];

// the value of the byte (signed char) in memory
signed char     value;

// headers for the functions used below
void getPage(int address);
void readFromStore(int pageNumber);
void insertIntoTLB(int pageNumber, int frameNumber);

// function to take the logical address and obtain the physical address and value stored at that address
void getPage(int logical_address){

    // obtain the page number and offset from the logical address
    int pageNumber = ((logical_address & ADDRESS_MASK)>>8);
    int offset = (logical_address & OFFSET_MASK);

    // first try to get page from TLB
    int frameNumber = -1; // initialized to -1 to tell if it's valid in the conditionals below

    int i;  // look through TLB for a match
    for(i = 0; i < TLB_SIZE; i++){
        if(TLBPageNumber[i] == pageNumber){   // if the TLB index is equal to the page number
            frameNumber = TLBFrameNumber[i];  // then the frame number is extracted
                TLBHits++;                // and the TLBHit counter is incremented
        }
    }

    // if the frameNumber was not found
    if(frameNumber == -1){
        int i;   // walk the contents of the page table
        for(i = 0; i < firstAvailablePageTableNumber; i++){
            if(pageTableNumbers[i] == pageNumber){         // if the page is found in those contents
                frameNumber = pageTableFrames[i];          // extract the frameNumber from its twin array
            }
        }
        if(frameNumber == -1){                     // if the page is not found in those contents
            readFromStore(pageNumber);             // page fault, call to readFromStore to get the frame into physical memory and the page table
            pageFaults++;                          // increment the number of page faults
            frameNumber = firstAvailableFrame - 1;  // and set the frameNumber to the current firstAvailableFrame index
        }
    }

    insertIntoTLB(pageNumber, frameNumber);  // call to function to insert the page number and frame number into the TLB
    value = physicalMemory[frameNumber][offset];  // frame number and offset used to get the signed value stored at that address
   printf("frame number: %d\n", frameNumber);
printf("offset: %d\n", offset);
    // output the virtual address, physical address and value of the signed char to the console
    printf("Virtual address: %d Physical address: %d Value: %d\n", logical_address, (frameNumber << 8) | offset, value);
}

// function to insert a page number and frame number into the TLB with a FIFO replacement
void insertIntoTLB(int pageNumber, int frameNumber){

    int i;  // if it's already in the TLB, break
    for(i = 0; i < numberOfTLBEntries; i++){
        if(TLBPageNumber[i] == pageNumber){
            break;
        }
    }

    // if the number of entries is equal to the index
    if(i == numberOfTLBEntries){
        if(numberOfTLBEntries < TLB_SIZE){  // and the TLB still has room in it
            TLBPageNumber[numberOfTLBEntries] = pageNumber;    // insert the page and frame on the end
            TLBFrameNumber[numberOfTLBEntries] = frameNumber;
        }
        else{                                            // otherwise move everything over
            for(i = 0; i < TLB_SIZE - 1; i++){
                TLBPageNumber[i] = TLBPageNumber[i + 1];
                TLBFrameNumber[i] = TLBFrameNumber[i + 1];
            }
            TLBPageNumber[numberOfTLBEntries-1] = pageNumber;  // and insert the page and frame on the end
            TLBFrameNumber[numberOfTLBEntries-1] = frameNumber;
        }
    }

    // if the index is not equal to the number of entries
    else{
        for(i = i; i < numberOfTLBEntries - 1; i++){      // iterate through up to one less than the number of entries
            TLBPageNumber[i] = TLBPageNumber[i + 1];      // move everything over in the arrays
            TLBFrameNumber[i] = TLBFrameNumber[i + 1];
        }
        if(numberOfTLBEntries < TLB_SIZE){                // if there is still room in the array, put the page and frame on the end
            TLBPageNumber[numberOfTLBEntries] = pageNumber;
            TLBFrameNumber[numberOfTLBEntries] = frameNumber;
        }
        else{                                             // otherwise put the page and frame on the number of entries - 1
            TLBPageNumber[numberOfTLBEntries-1] = pageNumber;
            TLBFrameNumber[numberOfTLBEntries-1] = frameNumber;
        }
    }
    if(numberOfTLBEntries < TLB_SIZE){                    // if there is still room in the arrays, increment the number of entries
        numberOfTLBEntries++;
    }
}

// function to read from the backing store and bring the frame into physical memory and the page table
void readFromStore(int pageNumber){
    // first seek to byte CHUNK in the backing store
    // SEEK_SET in fseek() seeks from the beginning of the file
    if (fseek(backing_store, pageNumber * CHUNK, SEEK_SET) != 0) {
        fprintf(stderr, "Error seeking in backing store\n");
    }

    // now read CHUNK bytes from the backing store to the buffer
    if (fread(buffer, sizeof(signed char), CHUNK, backing_store) == 0) {
        fprintf(stderr, "Error reading from backing store\n");
    }

    // load the bits into the first available frame in the physical memory 2D array
    int i;
    for(i = 0; i < CHUNK; i++){
        physicalMemory[firstAvailableFrame][i] = buffer[i];
    }

    // and then load the frame number into the page table in the first available frame
    pageTableNumbers[firstAvailablePageTableNumber] = pageNumber;
    pageTableFrames[firstAvailablePageTableNumber] = firstAvailableFrame;

    // increment the counters that track the next available frames
    firstAvailableFrame++;
    firstAvailablePageTableNumber++;
}

// main opens necessary files and calls on getPage for every entry in the addresses file
int main(int argc, char *argv[])
{
    // perform basic error checking
    if (argc != 2) {
        fprintf(stderr,"Usage: ./a.out [input file]\n");
        return -1;
    }

    // open the file containing the backing store
    backing_store = fopen("BACKING_STORE.bin", "rb");

    if (backing_store == NULL) {
        fprintf(stderr, "Error opening BACKING_STORE.bin %s\n","BACKING_STORE.bin");
        return -1;
    }

    // open the file containing the logical addresses
    address_file = fopen(argv[1], "r");

    if (address_file == NULL) {
        fprintf(stderr, "Error opening addresses.txt %s\n",argv[1]);
        return -1;
    }

    int numberOfTranslatedAddresses = 0;
    // read through the input file and output each logical address
    while ( fgets(address, BUFFER_SIZE, address_file) != NULL) {
        logical_address = atoi(address);

        // get the physical address and value stored at that address
        getPage(logical_address);
        numberOfTranslatedAddresses++;  // increment the number of translated addresses
    }

    // calculate and print out the stats
    printf("Number of translated addresses = %d\n", numberOfTranslatedAddresses);
    double pfRate = pageFaults / (double)numberOfTranslatedAddresses;
    double TLBRate = TLBHits / (double)numberOfTranslatedAddresses;

    printf("Page Faults = %d\n", pageFaults);
    printf("Page Fault Rate = %.3f\n",pfRate);
    printf("TLB Hits = %d\n", TLBHits);
    printf("TLB Hit Rate = %.3f\n", TLBRate);

    // close the input file and backing store
    fclose(address_file);
    fclose(backing_store);

    return 0;
}
