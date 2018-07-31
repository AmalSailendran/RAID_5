#include "stdio.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/queue.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#define RAID_DISKS 3
#define FILES_PATH "file.txt"
#define MAX_FILE_PATH_LENGTH 255
#define BLOCK_SIZE 2
#define bzero(b,len) (memset((b), '\0', (len)), (void) 0)

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))
int count_block;
typedef struct openFileDescriptor_s
{
    int fd;
    int cursor;
    char fileName[MAX_FILE_PATH_LENGTH + 1];
    int flags;
    int block_count;
    LIST_ENTRY(openFileDescriptor_s) link;
} openFileDescriptor_t;

typedef LIST_HEAD(openFileDescriptor_head, openFileDescriptor_s) openFileDescriptors;

openFileDescriptors currentOpenFds;
char buffer1[50] = {0};

int offset1=0;
int offset2=2;
int offset3=4;

struct files
{
    int count;
    int* descriptors;
};

void *myfunc1(void* myvar);
void *myfunc2(void* myvar);
void *myfunc3(void* myvar);

void *myfunc1(void* myvar)
{
    struct files *f=(struct files*)myvar;
    char *fd1="f1";

    if(f->descriptors[0]==-1)
        printf("\nerror in opening file\n");

    printf("\n offset1 to read : %d", offset1);
    read(f->descriptors[0],buffer1+offset1,2);
    offset1+=6 ;

    printf("\n offset1 to next read : %d", offset1);
    
    return NULL;
}

void *myfunc2(void* myvar)
{
    struct files *f=(struct files*)myvar;
    char *fd2="f2";

    if(f->descriptors[1]==-1)
        printf("\nerror in opening file\n");
    
    printf("\n offset2 to read : %d", offset2);
    read(f->descriptors[1],buffer1+offset2,2);
    offset2+=6;
    printf("\n offset2 to next read : %d", offset2);

    
    return NULL;
}
void *myfunc3(void* myvar)
{
    struct files *f=(struct files*)myvar;

    if(f->descriptors[2]==-1)
        printf("\nerror in opening file\n");
    
    printf("\n offset3 to read : %d", offset3);
    read(f->descriptors[2],buffer1+offset3,2);
    offset3+=6;
    
    printf("\n offset3 next read : %d", offset3);
    
    return NULL;
}




int getFileDescriptor() {
    static int fd = 2^28;
    return fd++;
}

void findParityBlock(int stripe, int *fileNum, int *fileOffset)
{
    *fileNum = stripe % RAID_DISKS;
    printf("\nfildnum in fileParityBlock function: %d\n",*fileNum );
    *fileOffset  = BLOCK_SIZE * stripe;
    printf("\nfileoffset in find parity block function: %d\n",*fileOffset );
}

int findDataBlock(int offset, int *fileNum, int *fileOffset)
{
    /*
     * One Stripe would contains BLOCK_SIZE * (RAID_DISKS - 1) application data,
     * as one block in each stripe is used for parity.
     */

    int dataInEachStripe = BLOCK_SIZE * (RAID_DISKS - 1);
    printf("\ndataInEachStripe is: %d",dataInEachStripe);

    /* First block is our metadata. So compensate that to the offset application is asking */
    //offset += BLOCK_SIZE;

    int whichStripe = (offset + 1) / dataInEachStripe;
    printf("\nwhichStripe is: %d",whichStripe);


    int offsetInStripe = (offset)  % dataInEachStripe;
    printf("\noffsetInStripe is: %d",offsetInStripe);


    int whichBlock = offsetInStripe / BLOCK_SIZE;
    printf("\nwhichBlock : %d",whichBlock);

    int parityFileNum;
    int parityOffset;
    findParityBlock(whichStripe, &parityFileNum, &parityOffset);

    if (whichBlock >= parityFileNum) {
        whichBlock++;
    }

    *fileNum = whichBlock;
    printf("\nfildnum in find data block function: %d\n",*fileNum );
    *fileOffset = whichStripe * BLOCK_SIZE;
    printf("\nfileoffset in find data block function: %d\n",*fileOffset );
    return whichStripe;
}

