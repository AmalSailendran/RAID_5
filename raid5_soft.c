
/******************************************************************
************************ RAID_5 SOFT ******************************
************************ AUTHOR : AMAL ****************************
*******************************************************************/

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
#include <math.h>

#define RAID_DISKS 3
#define FILES_PATH "file"
#define MAX_FILE_PATH_LENGTH 255
#define BLOCK_SIZE 2
#define bzero(b,len) (memset((b), '\0', (len)), (void) 0)
#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))
#define true 0
#define false 1
#define bool int

/*********** FD struct list node **********/
typedef struct openFileDescriptor_s
{
    int         fd;                                 /* file discriptor */
    int         cursor;                             /* offset used for write */
    char        fileName[MAX_FILE_PATH_LENGTH + 1]; /* FILENAME to store */
    int         flags;                              /* READ/WRITE flags */
    int         block_count;                        /* total block count used for storing data */
    LIST_ENTRY(openFileDescriptor_s) link;          /*  Next FD link */
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
    int blockcount[3];
};

/********* Multi Threads for read *********/
void *ProcessingThread1(void* myvar);
void *ProcessingThread2(void* myvar);
void *ProcessingThread3(void* myvar);

void *ProcessingThread1(void* myvar)
{
    struct files *f=(struct files*)myvar;
    //char *fd1="f1";

    if (0 == f->blockcount[0])
    {
        return NULL;
    }

    if(f->descriptors[0]==-1)
        printf("\nerror in opening file\n");

    //printf("\n offset1 to read : %d", offset1);

    if (0 == fread(buffer1+offset1, BLOCK_SIZE, 1, f->descriptors[0]))
    {
        return NULL;
    }
    
    offset1 += (BLOCK_SIZE * 3);

    f->blockcount[0] = f->blockcount[0] - 1;

    //printf("\n offset1 to next read : %d", offset1);

    return NULL;
}

void *ProcessingThread2(void* myvar)
{
    struct files *f=(struct files*)myvar;
    //char *fd2="f2";
    if (0 == f->blockcount[1])
    {
        return NULL;
    }

    if(f->descriptors[1]==-1)
        printf("\nerror in opening file\n");

    //printf("\n offset2 to read : %d", offset2);
    /*
    if(0 == read(f->descriptors[1],buffer1+offset2,2))
    {
        return NULL;
    }*/
    
    if (0 == fread(buffer1+offset2, BLOCK_SIZE, 1, f->descriptors[1]))
    {
        return NULL;
    }

    offset2 += (BLOCK_SIZE * 3);

    f->blockcount[1] = f->blockcount[1] - 1;

    //printf("\n offset2 to next read : %d", offset2);

    return NULL;
}
void *ProcessingThread3(void* myvar)
{
    struct files *f=(struct files*)myvar;
    
    if (0 == f->blockcount)
    {
        return NULL;
    }
    
    if(f->descriptors[2]==-1)
        printf("\nerror in opening file\n");

    //printf("\n offset3 to read : %d", offset3);
    
    /*if (0 == read(f->descriptors[2],buffer1+offset3,2))
    {
        return NULL;
    }*/

    if (0 == fread(buffer1 + offset3, BLOCK_SIZE, 1, f->descriptors[2]))
    {
        return NULL;
    }

    offset3 += (BLOCK_SIZE * 3);

    int b1 = 
    
    f->blockcount[2] = f->blockcount[2] - 1;

    //printf("\n offset3 next read : %d", offset3);

    return NULL;
}

/********* get the FD *******/
int getFileDescriptor() {
    static int fd = pow(28, 2);
    #if 0
    // handle the max fd logic 
    if (fd++ >= 2^30)
    {
        printf("Max FD reached");
        /* Find the free slot for FD and return it */
    }
    else
    {
        printf("Obtained valid FD");
        return fd;
    }
    #endif
    return fd++;
}

/********* Find the parity block *******/
void findParityBlock(int stripe, int *fileNum, int *fileOffset)
{
    *fileNum = stripe % RAID_DISKS;
    //printf("\nfildnum in fileParityBlock function: %d\n",*fileNum );
    *fileOffset  = BLOCK_SIZE * stripe;
    //printf("\nfileoffset in find parity block function: %d\n",*fileOffset );
}

