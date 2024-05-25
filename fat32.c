//-----------------------------------------
// NAME: Chowdhury Abdul Mumin Ishmam 
// STUDENT NUMBER: 7854067
// COURSE: COMP 3430, SECTION: A01
// INSTRUCTOR: Saulo Santos
// ASSIGNMENT: assignment 3, QUESTION: 1: Reading a fat-32 volume and implement 3 functions.
//
// REMARKS: Implement 3 functions, namely, info: which gets the information of the volume, list: listing
// the files and directories in the fat-32 volume and, get: getting a file from the volume and fetch it into
// the same folder of the program. 
//-----------------------------------------
#define _FILE_OFFSET_BITS 64
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <string.h>
#include <ctype.h>
#include "fat32.h"

#define ATTR_LONG_NAME_MASK	ATTR_READ_ONLY |ATTR_HIDDEN |ATTR_SYSTEM |ATTR_VOLUME_ID |ATTR_DIRECTORY |ATTR_ARCHIVE
#define LAST_LONG_ENTRY 0x40
#define MAX_PRINTED_FILES 100

//Structs for FS Info structure and FAT 32 directory entry
typedef struct Fsinfo fsinfo;
typedef struct DirInfo fat32DE;

//Struct for a Long Directory entry
struct LDIR_Info {
    uint8_t LDIR_Ord;          // Order of the long-name entry in the sequence
    uint16_t LDIR_Name1[5];    // Characters 1-5 of the long-name sub-component
    uint8_t LDIR_Attr;         // Attributes (Attribute is LONG NAME)
    uint8_t LDIR_Type;         // Type (0 for directory entry, reserved for future extensions)
    uint8_t LDIR_Chksum;       // Checksum of the corresponding short dir entry
    uint16_t LDIR_Name2[6];    // Characters 6-11 of the long-name sub-component
    uint16_t LDIR_FstClusLO;   // Must be zero
    uint16_t LDIR_Name3[2];    // Characters 12-13 of the long-name sub-component 
};
typedef struct LDIR_Info fat32LDE;

//Global Variables

//Variable to hold the name of the volume image
char* volume_img;
//Variable to hold the name of the functions(info/list/get)
char* functions;
//The file descriptor for the image
int fd; 
//Declaration of the Boot Sector struct variable  
fat32BS bs;
////Declaration of the FS info structure struct variable  
fsinfo fs_info;
//A char array to hold the name of the OEM 
char OEM_name[BS_OEMName_LENGTH] = {'\0'};
//A char array to hold the name of the Volume Label
char vol_lab[BS_VolLab_LENGTH] = {'\0'};
//A variable for number of directory entries per cluster
uint32_t directory_per_cluster;
//A flag used in the get function to indicate whether we got the desired file. O mean fail, and 1 success
int gotFile = 0;
//An array to hold the name of a file in the volume
char* file;
//Another file descriptor, needed for the get function to write a file contents
int new_fd;
//An unsigned char array to hold the Long File Name
unsigned char long_name[255];
//An array to keep track of the printed files in the list function, so that they are printed once
char printed_filenames[MAX_PRINTED_FILES][13]; // Assuming filenames are limited to 12 characters plus the null terminator
//Counter for the number of files printed
int num_printed_files = 0;

//Function prototypes
//Info
void read_and_validate_boot_sector(int);
void free_clusters_count(int);
void info();
//List
void print_dash(int);
int validate_directory(char *);
char* getName(const char *);
char* insertDot(const char *);
void list(uint32_t , int);
//Get
void tokenize_path(char* );
void get_file_contents(uint32_t);
void get(uint32_t , int , char* );

