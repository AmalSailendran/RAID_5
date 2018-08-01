The project simulates RAID 5 - redundant array of independent disks configuration. 

Commands to run in order in Linux terminal:

1. raidimport 1.txt 2.txt - This application imports the content of a regular Linux file srcfile (which will contain data inputted by the user. For this instance, 1.txt will contain data inputed by you (eg: 123456abcde in 1.txt)) into your RAID-5 file system, using raidfile as the destination name. It will call the standard Linux file system APIs to read the content of srcfile, and call your RAID-5 APIs to write the content to your soft RAID-5. (Note: Please execute this code twice as only on the second execution the correct parity is calculated and data along with parity is written to the RAID file and console. First execution results in incorrect data storage for a reason not aware of).

2. raidexport 2.txt 3.txt - This application exports a RAID-5 file raidfile to a regular Linux file dstfile. It reads the content of raidfile using raid5_read and writes it into dstfile using the standard Linux file system write API.

Note 1: 
To compile the code use the following:
  1. make -f makefile.mk clean
  2. make -f makefile.mk
  
Note 2:
The raidimport and raidexport commands were created using source raid5_soft_commands.sh - The shell script contains two functions (function raidimport() - invokes ./raid5_soft $1 $2 1 where 1 signifies import, function raidexport() - invokes ./raid5_soft $1 $2 2 where 2 signifies export).


Main RAID 5 APIs Used:

1. int raid5_create(char* filename).
2. int raid5_open(char* filename, int flags).
3. int raid5_write(int fd, const void* buffer, unsigned int count).
4. int raid5_write_int(const char* fileName, int offset, const void* buffer, unsigned int length, unsigned int blockc, openFileDescriptor_t *listfd).
5. int raid5_read(int fd,  void* buffer, unsigned int count).
6. void raid5_read_int(const char* fileName, int blockc, void* buffer, unsigned int count).
7. int raid5_close(int fd).