/********* Find the data block *******/
int findDataBlock(int offset, int *fileNum, int *fileOffset)
{
    /*
     * One Stripe would contains BLOCK_SIZE * (RAID_DISKS - 1) application data,
     * as one block in each stripe is used for parity.
     */

    int dataInEachStripe = BLOCK_SIZE * (RAID_DISKS - 1);
    //printf("\ndataInEachStripe is: %d",dataInEachStripe);

    /* First block is our metadata. So compensate that to the offset application is asking */
    //offset += BLOCK_SIZE;

    int whichStripe = (offset + 1) / dataInEachStripe;
    //printf("\nwhichStripe is: %d",whichStripe);


    int offsetInStripe = (offset)  % dataInEachStripe;
    //printf("\noffsetInStripe is: %d",offsetInStripe);


    int whichBlock = offsetInStripe / BLOCK_SIZE;
    //printf("\nwhichBlock : %d",whichBlock);

    int parityFileNum;
    int parityOffset;
    findParityBlock(whichStripe, &parityFileNum, &parityOffset);

    if (whichBlock >= parityFileNum) 
    {
        whichBlock++;
    }

    *fileNum = whichBlock;
    //printf("\nfildnum in find data block function: %d\n",*fileNum );
    
    *fileOffset = whichStripe * BLOCK_SIZE;
    
    //printf("\nfileoffset in find data block function: %d\n",*fileOffset );
    return whichStripe;
}