int raid5_write_int(const char* fileName, int offset, const void* buffer, unsigned int length, unsigned int blockc)
{
    int error = 0;
    count_block=blockc;
    openFileDescriptor_t *countfd;
    FILE* fds[RAID_DISKS] = {0};
    int currOffset = offset;
    printf("\noffset passed fromm write func: %d\n",currOffset);
    int remainingLength = length;
    printf("\nlenght passed fromm write func: %d\n",remainingLength);
    char* data = (char*) buffer;
    printf("\ndata in buffer is: %s\n",data);
    int startStripe = -1;
    int endStripe   = -1;

    for (int i = 0; i < RAID_DISKS; i++) {
        char  raidFile[MAX_FILE_PATH_LENGTH] = {0};
        memcpy(raidFile, FILES_PATH, strlen(FILES_PATH));
        strcat(raidFile, fileName);
        sprintf(raidFile+strlen(raidFile), "%u", i);

        fds[i] = fopen(raidFile, "r+");
        if (fds[i] == NULL) {
            error = -1;
            goto error;
        }
    }

    do {
        int      fileNum, fileOffset;
        int      whichStripe = findDataBlock(currOffset, &fileNum, &fileOffset);
        int      actualOffset = fileOffset + (currOffset % BLOCK_SIZE);
        printf("\ncurrent whichStripe from write func: %d\n",whichStripe );
        if (startStripe == -1) {
            startStripe = whichStripe;
        }

        endStripe = whichStripe;
        printf("\nendstripe is: %d\n", endStripe);
        printf("\nactual offset is: %d\n", actualOffset);
        int lengthToWriteInBlock;
        if ((actualOffset % BLOCK_SIZE) != 0) {
            lengthToWriteInBlock = MIN(BLOCK_SIZE - (actualOffset - fileOffset), remainingLength);
        } else {
            lengthToWriteInBlock = MIN(BLOCK_SIZE, remainingLength);
        }

        fseek(fds[fileNum], actualOffset, SEEK_SET);
        if (fwrite(data, 1, lengthToWriteInBlock, fds[fileNum]) != lengthToWriteInBlock) {
            printf("Cant write to the file %d bytes", lengthToWriteInBlock);
            error = -1;
            goto error;
        }

        remainingLength -= lengthToWriteInBlock;
        currOffset += lengthToWriteInBlock;
        data += lengthToWriteInBlock;
        count_block++;            
        
    } while (remainingLength > 0);
    

    printf("\nblock count is : %d",count_block);
    /* All the stripes that are touched, should calculate & write the parity */
    for (int i = startStripe; i <= endStripe; i++) {
        int fileNum;
        int fileOffset;
        findParityBlock(i, &fileNum, &fileOffset);
        fseek(fds[fileNum], fileOffset, SEEK_SET);
        /* I m just writing PP for now, but complete data xoring should be done to calcualte the parity and same calcualted parity data should be passed here below */
        if (fwrite("PP", 1, BLOCK_SIZE, fds[fileNum]) != BLOCK_SIZE) {
            printf("\nCant write to the parity to file");
            error = -1;
            goto error;
        }

    }

error:
    for (int i = 0; i < RAID_DISKS; i++) {
        (void)fclose(fds[i]);
    }

    return error;
}