//-----------------------------------------------------------------------------
// read_and_validate_boot_sector(int handle)
//
// PURPOSE: To read in the metadata of the volume, namely, Main Boot Sector and validate  
//          certain conditions that must hold true for a fat-32 volume image
// INPUT PARAMETERS:
//     handle: An file descriptor is passed as a parameter for reading the
//             volume.
// OUTPUT PARAMETERS:
//     None
//-----------------------------------------------------------------------------
void read_and_validate_boot_sector(int handle)
{
    //First we read in the jump boot
    read(handle, &bs.BS_jmpBoot, 3);
    //Validating the jmpboot
    assert((uint8_t)(bs.BS_jmpBoot[0]) == (0xEB | 0xE9));

    //Read in oem name and put in an array to print for info function
    read(handle, &bs.BS_OEMName, 8);

    //read in bytes per sector
    read(handle, &bs.BPB_BytesPerSec, 2);

    //read in sectors per cluster
    read(handle, &bs.BPB_SecPerClus, 1);
    
    //read in the reserved sector count
    read(handle, &bs.BPB_RsvdSecCnt, 2);
    //read in number of fats
    read(handle, &bs.BPB_NumFATs, 1);
    //read in root entry count
    read(handle, &bs.BPB_RootEntCnt, 2);
    assert(bs.BPB_RootEntCnt == 0);

    //Seek the 16-bit and some unnecessary info
    lseek(handle, 3, SEEK_CUR);
    //read in the fat size of 16 bit for validation check
    read(handle, &bs.BPB_FATSz16, 2);
    if(bs.BPB_FATSz16 != 0)
    {
        assert(bs.BPB_FATSz16 == 0);
        printf("This is not a 32-bit file. Please provide a 32 bit file\n");
        exit(EXIT_FAILURE);
    }

    //SEEK more unnecessary info
    lseek(handle, 8, SEEK_CUR);

    //read in the total count of sectors in 32 bit file
    read(handle, &bs.BPB_TotSec32, 4);
    assert(bs.BPB_TotSec32 != 0);

    //read in the fat size, number of sectors in 1 FAT
    read(handle, &bs.BPB_FATSz32, 4);

    //Seek unnecessary info
    lseek(handle, 4, SEEK_CUR);

    //Read in the root cluster number/ cluster number of the first cluster of root directory
    read(handle, &bs.BPB_RootClus, 4);
    assert(bs.BPB_RootClus >= 2);

    //read in the sector number of FSINFO structure in the reserved area
    read(handle, &bs.BPB_FSInfo, 2);

    //read in the backup boot sector
    read(handle, &bs.BPB_BkBootSec, 2);

    //read in the reserved 
    read(handle, &bs.BPB_reserved, 12);
    //Validation check for reverved bytes, all must be 0 for a 32-bit file
    for(int i = 0; i < 12; i++)
    {
        assert((uint8_t)(bs.BPB_reserved[i]) == 0);
    }

    //Seek drvnum and reserved1
    lseek(handle, 2, SEEK_CUR);

    //read in the boot signature and validate it
    read(handle, &bs.BS_BootSig, 1);
    assert(bs.BS_BootSig == 0x29);

    //Seek the volume id
    lseek(handle, 4, SEEK_CUR);

    //read in the volume label and put into an array for printing in info
    read(handle, &bs.BS_VolLab, 11);
    for(int i = 0; i < BS_VolLab_LENGTH; i++)
    {
        vol_lab[i] = bs.BS_VolLab[i];
    }
    
    //Each directory entry is 32 bytes so, number of directory entry per cluster would be
    directory_per_cluster = (bs.BPB_SecPerClus*bs.BPB_BytesPerSec)/32;
    //printf("\nDirectories per cluster: %u\n", directory_per_cluster);

    /*
    //This is taken from the lab
    lseek(handle, 0, SEEK_SET);    
    //Seek to the end of reserved sectors to read in the FAT
    lseek(handle, bs.BPB_RsvdSecCnt * bs.BPB_BytesPerSec, SEEK_CUR);

    //Validates the first two entries of the FAT
    uint32_t FAT[2];
    read(handle, &FAT[0], 4);
    read(handle, &FAT[1], 4);
    //Check of the first 2 entries of the fat for validation
    uint32_t Fat0Check = FAT[0] & EOC;
    uint32_t Fat1Check = FAT[1] & EOC;
    assert(Fat0Check == 0x0FFFFFF8);
    assert(Fat1Check == 0x0FFFFFFF);
    */
}

//-----------------------------------------------------------------------------
// free_clusters_count(int handle)
//
// PURPOSE: Read the FS info structure and calculate the last know free clusters count.
//          This would give us the free space on the volume for the info function. 
//          Also validates the FS Info Signatures
// INPUT PARAMETERS:
//     handle: An file descriptor is passed as a parameter for reading the
//             volume.
// OUTPUT PARAMETERS:
//     None
//-----------------------------------------------------------------------------
void free_clusters_count(int handle)
{
    //Seek to the start of the volume
    lseek(handle, 0, SEEK_SET);

    //Now we go to the FSinfo structure
    lseek(handle, bs.BPB_FSInfo*bs.BPB_BytesPerSec, SEEK_CUR);

    read(handle, &fs_info.FSI_LeadSig, 4);
    //Validate the lead signature that it is infact an Fsinfo structure
    assert(fs_info.FSI_LeadSig == 0x41615252);

    //Seek the reserved parts as its not needed
    lseek(handle, 480, SEEK_CUR);
    //Read in another signature and validate it
    read(handle, &fs_info.FSI_StrucSig, 4);
    assert(fs_info.FSI_StrucSig == 0x61417272);

    //The last known free cluster count
    read(handle, &fs_info.FSI_Free_Count, 4);

    //Seek to the start of the volume
    lseek(handle, 0, SEEK_SET);
}

