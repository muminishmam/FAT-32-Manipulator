> Compile the programs by running "make" in the directory

# Q1 Reading a FAT-32 File System and Manipulating it
 - use ./fat32 [volume name] [function: info/list/get] [path of a file to retrieve(for get only)], 
 example: "./fat32 ~comp3430/fat32volumes/3430-good.img list"

> For get, you can try numerous path: I am just giving two examples of getting a text file and an image
 -  ./fat32 ~comp3430/fat32volumes/3430-deep.img get BOOKS/PANDP.TXT
 -  ./fat32 ~comp3430/fat32volumes/3430-good.img get IMAGES/CODE.JPG
 - Or any other you feel like, but as per the assignment, use SHORT names, also you can use the one on the assignment too, but with the deep one:
 - ./fat32 ~comp3430/fat32volumes/3430-deep.img get BOOKS/WARAND~1.TXT 

> Remove all executables by running: "make clean" 