int raid5_write(int fd, const void* buffer, unsigned int count)
{
    openFileDescriptor_t *listfd;
    int error  = 0;

    LIST_FOREACH(listfd, &currentOpenFds, link)
    {
        if (listfd->fd == fd) {
            break;
        }
    }

    if (listfd == NULL) {
        error = -1;
        goto error;
    }
    listfd->block_count;
    /* Check if it was opened for write */
    if (listfd->flags & (O_WRONLY | O_RDWR | O_APPEND)) {
        int writeOffset = BLOCK_SIZE;
        if (listfd->flags & O_APPEND) {
            writeOffset = listfd->cursor;
        }
        error = raid5_write_int(listfd->fileName, writeOffset, buffer, count, listfd->block_count);
        if (!error) {
            if (listfd->flags & O_APPEND) {
                listfd->cursor += count;
            }
        }
    } else {
        error = -1;
    }

    return count;

error:
    return 0;
}
int raid5_create(char* filename)
{
    // Fail the call if file already exists.
    int flags= O_RDWR|O_APPEND|O_CREAT;
    if (flags & (O_CREAT | O_EXCL)) {
        for (int i = 0; i < RAID_DISKS; i++) {
            char raidFile[MAX_FILE_PATH_LENGTH] = {0};
            memcpy(raidFile, filename, strlen(filename));
            snprintf(raidFile+strlen(raidFile), MAX_FILE_PATH_LENGTH - strlen(raidFile), "%u", i);
            printf("\n Checking File %s", raidFile);
            if (access( raidFile, F_OK ) != -1) {
                return EEXIST;
            }
        }
    }
}
int raid5_open(char* filename, int flags)
{
    //int i = O_RDONLY; //, O_WRONLY, O_RDWR, O_APPEND, //O_CREAT, O_EXCL, and O_TRUNC
    struct stat  fileStat = {0};
    int          error    = -1;
    openFileDescriptor_t* fdData = NULL;


    /* Open the raid files where we will be saving our data */
    for (int i = 0; i < RAID_DISKS; i++) {
        char zero[BLOCK_SIZE] = {0};
        char raidFile[MAX_FILE_PATH_LENGTH] = {0};
        memcpy(raidFile, FILES_PATH, strlen(FILES_PATH));
        
        strcat(raidFile, filename);
        
        sprintf(raidFile+strlen(raidFile), "%u", i);

        printf("\n Opening File %s", raidFile);
        int filedesc = open(raidFile, O_CREAT | O_RDWR);
        if (filedesc == -1) {
            error = -1;
            goto error;
        }

        if (write(filedesc, zero, sizeof(zero) != sizeof(zero))) {
            printf("\n Failed to write the init_data into %s", raidFile);
        }

        if (close(filedesc) == -1) {
            error = -1;
            goto error;
        }
    }

    fdData = (openFileDescriptor_t*) malloc(sizeof(openFileDescriptor_t));
    if (fdData == NULL) {
        //error = ENOMEM;
        goto error;
    }

    bzero(fdData, sizeof(openFileDescriptor_t));
    memcpy(fdData->fileName, filename, strlen(filename));
    fdData->flags = flags;
    fdData->block_count=0;

    
    fdData->fd = getFileDescriptor();
    fdData->cursor = BLOCK_SIZE-2;
    LIST_INSERT_HEAD(&currentOpenFds, fdData, link);

    return fdData->fd;

error:

    for (int i = 0; i < RAID_DISKS; i++) {
        char raidFile[MAX_FILE_PATH_LENGTH] = {0};
        memcpy(raidFile, filename, strlen(filename));
        snprintf(raidFile+strlen(raidFile), MAX_FILE_PATH_LENGTH - strlen(raidFile), "%u", i);
        printf("\n Deleting File %s", raidFile);
        (void)remove(raidFile);
    }

    if (fdData != NULL) {
        free(fdData);
    }

    return error;
}
void raid5_read_int(const char* fileName)
{
    for (int i = 0; i < RAID_DISKS; i++) {
        char  raidFile[MAX_FILE_PATH_LENGTH] = {0};
        memcpy(raidFile, FILES_PATH, strlen(FILES_PATH));
        strcat(raidFile, fileName);
        sprintf(raidFile+strlen(raidFile), "%u", i);

        
    } 
    pthread_t thread1,thread2,thread3;
    int ret1,ret2,ret3;
    int count=1;
    memset(&buffer1, 0, sizeof(buffer1));

    

    struct files f;
    f.count=3;
    
    f.descriptors = (int*)malloc(sizeof(int) * f.count);
    f.descriptors[0]=open("file.txtfile.txt0",O_RDONLY);
    f.descriptors[1]=open("file.txtfile.txt1",O_RDONLY);
    f.descriptors[2]=open("file.txtfile.txt2",O_RDONLY);

    int i = 0;

    do
    {
        printf ("\nInside %d\n", i);

        ret1=pthread_create(&thread1,NULL,myfunc1,&f);
        ret2=pthread_create(&thread2,NULL,myfunc2,&f);
        ret3=pthread_create(&thread3,NULL,myfunc3,&f);

        pthread_join(thread1,NULL);
        pthread_join(thread2,NULL);
        pthread_join(thread3,NULL);

        i++;
    }while (i < count_block);

    
    
  
}
int raid5_read(int fd)
{
    openFileDescriptor_t *listfd;
    int error  = 0;

    LIST_FOREACH(listfd, &currentOpenFds, link)
    {
        if (listfd->fd == fd) {
            break;
        }
    }

    if (listfd == NULL) {
        error = -1;
        goto error;
    }

    /* Check if it was opened for write */
    if (listfd->flags & (O_WRONLY | O_RDWR | O_APPEND)) {
        error=-1;
        goto error;
        }
    else {
       
        raid5_read_int(listfd->fileName);
    }

   

error:
    return 0;
}

int main()
{
    LIST_INIT(&currentOpenFds);
    raid5_create((char*)"file.txt");
    int fd = raid5_open((char*)"file.txt", O_RDWR|O_APPEND|O_CREAT);
    if (fd < 0) {
        printf("\nOpen Failed");
    }

    if (raid5_write(fd, "1234567890", 10) != 10) {
        printf("\nWrite Failed");
    }
    
    
    int fd_read = raid5_open((char*)"file.txt", O_RDONLY|O_CREAT);
    raid5_read(fd_read);
    printf("\n BUFF OUT %s",buffer1);
    printf("\n");
    return 0;
}