//-----------------------------------------------------------------------------
// info()
//
// PURPOSE: This is the info function, which prints out the information of the fat-32 
//          volume. Such as, the drive name, free space on the volume, Cluster size, etc.
// INPUT PARAMETERS:
//     None
// OUTPUT PARAMETERS:
//     None
//-----------------------------------------------------------------------------
void info()
{
    //Prints out the drive name including the volume label and the OEM name
    printf("\nDrive name:\n");
    printf("\tVolume Label: %s\n", vol_lab);
    printf("\tOEM Name: %s\n", bs.BS_OEMName);

    //Free space on the drive
    printf("Free space on the drive: %u KBs\n", (fs_info.FSI_Free_Count*bs.BPB_SecPerClus*bs.BPB_BytesPerSec)/1024);
    //Amount of usable space
    uint32_t data_sectors = bs.BPB_TotSec32 -(bs.BPB_RsvdSecCnt + (bs.BPB_NumFATs * bs.BPB_FATSz32));
    //printf("\nSize of data sectors: %u\n", data_sectors);

    //Amount of usable space on the drive
    uint32_t usable_space = (data_sectors * bs.BPB_BytesPerSec)/1024;
    printf("Usable storage on the drive: %u KB\n", usable_space);

    //Print cluster size in sectors and KBs
    printf("Cluster Size: %u sectors, %f KBs (%u bytes)\n", bs.BPB_SecPerClus, (double)(bs.BPB_SecPerClus*bs.BPB_BytesPerSec)/1024, bs.BPB_BytesPerSec*bs.BPB_SecPerClus);
}
//-----------------------------------------------------------------------------
// print_dash(int level)
//
// PURPOSE: Prints a '-' character depending on the depth of a folder starting
//          from root. 
// INPUT PARAMETERS:
//     level:  number of - characters as the depth of the file/folder. 
// OUTPUT PARAMETERS:
//     None
//-----------------------------------------------------------------------------
void print_dash(int level)
{
    for(int i = 0; i < level; i++)
    {
        printf("-");
    }
}

//-----------------------------------------------------------------------------
// validate_directory(char *dir_name)
//
// PURPOSE: Validates the directory entry, that only allowed attributes are passed
//          through to next iterations. Used for checking in list function
// INPUT PARAMETERS:
//     dir_name: Takes a pointer to a specific directory name and according to the
//              hardware white paper, only valid characters are allowed in the name
// OUTPUT PARAMETERS:
//     int: A flag to determine whether the directory is valid or not, 0 mean fails
//          and 1 success. 
//-----------------------------------------------------------------------------
int validate_directory(char *dir_name)
{
    // Iterate over each byte of the directory name
    for (int i = 0; i < 11; i++) 
    {
        char character = dir_name[i];

        // Check if the character is one of the illegal characters
        if (character < 0x20 || character == 0x22 || character == 0x2A || character == 0x2B || 
            character == 0x2C || character == 0x2E || character == 0x2F || character == 0x3A || character == 0x3B ||
            character == 0x3C || character == 0x3D || character == 0x3E || character == 0x3F || character == 0x5B
            || character == 0x5C || character == 0x5D || character == 0x7C) 
        {
            return 0; // Illegal character found
        }
    }
    return 1; // All characters are legal
}
//-----------------------------------------------------------------------------
// getName(const char *dir_name)
//
// PURPOSE: Gets the name of a file/folder from its directory entry structure. 
// INPUT PARAMETERS:
//     dir_name: Takes a pointer to the specific directory name
// OUTPUT PARAMETERS:
//     name: Returns the name of the file/folder
//-----------------------------------------------------------------------------
char* getName(const char *dir_name)
{   
    //A counter for the name characters
    int name_counter = 0;
    //An array of char to hold the name, initialized
    static char name[13] = {'\0'};
    //Loop through each character and skip over the white spaces
    for(int i = 0; i < 11; i++)
    {
        if(dir_name[i] != ' ')
        {
            name[name_counter] = dir_name[i];
            name_counter++; 
        }
    }
    //Null-terminate after the last character
    name[name_counter] = '\0';
    return name;
}
//-----------------------------------------------------------------------------
// insertDot(const char *name)
//
// PURPOSE: Puts a '.' character for the file, between its name and extension as
//          the short names do not store it.  
// INPUT PARAMETERS:
//     name: Name of the file generated from the getName() function above
// OUTPUT PARAMETERS:
//     name: Returns the name of the file with a dot between the name and extension
//-----------------------------------------------------------------------------
char* insertDot(const char *name) {
    //Max full name character would be 8 + 3 + '.' + Null-terminated, so 13 char array
    static char full_filename[13];
    //Size of the name 
    int len = strlen(name);
    //Each extension is 3 characters long, therefore the dot should be placed before 3 character
    //index from the end
    int dot_index = len - 3;

    // Copy the original filename to the modified name buffer
    strcpy(full_filename, name);

    // Check if the filename has enough characters for an extension and insert dot if so
    if (len >= 4 && name[dot_index] != '.') {
        // Shift characters to make space for the dot
        for (int i = len; i >= dot_index; i--) {
            full_filename[i + 1] = full_filename[i];
        }
        // Insert the dot
        full_filename[dot_index] = '.';
    }
    return full_filename;
}

