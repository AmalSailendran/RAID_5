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

int raid5_write_int(const char* fileName, int offset, const void* buffer, unsigned int length, unsigned int blockc,openFileDescriptor_t *listfd)
{
    int error = 0;
    count_block=blockc;
    
    openFileDescriptor_t *countfd;
    FILE* fds[RAID_DISKS] = {0};
    FILE* fdp[RAID_DISKS] = {0};
    FILE *f0,*f2;
    int      fileNum, fileOffset;
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
    listfd->block_count=count_block;
    /* All the stripes that are touched, should calculate & write the parity */
    for (int i = startStripe; i <= endStripe; i++) 
    {
        int fileNum;
        int ch,read_count=0;
        int fileOffset;
        int fileNumParity=fileNum;
        int fileOffsetParity=fileOffset;
        findParityBlock(i, &fileNum, &fileOffset);
        char p1[BLOCK_SIZE]; 
        char p2[BLOCK_SIZE];
        p1[BLOCK_SIZE]='\0';
        p2[BLOCK_SIZE]='\0';

        printf("filenum in parity calcualtion loop is : %d\n", fileNum);
        printf("fileoffset in parity calcualtion loop is : %d\n", fileOffset);
        if(fileNum==0)
        {
            printf("\nit's 1 & 2 combo!\n");
            
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
                else
                    printf("%s opened\n",raidFile );
            }
            fseek(fdp[1], fileOffset, SEEK_SET);
            fseek(fdp[2], fileOffset, SEEK_SET);
            fread(p1,sizeof(p1),1,fdp[1]);
            printf("\nhiii\n");
            printf("%s\n", p1);
            fread(p2,sizeof(p2),1,fdp[2]);
            printf("%s\n", p2);
            unsigned char xor[BLOCK_SIZE+1];
            xor[BLOCK_SIZE]='\0';
            unsigned char sprintfXOR[BLOCK_SIZE+1];
            sprintfXOR[BLOCK_SIZE]='\0';
            int i;

            for(i=0; i<BLOCK_SIZE; ++i)
                xor[i] = (char)(p1[i] ^ p2[i]);
            printf("\nxorrrrrrrrrr\n");
            for(i=0; i<BLOCK_SIZE; ++i)
            {
                printf("%X", xor[i]);
                
            }
            
            
        }
        else if(fileNum==1)
        {
            printf("\nit's 0 & 2 combo!\n");
            
            char  raidFile0[MAX_FILE_PATH_LENGTH] = {0};
            memcpy(raidFile0, FILES_PATH, strlen(FILES_PATH));
            strcat(raidFile0, fileName);
            sprintf(raidFile0+strlen(raidFile0), "%u", 0);

            f0 = fopen(raidFile0, "r");
            if (f0 == NULL) {
                error = -1;
                goto error;
            }
            else
                    printf("%s opened\n",raidFile0 );
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
                    printf("%s opened\n",raidFile2 );
            fseek(f0, fileOffset, SEEK_SET);
            fseek(f2, fileOffset, SEEK_SET);

            fread(p1,sizeof(p1),1,f0);
            printf("\nhiii\n");
            printf("%s\n", p1);
            fread(p2,sizeof(p2),1,f2);
            printf("%s\n", p2);
            unsigned char xor[BLOCK_SIZE+1];
            xor[BLOCK_SIZE]='\0';
            unsigned char sprintfXOR[BLOCK_SIZE+1];
            sprintfXOR[BLOCK_SIZE]='\0';
            int i;

            for(i=0; i<BLOCK_SIZE; ++i)
                xor[i] = (char)(p1[i] ^ p2[i]);
            printf("\nxorrrrrrrrrr\n");
            
            for(i=0; i<BLOCK_SIZE; ++i)
            {
                printf("%X", xor[i]);
                
            }
            
            


        }
        else
        {
            printf("\nit's 0 & 1 combo!\n");
            for (int i = 0; i < RAID_DISKS-1; i++) {
                char  raidFile[MAX_FILE_PATH_LENGTH] = {0};
                memcpy(raidFile, FILES_PATH, strlen(FILES_PATH));
                strcat(raidFile, fileName);
                sprintf(raidFile+strlen(raidFile), "%u", i);

                fdp[i] = fopen(raidFile, "r");
                if (fdp[i] == NULL) {
                    error = -1;
                    goto error;
                }
                else
                    printf("%s opened\n",raidFile );
            }
            fseek(fdp[0], fileOffset, SEEK_SET);
            fseek(fdp[1], fileOffset, SEEK_SET);
            fread(p1,sizeof(p1),1,fdp[0]);
            printf("\nhiii\n");
            printf("%s\n", p1);
            fread(p2,sizeof(p2),1,fdp[1]);
            printf("%s\n", p2);
            unsigned char xor[BLOCK_SIZE+1];
            xor[BLOCK_SIZE]='\0';
            unsigned char sprintfXOR[BLOCK_SIZE+1];
            sprintfXOR[BLOCK_SIZE]='\0';
            int i;

            for(i=0; i<BLOCK_SIZE; ++i)
                xor[i] = (char)(p1[i] ^ p2[i]);
            printf("\nxorrrrrrrrrr\n");
            for(i=0; i<BLOCK_SIZE; ++i)
            {
                printf("%X", xor[i]);
                
            }
            
            

        }
        
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
        error = raid5_write_int(listfd->fileName, writeOffset, buffer, count, listfd->block_count, listfd);
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
void raid5_read_int(const char* fileName,unsigned int blockc)
{
    printf("\ncount block is%d\n",count_block );
    int paritynum=(count_block/3)+3;
    int bytes= count_block*BLOCK_SIZE+paritynum;
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
    printf("\nread buffer with bytes and is: \n");
    for(int i=0;i<bytes;i++)
        printf("%c", buffer1[i] );
    free(f.descriptors);



    
    
  
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
       
        raid5_read_int(listfd->fileName,listfd->block_count);
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
    if (raid5_write(fd, "1234567890", 10) != 10) {
        printf("\nWrite Failed");
    }
    
    int fd_read = raid5_open((char*)"file.txt", O_RDONLY|O_CREAT);
    raid5_read(fd_read);
    
    printf("\n");
    return 0;
}