int raid5_write_int(const char* fileName, int offset, const void* buffer, 
                    unsigned int length, openFileDescriptor_t *listfd)
{
    int error = 0;
    //openFileDescriptor_t *countfd;
    FILE* fds[RAID_DISKS] = {0};
    FILE* fdp[RAID_DISKS] = {0};
    FILE *f0,*f2;
    int      fileNum, fileOffset;
    int currOffset = offset;
    //printf("\noffset passed fromm write func: %d\n",currOffset);
    int remainingLength = length;
    //printf("\nlenght passed fromm write func: %d\n",remainingLength);
    char* data = (char*) buffer;
    //printf("\ndata in buffer is: %s\n",data);
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

        int      whichStripe = findDataBlock(currOffset, &fileNum, &fileOffset);
        int      actualOffset = fileOffset + (currOffset % BLOCK_SIZE);
        //printf("\ncurrent whichStripe from write func: %d\n",whichStripe );
        if (startStripe == -1) {
            startStripe = whichStripe;
        }

        endStripe = whichStripe;
        //printf("\nendstripe is: %d\n", endStripe);
        //printf("\nactual offset is: %d\n", actualOffset);
        int lengthToWriteInBlock;
        
        if ((actualOffset % BLOCK_SIZE) != 0) {
            lengthToWriteInBlock = MIN(BLOCK_SIZE - (actualOffset - fileOffset), remainingLength);
        } else 
        {
            lengthToWriteInBlock = MIN(BLOCK_SIZE, remainingLength);
        }

        fseek(fds[fileNum], actualOffset, SEEK_SET);
        if (fwrite(data, 1, lengthToWriteInBlock, fds[fileNum]) != lengthToWriteInBlock) 
        {
            //printf("Cant write to the file %d bytes", lengthToWriteInBlock);
            error = -1;
            goto error;
        }

        remainingLength -= lengthToWriteInBlock;
        currOffset += lengthToWriteInBlock;
        data += lengthToWriteInBlock;
        listfd->block_count++;

    } while (remainingLength > 0);


    //printf("\nblock count is : %d", listfd->block_count);
    
    //listfd->block_count=count_block;
    
    /* All the stripes that are touched, should calculate & write the parity */
    for (int i = startStripe; i <= endStripe; i++)
    {
        int fileNum;
        //int ch;
        //int read_count=0;
        int fileOffset;
        //int fileNumParity=fileNum;
        //int fileOffsetParity=fileOffset;
        findParityBlock(i, &fileNum, &fileOffset);
        char p1[BLOCK_SIZE] = {0};
        char p2[BLOCK_SIZE] = {0};
        //p1[BLOCK_SIZE]='\0';
        //p2[BLOCK_SIZE]='\0';
        unsigned char xor[BLOCK_SIZE] = {0};
        
        //printf("filenum in parity calcualtion loop is : %d\n", fileNum);
        //printf("fileoffset in parity calcualtion loop is : %d\n", fileOffset);
        
        if(fileNum==0)
        {
            //printf("\nit's 1 & 2 combo!\n");

            for (int i = 1; i < RAID_DISKS; i++) {
                char  raidFile[MAX_FILE_PATH_LENGTH] = {0};
                memcpy(raidFile, FILES_PATH, strlen(FILES_PATH));
                strcat(raidFile, fileName);
                sprintf(raidFile+strlen(raidFile), "%u", i);

                fdp[i] = fopen(raidFile, "r");
                if (fdp[i] == NULL) {
                    error = -1;
                    goto error;
                }
                //else
                    //printf("%s opened\n",raidFile );
            }
            
            fseek(fdp[1], fileOffset, SEEK_SET);
            fseek(fdp[2], fileOffset, SEEK_SET);
            fread(p1,sizeof(p1),1,fdp[1]);
            
            //printf("%s\n", p1);
            
            fread(p2,sizeof(p2),1,fdp[2]);
            //printf("%s\n", p2);
            //xor[BLOCK_SIZE]='\0';

            int i;

            for(i=0; i<BLOCK_SIZE; ++i)
                xor[i] = (char)(p1[i] ^ p2[i]);
            //printf("\nxorrrrrrrrrr\n");
            
            for(i=0; i<BLOCK_SIZE; ++i)
            {
                printf("%X", xor[i]);

            }
        }
        else if(fileNum==1)
        {
            //printf("\nit's 0 & 2 combo!\n");

            char  raidFile0[MAX_FILE_PATH_LENGTH] = {0};
            memcpy(raidFile0, FILES_PATH, strlen(FILES_PATH));
            strcat(raidFile0, fileName);
            sprintf(raidFile0+strlen(raidFile0), "%u", 0);

            f0 = fopen(raidFile0, "r");
            if (f0 == NULL) {
                error = -1;
                goto error;
            }
            
            
            //printf("%s opened\n",raidFile0 );
            char  raidFile2[MAX_FILE_PATH_LENGTH] = {0};
            memcpy(raidFile2, FILES_PATH, strlen(FILES_PATH));
            strcat(raidFile2, fileName);
            sprintf(raidFile2+strlen(raidFile2), "%u", 2);

            f2 = fopen(raidFile2, "r");
            if (f2 == NULL) {
                error = -1;
                goto error;
            }
            else
                    //printf("%s opened\n",raidFile2 );
            fseek(f0, fileOffset, SEEK_SET);
            fseek(f2, fileOffset, SEEK_SET);

            fread(p1,sizeof(p1),1,f0);
            //printf("%s\n", p1);
            fread(p2,sizeof(p2),1,f2);
            //printf("%s\n", p2);
            //xor[BLOCK_SIZE]='\0';

            int i;

            for(i=0; i<BLOCK_SIZE; ++i)
                xor[i] = (char)(p1[i] ^ p2[i]);
            //printf("\nxorrrrrrrrrr\n");

            //for(i=0; i<BLOCK_SIZE; ++i)
            //{
              //  printf("%X", xor[i]);

            //}
        }
        else
        {
            //printf("\nit's 0 & 1 combo!\n");
            for (int i = 0; i < RAID_DISKS-1; i++) 
            {
                char  raidFile[MAX_FILE_PATH_LENGTH] = {0};
                memcpy(raidFile, FILES_PATH, strlen(FILES_PATH));
                strcat(raidFile, fileName);
                sprintf(raidFile+strlen(raidFile), "%u", i);

                fdp[i] = fopen(raidFile, "r");
                if (fdp[i] == NULL) {
                    error = -1;
                    goto error;
                }
                //else
                    //printf("%s opened\n",raidFile );
            }
            fseek(fdp[0], fileOffset, SEEK_SET);
            fseek(fdp[1], fileOffset, SEEK_SET);
            fread(p1,sizeof(p1),1,fdp[0]);
            //printf("\nhiii\n");
            //printf("%s\n", p1);
            fread(p2,sizeof(p2),1,fdp[1]);
            //printf("%s\n", p2);
            
            //xor[BLOCK_SIZE]='\0';

            int i;

            for(i=0; i<BLOCK_SIZE; ++i)
                xor[i] = (char)(p1[i] ^ p2[i]);
            //printf("\nxorrrrrrrrrr\n");
            /*for(i=0; i<BLOCK_SIZE; ++i)
            {
                printf("%X", xor[i]);

            }*/
        }

        fseek(fds[fileNum], fileOffset, SEEK_SET);
        
        /* I m just writing PP for now, but complete data xoring should be done to calcualte the parity and same calcualted parity data should be passed here below */
        if (fwrite(xor, 1, BLOCK_SIZE, fds[fileNum]) != BLOCK_SIZE) {
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

/**************************************************************************************************
int raid5_write(int fd, const void* buffer, unsigned int count), return the number of bytes written, 
or -1 if an error occurred. V
****************************************************************************************************/
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

    /* Check if it was opened for write */
    if (listfd->flags & (O_WRONLY | O_RDWR | O_APPEND)) {
        int writeOffset = BLOCK_SIZE;
        if (listfd->flags & O_APPEND) {
            writeOffset = listfd->cursor;
        }
        error = raid5_write_int(listfd->fileName, writeOffset, buffer, count, listfd);
        if (!error) {
            if (listfd->flags & O_APPEND) {
                listfd->cursor += count;
            }
        }
    } 
    else 
    {
        error = -1;
    }

    return count;

error:
    return 0;
}

/*******************************************************************************************
int raid5_create(char* filename),Create the disk (internal file) for storing blocks
********************************************************************************************/
int raid5_create(char* filename)
{
    // Fail the call if file already exists.
    int flags= O_RDWR|O_APPEND|O_CREAT;
    if (flags & (O_CREAT | O_EXCL)) {
        for (int i = 0; i < RAID_DISKS; i++) {
            char raidFile[MAX_FILE_PATH_LENGTH] = {0};
            memcpy(raidFile, filename, strlen(filename));
            snprintf(raidFile+strlen(raidFile), MAX_FILE_PATH_LENGTH - strlen(raidFile), "%u", i);
            //printf("\n Checking File %s", raidFile);
            if (access( raidFile, F_OK ) != -1) {
                return EEXIST;
            }
        }
    }

    return 0;
}

/**********************************************************************************************
int raid5_open(char* filename, int flags), return the new file descriptor, or -1 if an error 
***********************************************************************************************/
int raid5_open(char* filename, int flags)
{
    //int i = O_RDONLY; //, O_WRONLY, O_RDWR, O_APPEND, //O_CREAT, O_EXCL, and O_TRUNC
    //struct stat  fileStat = {0};
    int          error    = -1;
    openFileDescriptor_t* fdData = NULL;


    /* Open the raid files where we will be saving our data */
    for (int i = 0; i < RAID_DISKS; i++) {
        char zero[BLOCK_SIZE] = {0};
        char raidFile[MAX_FILE_PATH_LENGTH] = {0};
        memcpy(raidFile, FILES_PATH, strlen(FILES_PATH));

        strcat(raidFile, filename);

        sprintf(raidFile+strlen(raidFile), "%u", i);

        //printf("\n Opening File %s", raidFile);
        int filedesc = open(raidFile, O_CREAT | O_RDWR,0600);
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

    //printf("fdData->fd : %d", fdData->fd);

    fdData->cursor = BLOCK_SIZE-2;
    LIST_INSERT_HEAD(&currentOpenFds, fdData, link);

    return fdData->fd;

error:

    for (int i = 0; i < RAID_DISKS; i++) 
    {
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

void raid5_read_int(const char* fileName, int blockc, void* buffer, unsigned int count)
{
    //printf("\n %%%%%%%%%%%%%%%%%%%%%%%%%% raid5_read_int count block is  %d\n",blockc );
        
    pthread_t thread1,thread2,thread3;
    int ret1,ret2,ret3;
    //int count=1;
    bzero(&buffer1, sizeof(buffer1));

    struct files f;
    f.count = RAID_DISKS;

    f.descriptors = (int*)malloc(sizeof(int) * f.count);
    long fsize;
    int block_count = 0;


    for (int i = 0; i < RAID_DISKS; i++) 
    {
        char  raidFile[MAX_FILE_PATH_LENGTH] = {0};
        memcpy(raidFile, FILES_PATH, strlen(FILES_PATH));
        strcat(raidFile, fileName);
        sprintf(raidFile+strlen(raidFile), "%u", i);

        //printf("\r\n %%%%%%%%%%%%%%% : raidFile %s", raidFile);

        //f.descriptors[i]=open(raidFile,O_RDONLY);
        f.descriptors[i] = fopen(raidFile, "r+");

        if (0 == blockc)
        {
            fseek(f.descriptors[i], 0, SEEK_END);
            fsize = ftell(f.descriptors[i]);
            fseek(f.descriptors[i], 0, SEEK_SET);
            
            //fsize = 10;
            //printf("\r\n %%%%%%%%%%%%%%% : fsize %d", fsize);
            f.blockcount[i] = fsize/BLOCK_SIZE;
            //printf("\r\n %%%%%%%%%%%%%%% : f.blockcount[i] %d", f.blockcount[i]);
            block_count += f.blockcount[i];  
            //printf("\r\n %%%%%%%%%%%%%%% : block_count %d", block_count);
        }
    }

    //printf("block_count : %d", block_count);

    if (0 != blockc)
    {
        block_count = blockc;
    }

    int paritynum = (block_count / RAID_DISKS) + RAID_DISKS;
    int bytes     = (block_count * BLOCK_SIZE) + paritynum;

    int i = 0;

    int threadc = (block_count / 3);

    do
    {
        //printf ("\nInside %d\n", i);

        ret1 = pthread_create(&thread1,NULL,ProcessingThread1,&f);
        ret2 = pthread_create(&thread2,NULL,ProcessingThread2,&f);
        ret3 = pthread_create(&thread3,NULL,ProcessingThread3,&f);

        //printf("\n Thread create ret code: %d %d %d \n",ret1, ret2, ret3);

        pthread_join(thread1,NULL);
        pthread_join(thread2,NULL);
        pthread_join(thread3,NULL);

        i++;
    }while (i <= threadc);

    printf("\r\n ###########################################");

    printf("\r\n !!!! DATA READ FROM RAID5 WITH PARITY !!!!!: \n");

    printf("\n   HEX ---> CHAR");

    char databuff[100] = {0};
    bzero(databuff, sizeof(databuff));

    int c = 0;
    int j = 0;

    for(int i=0;i<bytes;i++)
    {
        printf("\r\n   %x ---> %c", buffer1[i], buffer1[i] );

        /*
        if (buffer1[i]>=0 && buffer1[i] <128)
        {
            databuff[j] = buffer1[i];
            j++;
            printf("\r\n ---------- databuff[i] : %c", databuff[i]);
            c++;
        }*/
    }
    
    memcpy(buffer, buffer1, count);

    for (int i = 0; i < RAID_DISKS; i++) 
    {
        (void)close(f.descriptors[i]);
    }

    free(f.descriptors);

    printf("\r\n ###########################################");
}

/*************************************************************************
int raid5_read(int fd, void* buffer, unsigned int count), 
return the number of bytes read, or -1 if an error occurred.
*************************************************************************/
int raid5_read(int fd,  void* buffer, unsigned int count)
{
    //printf("\r\n ##### raid read begin #######");
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
        printf("##### raid read err listfd null #######");
        goto error;
    }

    /* Check if it was opened for write */
    if (listfd->flags & (O_WRONLY | O_RDWR | O_APPEND)) {
        error=-1;
        goto error;
        }
    else {

        raid5_read_int(listfd->fileName, listfd->block_count, buffer, count);
    }
error:
    return error;
}

/**************************************************************************
int raid5_close(int fd), return 0 on success, or -1 if an error occurred. V
***************************************************************************/
int raid5_close(int fd)
{
    openFileDescriptor_t *listfd;
    int error  = 0;

    LIST_FOREACH(listfd, &currentOpenFds, link)
    {
        if (listfd->fd == fd)
        {
            break;
        }
    }

    if (listfd == NULL) {
        error = -1;
        goto error;
    }

    //printf("\r\n FD removed and all files cleared");

    /* Free the fd from FD list */
    LIST_REMOVE(listfd, link);
    free(listfd);

error:
    return error;
}

/*
* Checks to see if the given filename is 
* a valid file
*/
bool isValidFile(char *filename) {
    // We assume argv[1] is a filename to open
    int fd;

    fd = open(filename,O_RDWR|O_CREAT,0644);
    //printf("fd = %d\n", fd);
    if (fd == -1) 
    {
        return false;
    }
    close(fd);
    return true;
    /* fopen returns 0, the NULL pointer, on failure */
}

int main(int argc, char* argv[])
{
    int CmdType;
    
    LIST_INIT(&currentOpenFds);
    
    //printf("\r\n c0 argc : %d", argc);

    /************************** Command Processing *******************************/
    if (argc != 0)
    {
        if ((argc != 0) && (argc != 4))
        {
            printf("Invalid Argument");
            return 0;
        }

        /*for (int i = 1; i < (argc - 1); i++) 
        {
            printf("\r\n argv[%u] = %s\n", i, argv[i]);
        }*/

        if ((strlen(argv[1]) >= MAX_FILE_PATH_LENGTH || strlen(argv[2]) >= MAX_FILE_PATH_LENGTH))
        {            
            printf("Invalid File");
            return 0;
        }

        char srcFile[MAX_FILE_PATH_LENGTH] = {0};
        char raidFile[MAX_FILE_PATH_LENGTH] = {0};
        char dstfile[MAX_FILE_PATH_LENGTH] = {0};

        CmdType = *(char *)argv[3];
        //printf("CmdType : %d", CmdType);

        /*******************************
         raid import command processing 
         raidimport srcfile raidfile:
         ********************************/
        if (49 == CmdType)
        {
            memcpy(srcFile, argv[1], strlen(argv[1]));
            memcpy(raidFile, argv[2], strlen(argv[2]));

            if (false == isValidFile(argv[1]))
            {
                printf("\r\n file not present : %s\r\n", argv[1]);
                return 0;
            }

            FILE *fp;

            fp = fopen(srcFile, "r"); // read mode

            if (fp == NULL)
            {
                printf("Error while opening the file.\n");
                return 0;
            }

            /********* raid writing *********/

            raid5_create((char*)raidFile);

            int fd = raid5_open((char*)raidFile, O_RDWR|O_APPEND|O_CREAT);
            if (fd < 0) 
            {
                printf("\nOpen Failed");
                return 0;
            }

            char ch;
            
            fseek(fp, 0, SEEK_END);
            long fsize = ftell(fp);
            fseek(fp, 0, SEEK_SET);  //same as rewind(f);

            char *stringbuff = malloc(fsize + 1);
            fread(stringbuff, fsize, 1, fp);
            fclose(fp);

            stringbuff[fsize] = 0;

            //printf("%s buff ", stringbuff);
            //printf("%d size", fsize);

            if (raid5_write(fd, stringbuff, fsize) != fsize) 
            {
                printf("\nWrite Failed");
            }  
            
            free(stringbuff);
            /*fclose(fp);*/
            raid5_close(fd);
        }
        /*******************************
         raid export command processing 
        raidexport raidfile, dstfile
        **********************************/
        else if (50 == CmdType)
        {
            memcpy(raidFile, argv[1], strlen(argv[1]));
            memcpy(dstfile, argv[2], strlen(argv[2]));


            int fd = raid5_open((char*)raidFile, O_RDONLY|O_CREAT);
            if (fd < 0) 
            {
                printf("\nOpen Failed");
                return 0;
            }

            /*read 100 bytes */
            char *readbufffile = malloc(100);
            readbufffile[100] = '\0';

            
            raid5_read(fd, readbufffile, 100);

            FILE *fp1;

            //printf("###################### readbufffile : %s", readbufffile);

            fp1 = fopen(dstfile, "wb"); // writw mode

            if (fp1 == NULL)
            {
                printf("Error while opening the file.\n");
                return 0;
            }
    
            fwrite(readbufffile, 1, sizeof(readbufffile), fp1); 

            free(readbufffile);
            raid5_close(fd);
            fclose(fp1);
        }
        else
        {
            printf("Invalid type");
        }

        //printf("\r\n srcFile : %s ", srcFile);
        //printf("\r\n dstfile : %s ", dstfile);
        //printf("\r\n raidFile : %s ", raidFile);

        /* Need write the code here */

    }
    /**************************************************************/

    #if 0
    raid5_create((char*)"file.txt");
    int fd = raid5_open((char*)"file.txt", O_RDWR|O_APPEND|O_CREAT);
    if (fd < 0) 
    {
        printf("\nOpen Failed");
    }

    if (raid5_write(fd, "1234567890", 10) != 10) {
        printf("\nWrite Failed");
    }
    if (raid5_write(fd, "1234567890", 10) != 10) {
        printf("\nWrite Failed");
    }

    int fd_read = raid5_open((char*)"file.txt", O_RDONLY|O_CREAT);

    /*read 100 bytes */
    char *readbuff = malloc(100);
    readbuff[100] = '\0';

    
    raid5_read(fd_read, readbuff, 100);

    printf("###################### Buff : %s", readbuff);
    
    free(readbuff);
    raid5_close(fd_read);

    #endif
    printf("\n");
    return 0;
}