//-----------------------------------------------------------------------------
// list(uint32_t cluster_num, int depth)
//
// PURPOSE: Output all files and directories on the drive, by printing a tree/list of
//          volume image
// INPUT PARAMETERS:
//     cluster_num: Takes in the specific cluster number for jumping to that cluster
//                  in the Data region.
//     Depth: The depth of the files and folders in the whole image, 
//              same as printing the '-' character
// OUTPUT PARAMETERS:
//     None
//-----------------------------------------------------------------------------
void list(uint32_t cluster_num, int depth)
{
    //According to the white paper, we can get the first_data_sector location in the image
    uint32_t first_data_sector = bs.BPB_RsvdSecCnt + (bs.BPB_NumFATs * bs.BPB_FATSz32);
    //As per the white paper, Given any valid data cluster number N, the sector number of the first sector of that cluster
    uint32_t first_sector_of_cluster = ((cluster_num -2) * bs.BPB_SecPerClus) + first_data_sector;
    //We seek to the first sector of a specific cluster
    lseek(fd, first_sector_of_cluster*bs.BPB_BytesPerSec, SEEK_SET);
    
    //The very first cluster number is the root cluster number so we start looking from that directory entry
    //and traverse cluster by cluster while reading subsequent directory entries. 

    //Now we keep on looping for each individual directory entries in a cluster
    for(int dirent_num = 1; dirent_num <= (int)directory_per_cluster; dirent_num++)
    {
        //A current directory variable that holds the information of a certain directory entry, for each 
        //iteration of the loop
        fat32DE curr_dir; 
        read(fd, &curr_dir, 32);
        //Validating that the directory name is valid, and the attriute is indeed of a directory/file
        if(curr_dir.dir_name[0] != 0x00 && (uint8_t)curr_dir.dir_name[0] != 0xE5 && curr_dir.dir_name[0] != 0x05 && curr_dir.dir_name[0] != 0x20 && curr_dir.dir_name[1] != 0x2E && curr_dir.dir_attr != 0x12)
        {
            //For the root directory, the volume attribute in the root directory files 
            //can be set, so we ignore them for now and 
            if(curr_dir.dir_attr == ATTR_VOLUME_ID)
            {}
            //else if it has a directory attribute set
            else if(curr_dir.dir_attr == ATTR_DIRECTORY)
            {
                //We validate the directory first using the valid directory function
                if(validate_directory(curr_dir.dir_name) == 1)
                {   
                    //Gets the name of the directory from the entry
                    char* name = getName(curr_dir.dir_name);
                    //Print the depth of this directory according to the root
                    print_dash(depth);
                    printf("Directory: %s\n", name);
                    //Since it is a directory, it might also contain sub-directories, so we recursively call
                    //list again to list its sub-directories
                    uint32_t this_dir_clust_num = (curr_dir.dir_first_cluster_hi << 16) | curr_dir.dir_first_cluster_lo; 
                    list(this_dir_clust_num, depth + 1);
                }
            }
            //Else It is a file if not volume - ID set or directory attribute
            //Also filtering the long name attributes since i will use a different technique 
            //to get the long file names.
            else if((curr_dir.dir_attr) != (ATTR_LONG_NAME))
            {   
                //As always, we validate the directory
                if(validate_directory(curr_dir.dir_name) == 1)
                {
                    //Get the name from the enrty
                    char* name = getName(curr_dir.dir_name);
                    //Since we know this is a file, so we must put the imaginary '.' to reality.
                    char* filename = insertDot(name);
                    
                    //Now, here is the trick, In this very place, I remember this specific location
                    //of the offset
                    off_t previous_offset = lseek(fd, 0, SEEK_CUR);
                    
                    //Now I loop through the filename by character which i just got 2 lines above
                    //and check for the filenmae with just '~' character. This means that
                    //we encountered a short entry with a preceding long entry, and it has a long name 

                    //Also got the idea from using hexdump, and that the file is in little-endian. 
                    //It is brute-force but does work. Oh well!
                    for(int i =0; i < 12; i++)
                    {
                        if(filename[i] == '~' && filename[0] != '_')
                        {
                            //Long FileName Support:
                            
                            //Create a new file descriptor 
                            int new_offset_fd = open(volume_img, O_RDONLY);
                            //An array of unsigned char initialized with 0 for holding the long file name
                            unsigned char long_filename[256] = {0};
                            
                            // Initialize index for long_filename array
                            int index = 0;
                            
                            //Declare a Long Directory Entry structure
                            fat32LDE curr_ldir;
                            //And now we seek our new fd to that offset of the short entry
                            lseek(new_offset_fd, previous_offset, SEEK_SET);
                            // Loop through long filename entries
                           do {
                                
                                //We go back 64 bytes, since last 32 is the short entry directory structure
                                //So a 32 bytes more backward would be the first long name entry. Found from the
                                //white paper. 
                                lseek(new_offset_fd,-64, SEEK_CUR);

                                //Read in the contents of the long directory entry
                                read(new_offset_fd, &curr_ldir.LDIR_Ord, 1);
                                read(new_offset_fd, &curr_ldir.LDIR_Name1[0], 2);
                                read(new_offset_fd, &curr_ldir.LDIR_Name1[1], 2);
                                read(new_offset_fd, &curr_ldir.LDIR_Name1[2], 2);
                                read(new_offset_fd, &curr_ldir.LDIR_Name1[3], 2);
                                read(new_offset_fd, &curr_ldir.LDIR_Name1[4], 2);
                                read(new_offset_fd, &curr_ldir.LDIR_Attr, 1);
                                read(new_offset_fd, &curr_ldir.LDIR_Type, 1);
                                read(new_offset_fd, &curr_ldir.LDIR_Chksum, 1);
                                read(new_offset_fd, &curr_ldir.LDIR_Name2[0], 2);
                                read(new_offset_fd, &curr_ldir.LDIR_Name2[1], 2);
                                read(new_offset_fd, &curr_ldir.LDIR_Name2[2], 2);
                                read(new_offset_fd, &curr_ldir.LDIR_Name2[3], 2);
                                read(new_offset_fd, &curr_ldir.LDIR_Name2[4], 2);
                                read(new_offset_fd, &curr_ldir.LDIR_Name2[5], 2);
                                read(new_offset_fd, &curr_ldir.LDIR_FstClusLO, 2);
                                read(new_offset_fd, &curr_ldir.LDIR_Name3[0], 2);
                                read(new_offset_fd, &curr_ldir.LDIR_Name3[1], 2);
                                
                                //Since the names are stored in unicode, need to convert to char
                                // Copy characters from LDIR_Name1 one by one
                                for (int i = 0; i < 5; i++) {
                                    uint16_t unicode_char = curr_ldir.LDIR_Name1[i];
                                    //If it is not empty and not the last entry of a name
                                    if (unicode_char != 0x0000 && unicode_char != 0xFFFF) {
                                        //Get the upper two bits of the unicode and put into the long filename array
                                        //ANDing with the top two bits of the uint_16 unicode value and casting to char
                                        //as they are the ascii values(5700 - 57, for 'W')
                                        long_filename[index++] = (char)(unicode_char & 0xFF);
                                    }
                                }
                                
                                //Same as above but for the second part of the name
                                // Copy characters from LDIR_Name2
                                for (int i = 0; i < 6; i++) {
                                    uint16_t unicode_char = curr_ldir.LDIR_Name2[i];
                                    if (unicode_char != 0x0000 && unicode_char != 0xFFFF) {
                                        long_filename[index++] = (char)(unicode_char & 0xFF);
                                    }
                                }
                                
                                //Same as above but for the second part of the name
                                // Copy characters from LDIR_Name3
                                for (int i = 0; i < 2; i++) {
                                    uint16_t unicode_char = curr_ldir.LDIR_Name3[i];
                                    if (unicode_char != 0x0000 && unicode_char != 0xFFFF) {
                                        long_filename[index++] = (char)(unicode_char & 0xFF);
                                    }
                                }
                                //From white paper, we also know that ord attribute will have a unique number 
                                //from 1 to N, where the N | LAST_LONG_ENTRY will the last entry of a long-name sequence.
                                //Masking always gives 0x40 or higher, so the order must be less than equal to 0x40 for the loop to continue                    
                            } while((curr_ldir.LDIR_Ord) <= LAST_LONG_ENTRY);

                            // Null-terminate the long filename string
                            long_filename[index] = '\0';

                            //printf("File: %s, Long Filename: %s\n",filename, long_filename);                        

                            
                            //This portion was done since the last very long filename was being printed twice, 
                            //So i made an array of keeping all short file name entries with associated long names,
                            //and put them in the array after printing so that they are not 
                            //printed twice if already printed, because of recursive cases. 

                            //If not printed then 0, else 1
                            int already_printed = 0;
                            for (int j = 0; j < num_printed_files; j++) {
                                if (strcmp(filename, printed_filenames[j]) == 0) {
                                    already_printed = 1;
                                    break;
                                }
                            }
                            //If the file name has not been printed, print it and insert the short filename
                            //into the printed_filenames array
                            if (already_printed == 0) 
                            {   
                                //Print the short file name along with its long file name
                                print_dash(depth);
                                printf("File: %s, Long Filename: %s\n", filename, long_filename);

                                if (num_printed_files < MAX_PRINTED_FILES) {
                                    strcpy(printed_filenames[num_printed_files], filename);
                                    num_printed_files++;
                                }
                            }
                            break;
                        }
                    }
                    //If there is no '~' character, therefore it is just a normal file, so we print only the ones 
                    //that are allowed and not hidden. 
                    int i;
                    for (i = 0; filename[i] != '\0'; i++) {
                        if (filename[i] == '_' || filename[i] == '~') 
                        {
                            // If filename contains '_' or '~', skip printing and break out of the loop
                            break;
                        }
                    }
                    if (filename[i] == '\0') {
                        // If loop completes without finding '_' or '~', 
                        //print the filename
                        print_dash(depth);
                        printf("File: %s\n", filename);
                    }

                }
            }
        }
        //Seek to the next directory entry
        lseek(fd, (first_sector_of_cluster*bs.BPB_BytesPerSec) + (32 * dirent_num), SEEK_SET);
    }
    //Now some directories might have multiple clusters, so we refer back to the FAT table as directory entries
    //only tell about the first cluster number of the files and sub-directories, so 
    //check if there is a next cluster address, if so, we recursively call list again

    //Calculations are taken from hardware white paper:
    // Calculate FAT entry's sector number and offset within that sector
    uint32_t FATOffset = cluster_num * 4;
    uint32_t ThisFATSecNum = bs.BPB_RsvdSecCnt + (FATOffset / bs.BPB_BytesPerSec);
    uint32_t ThisFATEntOffset = FATOffset % bs.BPB_BytesPerSec;
    
    // Seek to the correct sector for the FAT entry
    lseek(fd, ThisFATSecNum * bs.BPB_BytesPerSec, SEEK_SET);
    
    // Read the next cluster from the FAT entry
    uint32_t next_cluster;
    lseek(fd, ThisFATEntOffset, SEEK_CUR);
    read(fd, &next_cluster, 4);
    // Check if there's another cluster to process recursively
    if(next_cluster != EOC)
    {   
        //Top 4 bits are discarded and remaining 28 bits are our cluster number
        next_cluster &= EOC;
        list(next_cluster, depth);
    }
}
//-----------------------------------------------------------------------------
// tokenize_path(char* path)
//
// PURPOSE: Tokenize the 4 argument of the argv array, which is for example Books/WARAND~1.TXT
// INPUT PARAMETERS:
//     path: Takes in the whole path array and tokenizes it by getting the last element as we know 
//           it is going to be the file
// OUTPUT PARAMETERS:
//     None
//-----------------------------------------------------------------------------
void tokenize_path(char* path)
{
    //The tokens of the path array will be held here 
    char* tokens;
    //This is the full path array
    char* commands[40] = {0};
    //To keep the index of the path in commands array
    int path_index = 0;
    //Tokenize it with the '/' character
    tokens = strtok(path, "/");
    while(tokens != NULL)
    {
        //Put tokens by index in the commands array
        commands[path_index] = strdup(tokens);
        path_index++;
        tokens = strtok(NULL, "/");
    }
    //printf("\n%s %s\n", commands[0], commands[path_index-1]);

    //We know that the last token will be the file name so take that into a variable
    //This is the file we are trying to get
    file = commands[path_index-1];

    // Convert the file name to uppercase as all short entry names are in uppercase 
    for (int i = 0; file[i] != '\0'; i++) {
        file[i] = toupper(file[i]);
    }
    //printf("\nFile: %s\n", file);
}

