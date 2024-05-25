# Reading a FAT-32 File System and fetch a file from the file system

# Features

- Performs 3 functions: info, list and get
- Info -> Gives all the information about the FS.
- List -> Lists all the files and directories in the FS(works as unix's 'ls' command)
- Get -> Gets or fetches a specific file from the FS into the current working directory. 

# Compiling and Running

> Compile the programs by running "make" in the directory

 - use ./fat32 [volume name] [function: info/list/get] [path of a file to retrieve(for get only)], 
 example: "./fat32 ~comp3430/fat32volumes/3430-good.img list"

> For get, you can try numerous path: I am just giving two examples of getting a text file and an image
 -  ./fat32 ~comp3430/fat32volumes/3430-deep.img get BOOKS/PANDP.TXT
 -  ./fat32 ~comp3430/fat32volumes/3430-good.img get IMAGES/CODE.JPG
 - Or any other you feel like, use SHORT names, also you can use with the deep one:
 - ./fat32 3430-deep.img get BOOKS/WARAND~1.TXT 

> Remove all executables by running: "make clean" 
