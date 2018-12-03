/*
 *  MATT RINNE
 *  ASSIGN 5
 *  CS300
 *  VIRTUAL MEMORY MANAGER
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define PAGE_TABLE_SIZE 128
#define PAGE_SIZE 256
#define TLB_SIZE 16
#define FRAME_SIZE 256
#define NUM_FRAMES 128
#define NUM_BYTES 256

#define BUFFER_SIZE 8


/******** global variables ********/
FILE *addresses;
FILE *bs;

int page_fault_count = 0;
int next_ptn = 0; /* tracks next open page table slot */
int time = 0; /* tracks the "time" */
int pt_count = 0; /* counts how many times the page table is updated */

char address[BUFFER_SIZE];  /* used for reading in addresses */
int logical_address;
int bin_address[32];  /* used for holding the binary translation of the loggical address */

int ptn[PAGE_TABLE_SIZE]; /* holds page numbers in the page table */
int ptt[PAGE_TABLE_SIZE]; /* holds the time at which the each page is put into the page table */

int physical_memory[NUM_FRAMES][FRAME_SIZE];

signed char bs_buffer[NUM_BYTES];
signed char frame_value;

int tlb[TLB_SIZE][2]; /* TLB, first column is pagge number, second is frame number */

int fifo = 0; /* used for tracking first in first out in the TLB */
int TLB_hit_count = 0;
/**********************************/


/******** functions ********/
int get_page_number(int logical);
int get_page_offset(int logical);
void insert_TLB(int pn, int fn);
int read_from_bs(int pn);
// int read_from_bs2(int x, int pn);
void find_page(int logical);
/***************************/


/* returns page number from logical address */
int
get_page_number(int logical)
{
  return (logical & 0xFFFF)>>8;
}


/* returns page offset from logical address */
int get_page_offset(int logical)
{
  return (logical & 0xFF);
}


/* inserts page and frame number into TLB */
void
insert_TLB(int pn, int fn)
{
  tlb[fifo][0] = pn;
  tlb[fifo][1] = fn;
  fifo++;
  if (fifo == TLB_SIZE) fifo = 0;
}


/* reads value from BINARY_STORE */
int
read_from_bs(int pn)
{
  fseek(bs,pn*NUM_BYTES,SEEK_SET);
  fread(bs_buffer,sizeof(signed char),NUM_BYTES,bs);
  if (pt_count < NUM_FRAMES)
  {
    for (int i=0; i<NUM_BYTES; i++) physical_memory[next_ptn][i] = bs_buffer[i];
    ptn[next_ptn] = pn; /* stores page number in page table */
    ptt[next_ptn] = time; /* stores time in page table */
    time++;
    next_ptn++; /* increments the next open page table slot */
    pt_count++;
    return next_ptn - 1;  /* returns the frame used in physical memory */
  }
  else
  {
    int min_time = ptt[0];
    int min_entry = 0;
    for (int i=1; i<PAGE_TABLE_SIZE; i++) if (ptt[i] < min_time) min_entry = i; /* finds page in page table that was least recently used */
    for (int i=0; i<NUM_BYTES; i++) physical_memory[min_entry][i] = bs_buffer[i];
    ptn[min_entry] = pn;  /* replaces least recently used pagge with new page */
    ptt[min_entry] = time;  /* updates the time correlated to the new page */
    pt_count++;
    time++;
    return min_entry; /* returns the frame used in physical memory */
  }
}


/* retrieves page from logical address */
void
find_page(int logical)
{
  int page_number = get_page_number(logical);
  int offset = get_page_offset(logical);
  int frame_number = -1;
  int flag = 0;

  /* check for page number in TLB */
  for(int i=0; i<TLB_SIZE; i++)
  {
    if (tlb[i][0] == page_number) /* page number is found in TLB */
    {
      frame_number = tlb[i][1];
      ptt[frame_number] = time;
      time++;
      TLB_hit_count++;
      flag = 1;  /* flag indicating page was found in TLB */
    }
  }

  /* scan page table for frame number if not in TLB*/
  if (frame_number == -1)
  {
    for (int i=0; i<next_ptn; i++)
    {
      if (ptn[i] == page_number)  /* page number is found in page table */
      {
        ptt[i] = time;
        time++;
        frame_number = i;
      }
    }
    if (frame_number == -1)
    {
      frame_number = read_from_bs(page_number);
      page_fault_count++;
    }
  }

  frame_value = physical_memory[frame_number][offset];
  if (!flag) insert_TLB(page_number,frame_number);  /* if not found in TLB, insert */
  printf("Virtual address: %d ",logical);
  printf("Physical address: %d ",(frame_number<<8) | offset);
  printf("Value: %d\n",frame_value);
}


int
main(int argc, char const *argv[]) {
  if (argc != 2)
  {
    fprintf(stderr,"Must include address file\n");
    fprintf(stderr,"Exiting...\n");
    return -1;
  }

  addresses = fopen(argv[1], "r");
  if (addresses == NULL)
  {
    fprintf(stderr,"Address file is NULL\n");
    return -1;
  }

  bs = fopen("BACKING_STORE.bin","rb");
  if (bs == NULL)
  {
    fprintf(stderr,"BACKING_STORE is NULL\n");
    return -1;
  }

  int count = 0;
  while (fgets(address,BUFFER_SIZE,addresses) != NULL)
  {
    logical_address = atoi(address);
    find_page(logical_address);
    count++;
  }
  printf("Number of Translated Addresses = %d\n",count);
  printf("Page Faults = %d\n",page_fault_count);
  printf("Page Fault Rate = %.3f\n",page_fault_count/(double)count);
  printf("TLB Hits = %d\n",TLB_hit_count);
  printf("TLB Hit Rate = %.3f\n",TLB_hit_count/(double)count);

  fclose(addresses);
  fclose(bs);
  return 0;
}