//-----------------------------------------------------------------------------
// get(uint32_t cluster_num, int depth, char* file)
//
// PURPOSE: Traverses the full directory entry structures to find the file which matches with the 
//          current filename
// INPUT PARAMETERS: 
//     cluster_num: Takes in the specific cluster number for jumping to that cluster
//                  in the Data region.
//     depth: The depth of the files and folders in the whole image, 
//              same as printing the '-' character
//     file: The file that we are trying to retrieve
//     
// OUTPUT PARAMETERS:
//     None
//-----------------------------------------------------------------------------
void get(uint32_t cluster_num, int depth, char* file)
{
    //According to the white paper, we can get the first_data_sector location in the image
    uint32_t first_data_sector = bs.BPB_RsvdSecCnt + (bs.BPB_NumFATs * bs.BPB_FATSz32);
    //As per the white paper, Given any valid data cluster number N, the sector number of the first sector of that cluster
    uint32_t first_sector_of_cluster = ((cluster_num -2) * bs.BPB_SecPerClus) + first_data_sector;
    //We seek to the first sector of a specific cluster
    lseek(fd, first_sector_of_cluster*bs.BPB_BytesPerSec, SEEK_SET);
    
    //No we keep on looping for each individual directory entries in a cluster
    for(int dirent_num = 1; dirent_num <= (int)directory_per_cluster && gotFile == 0; dirent_num++)
    {
        //A current directory variable that holds the information of a certain directory entry, for each 
        //iteration of the loop
        fat32DE curr_dir; 
        read(fd, &curr_dir, 32);
        //Validating that the directory name is valid, and the attriute is indeed of a directory/file
        if(curr_dir.dir_name[0] != 0x00 && (uint8_t)curr_dir.dir_name[0] != 0xE5 && curr_dir.dir_name[0] != 0x05 && curr_dir.dir_name[0] != 0x20 && curr_dir.dir_name[1] != 0x2E && curr_dir.dir_attr != 0x12)
        {
            //For the root directory, the volume attribute in the root directory files 
            //can be set, so we ignore them for now and 
            if(curr_dir.dir_attr == ATTR_VOLUME_ID)
            {}
            //else if it has a directory attribute set
            else if(curr_dir.dir_attr == ATTR_DIRECTORY)
            {
                //We validate the directory first using the valid directory function
                if(validate_directory(curr_dir.dir_name) == 1)
                {   
                    //Recursively goes to the next cluster number of a directories for its subdirectories
                    uint32_t this_dir_clust_num = (curr_dir.dir_first_cluster_hi << 16) | curr_dir.dir_first_cluster_lo; 
                    get(this_dir_clust_num, depth + 1, file);
                }
            }
            else if((curr_dir.dir_attr) != (ATTR_LONG_NAME))
            {
                if(validate_directory(curr_dir.dir_name) == 1)
                {
                    //Gets the name of this file appended with the '.' character. 
                    char* name = getName(curr_dir.dir_name);
                    char* filename = insertDot(name);
                    
                    //If this filename matches with the file that we are trying to retrieve
                    if(strcmp(filename, file) == 0)
                    {
                        //We set the gotFile variable that the file has been found
                        gotFile = 1;
                        //We get the starting cluster number of this file
                        uint32_t start_cluster_file = curr_dir.dir_first_cluster_hi << 16 | curr_dir.dir_first_cluster_lo;
                        //We open a new file descriptor with the name of this file and gives it permission to read/write for user and group and read for everyone
                        new_fd = open(file, O_WRONLY | O_CREAT | O_TRUNC | O_APPEND, S_IRUSR | S_IWUSR | S_IXUSR| S_IRGRP | S_IWGRP | S_IROTH);
                        //Now we get the contents of this file by calling the get_file_contents function
                        get_file_contents(start_cluster_file);
                    }
                }
            }
        }
        //Seek to the next directory entry
        lseek(fd, (first_sector_of_cluster*bs.BPB_BytesPerSec) + (32 * dirent_num), SEEK_SET);
    }
    //If we still did not get the file, that means that this directory has another cluster under it
    //We try to get the next cluster number from the FAT table 
    if(gotFile == 0)
    {
        // Calculate FAT entry's sector number and offset within that sector
        uint32_t FATOffset = cluster_num * 4;
        uint32_t ThisFATSecNum = bs.BPB_RsvdSecCnt + (FATOffset / bs.BPB_BytesPerSec);
        uint32_t ThisFATEntOffset = FATOffset % bs.BPB_BytesPerSec;
        
        // Seek to the correct sector for the FAT entry
        lseek(fd, ThisFATSecNum * bs.BPB_BytesPerSec, SEEK_SET);
        
        // Read the next cluster from the FAT entry
        uint32_t next_cluster;
        lseek(fd, ThisFATEntOffset, SEEK_CUR);
        read(fd, &next_cluster, 4);
        //If there is a next cluster number in the FAT table, we call get again recursively to get the actual file
        if(next_cluster != EOC)
        {
            next_cluster &= EOC;
            get(next_cluster, depth, file);
        }
    }
}

