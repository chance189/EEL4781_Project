# EEL4781_Project

This project attempts to create a client and a server. Note, the client and server are thought to have their own unique filespace, therefore, it is discouraged to run the two executables in the same folder. 

## Making the project
To make the project, simply navigate to the code directores and run 'make' on a linux terminal. Create a folder for client, and move the executable to that folder, to ensure that the local spaces of each program are not opening the same resources.

## How it works
The client and server are agreeing to what information is sent or received based on a struct with specific enum information. Based on the flags and or values in this struct, it denotes whether the server is sending or receiving a file, what portion of a file to send, errors, ect.

The struct has explicit size using unsigned integers, with a width of each ensuring no overflow could occur. This way, there is not issue in reading out information from the socket, as it will always be the same number of bytes.
Additionally, the file_name is fixed in this implementation to 255 bytes

## Debug

In the server class, you can toggle the Debug global variable to denote whether to see debug messages. These include the percentage (in units of 10%) displayed out to stdout. 
The above also includes when the file has finished sending.

The client will only print out error messages, from parsing the struct sent by the server.

## Author

* Chance Reimer
