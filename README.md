HTTP Server


-------------List of files------------

1. server.c - Code file written in c programming language, contains the main function, and all the implementaions of the functions of the program.
2. threadpool.c - Code file written in c programming language, contains all the implementaions of the functions written in the header file threadpool.h .


-------------Description-------------

This program implements an HTTP Server that can handle requests from the kind of "GET" method, and HTTP/1.0 or HTTP/1.1 versions.
The server handles the connections with the clients. There is always one socket where the server listens to connections and for each client connection request, the
server opens another socket especially for the specific client. In order to make this server a multithreaded program, it creates threads (in the quantity asked by
the user in the cmd input) that handle the connections with the clients. Since, the server maintains a limited number of threads, it constructs a thread pool.
This pool of threads is being created in advanced and each time it needs a thread to handle a client connection and request, it takes one thread from the pool,
or enqueue the request to the queue, in case there is no available thread in the pool.


--------Installation/Compilation of the program---------
Open linux terminal, navigate to the folder containing "ex3" folder using the "cd" command (confirm it by using ls command)
Type in the terminal: gcc threadpool.h threadpool.c server.c -o server -g -Wall -lpthread .


-----------Activation of the program--------------------
Open linux terminal, navigate to server executeable file location using "cd" command (confirm it using ls command) and type: 
./server <port> <pool-size> <max-number-of-request> .
To run the program with valgrind, type: valgrind ./server <port> <pool-size> <max-number-of-request> .
Afterwards the server will run and for sending to it request you can use a browser and put in the URL: http://<computer-name>:<port>/<your-path>.
If you use a browser from the same machine the server is running on, you can use “localhost” as computer-name, otherwise, use the IPv4 of the computer running the server.


----------Program functions with their goal and output---------------

int make_socket_connection(char* argv[]);
goal: makes the welcome socket of the server.
output: returns the file descriptor of the welcome socket.

int create_response(void* fd);
goal: read the request from the client, check the validation of the request, make the needed response, write the response to the client.
output: returns an indication of what was send to the client (an error response, 302 found or a file/directory content response).

int check_request(char* request, int fd);
goal: checks that there are 3 tokens in the first line of the request and their validation, and checks if the path is a file or a folder and sends
the requests to the needed sub method
output: returns an indication of what was send to the client (an error response, 302 found or a file/directory content response).

int make_response(int response_num, char* path, int fd);
goal: sends the client the error response needed by the value of response_num.
output: returns ERR_WAS_SEND.

int make_file_response(char* path, int fd);
goal: makes the headers of the response, and call a function which read the content of the path's file and write it to the client's socket.
output: it returns OK in case everything went well, and ERR_WAS_SEND in case an error occurred and the server send an errror response to the client.

int make_dir_content_response(char* path, int fd);
goal: write the headers and the content of the directory which the path points to, to the client's socket.
output: it returns OK in case everything went well, and ERR_WAS_SEND in case an error occurred and the server send an errror response to the client.

int stat_func(struct stat* file_stat, char* path, int fd);
goal: check if the path does exist in the server.
output: -1 in case path does not exist, and OK in case it does exist.

int check_path_permission(char* path, int fd);
goal: checks the validation of "path".
output: it returns if the path is valid or invalid.

int search_index_html_file(char* path, int fd);
goal: checks if the file "index.html" does exist in the last folder of the path.
output: returns OK if "index.html" does exist and -1 if it doesn't exist.

int read_write_file(char* path, int fd);
goal: read data from file and write it to the client's socket.
output: return OK if everything went well, and ERR_WAS_SEND if an error occurred.

int write_response(char* buffer, int fd);
goal: writes the response to the client's socket.
output: returns OK if everything went well, and ERR_WAS_SEND if an error occurred.

char* get_mime_type(char* name);
goal: identify the mime type of a file.
output: returns the mime type of a file.

char* get_time();
goal: gets the time which the response is send.
output: returns the time which the response is send.

char* get_last_modified(char* file);
goal: gets the time which the file or folder was modified for the last time.
output: returnss the time which the file or folder was modified for the last time.

int num_to_str_size(int num);
goal: gets a number and identify the number of digits which this number contains.
output: returns the number of digits which this number contains.

char* add_entity_quote(char* entity_name, int flag);
goal: gets a name of file or folder , and adds to it quotation mark.
output: returns a name of file or folder with the quotation mark.

char* do_str_malloc(int size);
goal: does a malloc of char*.
output: returns a pointer to it.

int is_only_digits(char* str);
goal: check if there is a non digit char in a string.
output: returns 1 is there is not a non digit char in 'str, otherwise it returns -1.