//-----------------------------------------------------------------------------
// get_file_contents(uint32_t cluster_num)
//
// PURPOSE: Writing the contents of the file that we need to retrieve from the volume
// INPUT PARAMETERS: 
//     cluster_num: Takes in the specific cluster number for jumping to that cluster
//                  in the Data region.
//     depth: The depth of the files and folders in the whole image, 
//              same as printing the '-' character
//     file: The file that we are trying to retrieve
//     
// OUTPUT PARAMETERS:
//     None
//-----------------------------------------------------------------------------
void get_file_contents(uint32_t cluster_num)
{
    //We go to the first data sector of this file
    uint32_t first_data_sector = bs.BPB_RsvdSecCnt + (bs.BPB_NumFATs * bs.BPB_FATSz32);
    //As per the white paper, Given any valid data cluster number N, the sector number of the first sector of that cluster
    uint32_t first_sector_of_cluster = ((cluster_num -2) * bs.BPB_SecPerClus) + first_data_sector;
    //We seek to the first sector of a specific cluster
    lseek(fd, first_sector_of_cluster*bs.BPB_BytesPerSec, SEEK_SET);

    //An array to hold the bytes of a sector
    char buffer[bs.BPB_BytesPerSec];

    uint32_t bytes_read;
    uint32_t bytes_written;


    // Read the data from the cluster into a buffer
    bytes_read = read(fd, buffer, bs.BPB_BytesPerSec);
    assert(bytes_read > 0);
    
    // Write the data from the buffer to the new file descriptor
    bytes_written = write(new_fd, buffer, bytes_read);
    assert(bytes_written > 0);

    //Get to the next cluster number of this file from the FAT table if so

    // Calculate FAT entry's sector number and offset within that sector
    uint32_t FATOffset = cluster_num * 4;
    uint32_t ThisFATSecNum = bs.BPB_RsvdSecCnt + (FATOffset / bs.BPB_BytesPerSec);
    uint32_t ThisFATEntOffset = FATOffset % bs.BPB_BytesPerSec;
    
    // Seek to the correct sector for the FAT entry
    lseek(fd, ThisFATSecNum * bs.BPB_BytesPerSec, SEEK_SET);
    
    // Read the next cluster from the FAT entry
    uint32_t next_cluster;
    lseek(fd, ThisFATEntOffset, SEEK_CUR);
    read(fd, &next_cluster, 4);
    //If there is no end of chain marker, that means there is another cluster associated with it.
    if(next_cluster != EOC)
    {
        next_cluster &= EOC;
        get_file_contents(next_cluster);
    }

}
//-----------------------------------------------------------------------------
// Main
//
// PURPOSE: Opens the volume and checks the commands for specific functions(info,list,get)
// INPUT PARAMETERS: 
//      argc: Amount of arguments passed while running the program on terminal
//      argv: Arguments passed to characters in arrays   
//     
// OUTPUT PARAMETERS:
//     None
//-----------------------------------------------------------------------------
int main(int argc, char* argv[])
{
    //Checks
    assert(argc >= 3);
    assert(argv != NULL);

    //getting the volume image
    volume_img = argv[1];
    //getting the function required in command {info, list, get}
    functions = argv[2];

    //Open the file descriptor fd, used throughout the program
    if((fd = open(volume_img, O_RDONLY)) == -1)
    {
        printf("Unable to open the volume image.\n");
        exit(EXIT_FAILURE);
    }

    //Reads in the main boot sector infos and validates it
    read_and_validate_boot_sector(fd);
    //Reads in the fsinfo structure to determine the free clusters for free space on the volume
    free_clusters_count(fd);

    //Check for the function and call that specific one
    if((strcmp(functions, "info")) == 0)
    {
        info();
    }
    else if((strcmp(functions, "list")) == 0)
    {
        //The first cluster number is of the root cluster of the image
        list(bs.BPB_RootClus, 0);
    }
    else if((strcmp(functions, "get")) == 0)
    {
        //Must have a path to get the file from
        if(argc < 4)
        {
            printf("Please provide a proper number of arguments for get(). Exiting.\n");
            exit(EXIT_FAILURE);
        }
        tokenize_path(argv[3]);
        get(bs.BPB_RootClus, 0, file);
        if(gotFile == 0)
        {
            printf("\nNo file in the volume named: %s. Please provide a correct path and file.\n", file);
            exit(EXIT_FAILURE);
        }
    }
    else
    {
        printf("Invalid function. Please provide a proper function(info,list,get)\n");
        exit(EXIT_FAILURE);
    }
    
    close(new_fd);
    close(fd);
    printf("\nEnd of Processing.\n");
    return EXIT_SUCCESS;
}
