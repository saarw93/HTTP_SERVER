/** Author: Saar Weitzman**/
/** I.D: 204175137**/
/** Date: 31.12.18**/
/** Ex2- HTTP Server Implementation**/


#include "threadpool.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>


//#define DEBUG
#define USAGE "Usage: server <port> <pool-size> <max-number-of-request>\r\n"
#define RFC1123FMT "%a, %d %b %Y %H:%M:%S GMT"
#define BUFFER_SIZE 4001
#define OK 200
#define ERR_WAS_SEND (-1)
#define MAX_PORT_NUM 65535
#define MIN_PORT_NUM 1


/**functions declerations**/
int make_socket_connection(char* argv[]);
int create_response(void* fd);
int check_request(char* request, int fd);
int make_response(int response_num, char* path, int fd);
int make_file_response(char* path, int fd);
int make_dir_content_response(char* path, int fd);
int stat_func(struct stat* file_stat, char* path, int fd);
int check_path_permission(char* path, int fd);
int search_index_html_file(char* path, int fd);
int read_write_file(char* path, int fd);
int write_response(char* buffer, int fd);
char* get_mime_type(char* name);
char* get_time();
char* get_last_modified(char* file);
int num_to_str_size(int num);
char* add_entity_quote(char* entity_name, int flag);
char* do_str_malloc(int size);
int is_only_digits(char* str);



int main(int argc, char* argv[])
{
    if (argc != 4)  //there are too many arguments in argv
    {
        printf(USAGE);
        exit(EXIT_FAILURE);
    }

    int i;
    for (i = 1; i < argc; i++)
    {
        if (is_only_digits(argv[i]) != 1)  //check that the input numbers are composed only from digits
        {
            printf(USAGE);
            exit(EXIT_FAILURE);
        }
    }
    //if the input port, or max num of requests, or max num of threads in pool are negative
    if ((atoi(argv[1]) < MIN_PORT_NUM || atoi(argv[1]) > MAX_PORT_NUM) || (atoi(argv[2]) <= 0 || atoi(argv[3]) <= 0))
    {
        printf(USAGE);
        exit(EXIT_FAILURE);
    }

    int fd = make_socket_connection(argv);  //make a welcome socket
    if (fd < 0)
        exit(EXIT_FAILURE);

    threadpool *server_threadpool = create_threadpool(atoi(argv[2]));  //create thread pool by the input pool size
    if (server_threadpool == NULL)
    {
        printf(USAGE);
        close(fd);
        exit(EXIT_FAILURE);
    }

    int* clients_fd = (int*)malloc(sizeof(int) * atoi(argv[3])); //the size of max number of requests
    if (clients_fd == NULL)
    {
        printf("ERROR: malloc of clients_fd array failed\r\n");
        close(fd);
        destroy_threadpool(server_threadpool);
        exit(EXIT_FAILURE);
    }
    memset(clients_fd, 0, sizeof(int) * atoi(argv[3]));  //initialize the array with zeroes

    for( i = 0; i < atoi(argv[3]); i++)
    {
        clients_fd[i] = accept(fd, NULL, NULL);  //wait for clients' requests
        if(clients_fd[i] < 0)
        {
            perror("ERROR: accept() has been failed\r\n");
            break;
        }
        //use one of threads in pool to create response to the client
        dispatch (server_threadpool, create_response, (void*)&clients_fd[i]);
    }
    close(fd);
    destroy_threadpool(server_threadpool);
    free(clients_fd);
    return 0;
}


/**This function:
 * 1.read the request from the client
 * 2.check the validation of the request
 * 3.make the needed response
 * 4.write the response to the client
 * the function will do it by using other sub methods
 * **/
int create_response(void* fd)
{
    if (fd == NULL)
    {
        printf("Error: create response got a NULL void* fd\r\n");
        return -1;
    }

    char request_buffer[BUFFER_SIZE];
    bzero(request_buffer, BUFFER_SIZE);

    int nbytes = 0;
    int sockfd = (*(int*)fd);  //cast the input fd argument
    #ifdef DEBUG
    printf("in create response function\r\n");
    #endif

    nbytes = (int)read(sockfd, request_buffer , BUFFER_SIZE - 1);  //read request from client
    if (nbytes < 0)
    {
        perror("ERROR: cannot read from socket");
        return  make_response(500, NULL, sockfd);
    }

    int response = check_request(request_buffer, sockfd); //check request will check validation and send response to client
    close(sockfd);
    if (response == ERR_WAS_SEND)
        return ERR_WAS_SEND;    //notify the server sent an error message to the client
    return 0;
}


/**the function:
 * 1.checks that there are 3 tokens in the first line of the request and their validation
 * 2.checks if the path is a file or a folder and sends the requests to the needed sub method
 * **/
int check_request(char* request, int fd)
{
    char *method = NULL, *path = NULL, *version = NULL;
    char *temp_token = request;
    int status = 0;
    strtok(temp_token, "\r\n");  //get the first line of the request

    #ifdef DEBUG
    printf("%s\r\n", temp_token);
    #endif

    temp_token = strtok(temp_token, " ");  // get the first token (i.e, the GET)
    int i = 0;
    while (temp_token != NULL)   // walk through other tokens (i.e, the path and the version)
    {
        if (i == 0)
            method = temp_token;
        if (i == 1)
            path = temp_token;
        if (i == 2)
            version = temp_token;
        temp_token = strtok(NULL, " ");
        i++;
    }

    if (i != 3)  //too much or not enough arguments in the cmd input
        return make_response(400, NULL, fd);

    if (strcmp(version, "HTTP/1.0") != 0 && strcmp(version, "HTTP/1.1") != 0)
        return make_response(400, NULL, fd);

    if (strcmp(method, "GET") != 0)
        return make_response(501, NULL, fd);


    /*from here we check the validation of the path*/
    struct stat file_stat;  //a struct that saves data on a file
    status = stat_func(&file_stat, path+1, fd);
    if (status != 1)
        return ERR_WAS_SEND;

    /*if we got here, the path exists*/

    if(S_ISDIR(file_stat.st_mode) == 1)   /*the path is a directory*/
    {
        char* slash_end_path = strrchr(path, '/');  //get a pointer to the last slash in the requested path

        if (slash_end_path == NULL || slash_end_path+1 == NULL)
            return make_response(400, NULL, fd);

        if (strcmp(slash_end_path + 1, "\0") == 0) //the path ends with slash, need to send response of index.html to the client
        {
            status = search_index_html_file(path+1, fd);  //check if index.html file does exist in the path folder
            if (status == 500)
                return make_response(500, NULL, fd);
            if (status == OK)  //need to return index.html
            {
                int path_len = (int)(strlen(path) + strlen("index.html") + 1);
                char* index_path = (char*)malloc(sizeof(char) * path_len);  //append the index.html to the path
                if (index_path == NULL)
                    return make_response(500, NULL, fd);
                bzero(index_path, path_len);
                sprintf(index_path, "%s%s", path+1, "index.html");
                if (check_path_permission(index_path, fd) != OK)    //need to check the permissions of the path
                {
                    free(index_path);
                    return ERR_WAS_SEND;
                }

                int val = make_file_response(index_path, fd);
                free(index_path);
                return val;
            }
            else   //need to return the content of the last directory in the path
            {
                if (check_path_permission(path+1, fd) != OK)    //need to check the permissions of the path
                    return ERR_WAS_SEND;
                return make_dir_content_response(path+1, fd);
            }
        }
        else   //there is no slash at the end of the path, need to send 302 found response
            return make_response(302, path+1, fd);
    }

    /*if we got here, it means the path is a file, need to checks it's permissions and if it is a regular file*/

    status = check_path_permission(path+1, fd);
    if (status == ERR_WAS_SEND)  //the path is invalid
        return ERR_WAS_SEND;

    /*if we got here, the path permissions are OK and the file is a regular file*/
    return make_file_response(path+1, fd);
}


/**The function check if the path does exist in the server**/
int stat_func(struct stat* file_stat, char* path, int fd)
{
    if (path != NULL && file_stat != NULL)
    {
        if (strlen(path) == 0)  //case the request path is empty, need to bring the root directory content, or the index.html of it
            strcpy(path, "./");
        if (stat(path, file_stat) < 0)
        {
            if (errno == ENOENT || errno == EFAULT || (strcmp((path+strlen(path)-1), "/") == 0 && errno == ENOTDIR))
                return make_response(404, NULL, fd); //the path deos not exist in the server
            else
                return make_response(500, NULL, fd); //there was a problem with the function stat()
        }
        return 1;  //the path is valid
    }
    return make_response(500, NULL, fd);   //error with the path and/or the stat inputs of the function
}


/**The function checks if the file "index.html" does exist in the last folder of the path**/
int search_index_html_file(char* path, int fd)
{
    int num_of_files = 0, i, j;
    struct dirent **dir_file_list;   //a struct that saves data on a directory
    num_of_files = scandir(path, &dir_file_list, NULL, alphasort); //scan directory to check if there is an index.html file in it
    if (num_of_files == -1)
    {
        perror("ERROR: scandir() function failed\r\n");
        return 500;
    }

    for (i = 2; i < num_of_files; i++)
    {
        if (strcmp(dir_file_list[i]->d_name, "index.html") == 0)
        {
            for (j = 0; j < num_of_files; j++)
                free(dir_file_list[j]);
            free(dir_file_list);
            return OK;
        }
    }

    for (j = 0; j < num_of_files; j++)
        free(dir_file_list[j]);
    free(dir_file_list);
    return -1;  //the file index.html does not exist, need to return the contents of the directory of the path
}


#define FILE_RESPONSE_TYPE "HTTP/1.0 200 OK\r\nServer: webserver/1.0\r\nDate: %s\r\nContent-Type: %s\r\nContent-Length: %d\r\nLast-Modified: %s\r\nConnection: close\r\n\r\n"
#define FILE_RESPONSE "HTTP/1.0 200 OK\r\nServer: webserver/1.0\r\nDate: %s\r\nContent-Length: %d\r\nLast-Modified: %s\r\nConnection: close\r\n\r\n"
/**This function makes the headers of the response, and call a function which read the content of the path's file and write it to the client's socket**/
int make_file_response(char* path, int fd)
{
    struct stat file_stat;  //struct that gives data on the file
    int status = stat_func(&file_stat, path, fd);
    if (status != 1)
        return ERR_WAS_SEND;

    int file_size = file_stat.st_size;
    char* mime_type = get_mime_type(path);
    char* time = get_time();
    char* last_modified = get_last_modified(path);
    if (time == NULL || last_modified == NULL)  //there was an internal server error with one of those variables
    {
        if (time != NULL)
            free(time);
        if(last_modified != NULL)
            free(last_modified);

        return make_response(500, NULL, fd);
    }

    int size = (int)(strlen(time) + strlen(last_modified) + file_size + 1);
    if (mime_type != NULL)
        size+= (int)(strlen(FILE_RESPONSE_TYPE) + strlen(mime_type));  //size with mime_type
    else
        size+= (int)strlen(FILE_RESPONSE);  //size without mime_type

    char* header_response = (char*)malloc(sizeof(char) * size);
    if (header_response == NULL)
    {
        free(time);
        free(last_modified);
        return make_response(500, NULL, fd);
    }
    bzero(header_response, sizeof(char) * size);

    if (mime_type != NULL)
        sprintf(header_response, FILE_RESPONSE_TYPE, time, mime_type, file_size, last_modified); //string with mime_type
    else
        sprintf(header_response, FILE_RESPONSE, time, file_size, last_modified);  //string without mime_type

    #ifdef DEBUG
    printf("response =\r\n %s%s\r\n", header_response, header_response);
    #endif


    status = write_response(header_response, fd); //write the headers to the client
    if (status != OK)
        return ERR_WAS_SEND;

    int val = read_write_file(path, fd);  //read and write the content of the file to the client
    free(time);
    free(last_modified);
    free(header_response);
    return val;
}


/**This function read data from file and write it to the client's socket**/
int read_write_file(char* path, int fd)
{
    int file_fd = open(path, O_RDONLY);  //open the file for read
    if (file_fd < 0)
        return make_response(500, NULL, fd);
    int r_nbytes = 0, w_nbytes = 0;
    struct stat file_stat;
    int status = stat_func(&file_stat, path, fd);
    if (status != 1)
        return ERR_WAS_SEND;

    unsigned char buffer[1024] = {'\0'};
    while (1)
    {
        r_nbytes = (int)read(file_fd, buffer, sizeof(buffer)-1);  //read from the file
        if (r_nbytes < 0)
        {
            close(file_fd);
            return make_response(500, NULL, fd);
        }
        if (r_nbytes == 0)
            break;

        w_nbytes = (int)write(fd, buffer, r_nbytes); //write to the client the content which was read in the read() function
        if (w_nbytes < 0) {
            close (file_fd);
            perror("ERROR: write function failed\r\n");
            return -1;
        }
    }
    close(file_fd);
    return OK;
}


/**This function writes the response to the client's socket**/
int write_response(char* buffer, int fd)
{
    if (buffer == NULL || fd < 0)
        return -1;
    int w_nbytes = 0;

    w_nbytes = (int)write(fd, buffer, strlen(buffer));
    if (w_nbytes < 0)
    {
        perror("ERROR: write function failed\r\n");
        return -1;
    }
    return OK;
}

#define FILE_INFO "<tr>\r\n<td><A HREF=%s>%s</A></td><td>%s</td><td>%s</td></tr>\r\n"
#define FOLDER_INFO "<tr>\r\n<td><A HREF=%s>%s</A></td><td>%s</td><td></td></tr>\r\n"
/**This function write the headers and the content of the directory which the path points to, to the client's socket**/
int make_dir_content_response(char* path, int fd)
{
    int num_of_files = 0, size = 0, old_size = 0, entity_size = 0, status = 0, i = 0;
    char f_size_to_str[100] = {'\0'};
    char* last_modified = NULL, *entity_name = NULL;
    struct stat entity_stat;   //a struct that saves data on a file
    struct dirent **dir_file_list;   //a struct that saves data on a directory
    num_of_files = scandir(path, &dir_file_list, NULL,
                           alphasort); //scan directory to get all the files and sub folders in the path directory
    if (num_of_files == -1)
    {
        perror("ERROR: scandir() function failed\r\n");
        return make_response(500, NULL, fd);
    }

    char html_start[] = "<HTML>\r\n<HEAD><TITLE>Index of %s</TITLE></HEAD>\r\n\r\n<BODY><H4>Index of %s</H4>\r\n\r\n"
            "<table CELLSPACING=8>\r\n<tr><th>Name</th><th>Last Modified</th><th>Size</th></tr>\r\n\r\n";
    char html_end[] = "</table>\r\n<HR>\r\n<ADDRESS>webserver/1.0</ADDRESS>\r\n</BODY></HTML>";
    size = (int)(strlen(html_start) + (strlen(path) * 2) + strlen(html_end) + 1);

    char* response = (char*)malloc(sizeof(char) * size);  //response will be the html content of the server response to the client
    if (response == NULL)
    {
        int j;
        for (j = 0; j < num_of_files; j++)
            free(dir_file_list[j]);
        free(dir_file_list);
        return make_response(500, NULL, fd);
    }
    bzero(response, size);
    sprintf(response, html_start, path, path);  //copy the start of the html content to the response string

    char* temp_path = NULL, *entity_href = NULL;
    for (i = 0; i < num_of_files; i++)   //go over all the folders and files in the path's folder
    {
        entity_name = dir_file_list[i]->d_name;  //get the name of the folder/file that is in the path folder

        if (i < 2)   //it's the folders "." and ".."
        {
            status = stat_func(&entity_stat, entity_name, fd);
            if (status != 1) {
                free(response);
                int j;
                for (j = 0; j < num_of_files; j++)
                    free(dir_file_list[j]);
                free(dir_file_list);
                return ERR_WAS_SEND;
            }
            entity_href = add_entity_quote(entity_name, 0); //add the quotation mark to the name of the folder
            if (entity_href == NULL)
            {
                free(response);
                int j;
                for (j = 0; j < num_of_files; j++)
                    free(dir_file_list[j]);
                free(dir_file_list);
                return make_response(500, NULL, fd);
            }

            last_modified = get_last_modified(entity_name);
            if (last_modified == NULL)
            {
                free(response);
                free(entity_href);
                int j;
                for (j = 0; j < num_of_files; j++)
                    free(dir_file_list[j]);
                free(dir_file_list);
                return make_response(500, NULL, fd);
            }
        }

        else  //i >= 2, all the folders and files in the path directory
        {
            temp_path = (char *)malloc(sizeof(char) * (strlen(path) + strlen(entity_name) + 1));  //temp_path is the append of path+entity_name
            if (temp_path == NULL)
            {
                free(response);
                int j;
                for (j = 0; j < num_of_files; j++)
                    free(dir_file_list[j]);
                free(dir_file_list);
                return make_response(500, NULL, fd);
            }
            bzero(temp_path, sizeof(char) * (strlen(path) + strlen(entity_name) + 1));
            sprintf(temp_path, "%s%s", path, entity_name);  //append the folder/file in the path's folder to the path itself
            status = stat_func(&entity_stat, temp_path, fd);
            if (status != 1) {
                free(response);
                free(temp_path);
                int j;
                for (j = 0; j < num_of_files; j++)
                    free(dir_file_list[j]);
                free(dir_file_list);
                return ERR_WAS_SEND;
            }
            if (S_ISDIR(entity_stat.st_mode))  //need to make the "A HREF" folder format
                entity_href = add_entity_quote(entity_name, 0); //add the quotation mark to the name of the folder
            else    //need to make the "A HREF" file format
                entity_href = add_entity_quote(entity_name, 1); //add the quotation mark to the name of the folder

            if (entity_href == NULL)
            {
                 free(response);
                 free(temp_path);
                 int j;
                 for (j = 0; j < num_of_files; j++)
                     free(dir_file_list[j]);
                 free(dir_file_list);
                 return make_response(500, NULL, fd);
            }

            last_modified = get_last_modified(temp_path);
            if (last_modified == NULL)
            {
                free(response);
                free(entity_href);
                free(temp_path);
                int j;
                for (j = 0; j < num_of_files; j++)
                    free(dir_file_list[j]);
                free(dir_file_list);
                return make_response(500, NULL, fd);
            }
        }


        if(S_ISDIR(entity_stat.st_mode) == 1)  //need size for folder entity presentation
            size+= (int)(strlen(FOLDER_INFO) + strlen(entity_href) + strlen(entity_name) + strlen(last_modified) + 1) ;
        else    //need size for file entity presentation
        {
            entity_size = entity_stat.st_size;  //get the size of the folder or file
            sprintf(f_size_to_str, "%d", entity_size);
            size += (int) (strlen(FILE_INFO) + strlen(f_size_to_str) + strlen(entity_href)+ strlen(entity_name) + strlen(last_modified) + 1);
        }
        old_size = (int)strlen(response);  //saves the size of char* response before realloc
        response = (char*)realloc(response, size);
        if (response == NULL)
        {
            free(last_modified);
            free(entity_href);
            if (i >= 2)
                free(temp_path);
            int j;
            for (j = 0; j < num_of_files; j++)
                free(dir_file_list[j]);
            free(dir_file_list);
            return make_response(500, NULL, fd);
        }
        bzero(response + old_size, size - old_size);

        if (S_ISDIR(entity_stat.st_mode) == 1) //build a string for a folder
            sprintf(response + strlen(response), FOLDER_INFO, entity_href, entity_name, last_modified);
        else   //build a string for a file
            sprintf(response + strlen(response), FILE_INFO, entity_href, entity_name, last_modified, f_size_to_str);

        free(last_modified);
        free(entity_href);
        if (i >= 2)
            free(temp_path);
    }  //end of loop

    strcat(response, html_end); //append the end of the html to the rest of it

    int j;
    for (j = 0; j < num_of_files; j++)
        free(dir_file_list[j]);
    free(dir_file_list);

    char* date = get_time();
    last_modified = get_last_modified(path);
    if (date == NULL || last_modified == NULL)
    {
        if (date != NULL)
            free(date);
        if (last_modified != NULL)
            free(last_modified);
        free(response);
        return make_response(500, NULL, fd);
    }

    int cont_len_sum_dig = num_to_str_size((int)strlen(response));  //get the number of digits of the content length of response
    int header_size =(int)(strlen(FILE_RESPONSE_TYPE) + strlen(date) + strlen(last_modified) + strlen("text/html") + cont_len_sum_dig + size);
    char* header_response = (char*)malloc(sizeof(char) * header_size);
    if (header_response == NULL)
    {
        free(date);
        free(last_modified);
        free(response);
        return make_response(500, NULL, fd);
    }
    bzero(header_response, sizeof(char) * header_size);

    sprintf(header_response, FILE_RESPONSE_TYPE, date, "text/html", (int)strlen(response), last_modified);
    #ifdef DEBUG
    printf("response = \r\n\r\n%s%s\r\n\r\n", header_response, response);
    #endif

    write_response(header_response, fd);  //write the headers to the client's socket
    status = write_response(response, fd);  //write the directory content to the client's socket
    free(date);
    free(last_modified);
    free(response);
    free(header_response);
    if (status == ERR_WAS_SEND)
        return ERR_WAS_SEND;
    return OK;
}

/**This function gets a number and returns the number of digits which this number contains**/
int num_to_str_size(int num)
{
    int counter = 0;
    while (num != 0) {
        num /= 10;
        counter++;
    }
    return counter;
}

/**This function gets a name of file/folder ,adds to it quotation mark, and returns it**/
char* add_entity_quote(char* entity_name, int flag)
{
    if (entity_name == NULL)
        return NULL;  //there is an internal server problem

    int size = 0;
    char* entity_quote = NULL;

    if (flag == 0)  //it's a folder
    {
        size = (int)strlen(entity_name) + 4;
        entity_quote = do_str_malloc(size);  //create array of char* to put the file/folder with the ""
        if (entity_quote == NULL)
            return NULL;
        sprintf(entity_quote, "\"%s/\"", entity_name);
    }
    else  //it's a file
    {
        size = (int)strlen(entity_name) + 3;
        entity_quote = do_str_malloc(size);  //create array of char* to put the file/folder with the ""
        if (entity_quote == NULL)
            return NULL;
        sprintf(entity_quote, "\"%s\"", entity_name);
    }
    return entity_quote;
}

/**This function does a malloc of char* and returns a pointer to it**/
char* do_str_malloc(int size)
{
    if (size <= 0)
        return NULL;
    char* str = (char*)malloc(sizeof(char) * size);
    if (str == NULL)
        return NULL;
    bzero(str, size);
    return str;
}

/**This function gets a path and checks the validation of this path, it returns if the path is valid or invalid**/
int check_path_permission(char *path, int fd)
{
    if (path == NULL)
        return make_response(500, NULL, fd);
    struct stat f_stat;
    char *path_copy = (char*)malloc(sizeof(char)*(strlen(path)+1));
    if (path_copy == NULL)
        return make_response(500, NULL, fd);  //need to send error 500 to the client

    char *folder = NULL;
    strcpy(path_copy, path);
    folder = strtok(path_copy, "/");  //cut the path by "/", to check the permissions of each folder in the path
    if (folder == NULL) {
        free(path_copy);
        return make_response(404, NULL, fd);
    }
    int status = 0;
    while (1)
    {
        status = stat_func(&f_stat, folder, fd);
        if(status != 1)
        {
            free(folder);
            return ERR_WAS_SEND;
        }

        if((S_ISDIR(f_stat.st_mode) == 1) && !(f_stat.st_mode & S_IXOTH))  //folder does not have execute permission to "other"
        {
            path_copy = strtok(NULL, "/");
            if (path_copy == NULL && strcmp(path + strlen(path)-1, "/") == 0 && (f_stat.st_mode & S_IROTH))  //it's the last folder in path, and it has "/" at the end and has read permission
            {
                free(folder);
                return OK;  //it means the the permissions of the path's folders are OK
            }
            else {
                free(folder);
                return make_response(403, NULL, fd);
            }
        }
        path_copy = strtok(NULL, "/");
        if (path_copy != NULL)
            sprintf(folder + strlen(folder), "/%s", path_copy);
        else
            break;
    }

    if (S_ISREG(f_stat.st_mode) && (f_stat.st_mode & S_IROTH)) //file is regular and has read permission to "other"
    {
        free(folder);
        return OK;    //it means the the permissions of the path's folders and file are OK
    }
    if (S_ISDIR(f_stat.st_mode) && strcmp(path+strlen(path)-1, "/") == 0 && (f_stat.st_mode & S_IROTH)) {
        free(folder);
        return OK;  //it means the the permissions of the path's folders are OK
    }
    free(folder);
    return make_response(403, NULL, fd);
}



/**This function makes the welcome socket of the server, and returns the fd of it**/
int make_socket_connection(char* argv[])
{
    int fd;		/*socket descriptor*/

    if((fd = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
        perror("ERROR: socket() has been failed\r\n");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in srv;	/* used by bind() */

    srv.sin_family = AF_INET; /* use the Internet addr family */

    srv.sin_port = htons(atoi(argv[1])); /* bind socket ‘fd’ to the given port*/


    srv.sin_addr.s_addr = htonl(INADDR_ANY);

    if(bind(fd, (struct sockaddr*) &srv, sizeof(srv)) < 0)  /* bind: a client may connect to any of my addresses */
    {
        perror("ERROR: bind() has been failed\r\n");
        exit(EXIT_FAILURE);
    }

    if(listen(fd, 5) < 0)  /*connect simultaneously to maximum 5 clients, other client will wait in queue*/
    {
        perror("ERROR: listen() has been failed\r\n");
        exit(EXIT_FAILURE);
    }
    return fd;
}


#define RESPONSE_302 "HTTP/1.0 %s\r\nServer: webserver/1.0\r\nDate: %s\r\nLocation: /%s%c\r\nContent-Type: %s\r\nContent-Length: %d\r\nConnection: close\r\n\r\n"
#define ERROR_RESPONSE "HTTP/1.0 %s\r\nServer: webserver/1.0\r\nDate: %s\r\nContent-Type: %s\r\nContent-Length: %d\r\nConnection: close\r\n\r\n"
#define HTML "<HTML><HEAD><TITLE>%s</TITLE></HEAD>\r\n<BODY><H4>%s</H4>\r\n%s\r\n</BODY></HTML>\r\n"
#define RES_302 "302 Found"
#define ERR_400 "400 Bad Request"
#define ERR_403 "403 Forbidden"
#define ERR_404 "404 Not Found"
#define ERR_500 "500 Internal Server Error"
#define ERR_501 "501 Not supported"
/**This function makes a response to the client if an error occurred during the building of the response, or in case of 302 found response**/
int make_response(int response_num, char* path, int fd)
{
    int size = 0;
    char* time = get_time();
    if (time == NULL)
    {
        printf("there was a problem while trying to make the response\r\n");
        return -1;
    }
    switch (response_num)
    {
        case 302 :
            size =(int)(strlen(RESPONSE_302) + strlen(HTML) + strlen(RES_302) + strlen(time) + strlen(path) + strlen("text/html") + 80);
            break;
        case 400:
            size = (int)(strlen(ERROR_RESPONSE) + strlen(HTML) + strlen(ERR_400) + strlen(time) + strlen("text/html") + 80);
            break;
        case 403 :
            size = (int)(strlen(ERROR_RESPONSE) + strlen(HTML) + strlen(ERR_403) + strlen(time) + strlen("text/html") + 80);
            break;
        case 404 :
            size = (int)(strlen(ERROR_RESPONSE) + strlen(HTML) + strlen(ERR_404) + strlen(time) + strlen("text/html") + 80);
            break;
        case 500 :
            size = (int)(strlen(ERROR_RESPONSE) + strlen(HTML) + strlen(ERR_500) + strlen(time) + strlen("text/html") + 80);
            break;
        case 501 :
            size = (int)(strlen(ERROR_RESPONSE) +  strlen(HTML) + strlen(ERR_501) + strlen(time) + strlen("text/html") + 80);
            break;
    }
    char *response = (char *)malloc(sizeof(char) * size);
    if (response == NULL)
    {
        free(time);
        printf("ERROR: cannot make an error response for the client\r\n");
        return -1;
    }
    bzero(response, size * sizeof(char));

    switch (response_num)
    {
        case 302 :
            sprintf(response, RESPONSE_302 HTML, RES_302, time, path,'/', "text/html", 123, RES_302, RES_302, "Directories must end with a slash.");
            break;
        case 400:
            sprintf(response, ERROR_RESPONSE HTML, ERR_400, time, "text/html", 113, ERR_400, ERR_400, "Bad Request.");
            break;
        case 403 :
            sprintf(response, ERROR_RESPONSE HTML, ERR_403, time, "text/html", 111, ERR_403, ERR_403, "Access denied.");
            break;
        case 404 :
            sprintf(response, ERROR_RESPONSE HTML, ERR_404, time, "text/html", 112, ERR_404, ERR_404, "File not found.");
            break;
        case 500 :
            sprintf(response, ERROR_RESPONSE HTML, ERR_500, time, "text/html", 144, ERR_500, ERR_500, "Some server side error.");
            break;
        case 501 :
            sprintf(response, ERROR_RESPONSE HTML, ERR_501, time, "text/html", 129, ERR_501, ERR_501, "Method is not supported.");
            break;
    }
    free(time);
    #ifdef DEBUG
    printf("response = \r\n\n%s\r\n\r\n", response);
    #endif
    write_response(response, fd);
    free(response);
    if (response_num == 302)
        return OK;
    return ERR_WAS_SEND;
}


/**This function sends the time which the response is send**/
char* get_time()
{
    time_t now;
    char* time_buffer = (char*)malloc(128*sizeof(char));
    if (time_buffer == NULL)
    {
        return NULL;
    }
    bzero(time_buffer, 128*sizeof(char));

    now = time(NULL);
    strftime(time_buffer, 128, RFC1123FMT, gmtime(&now));

    return time_buffer;   //time_buffer holds the correct format of the current time.
}


/**This function sends the mime type of a file**/
char* get_mime_type(char *name) {
    char *ext = strrchr(name, '.');
    if (!ext) return NULL;
    if (strcmp(ext, ".html") == 0 || strcmp(ext, ".htm") == 0) return "text/html";
    if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0) return "image/jpeg";
    if (strcmp(ext, ".gif") == 0) return "image/gif";
    if (strcmp(ext, ".png") == 0) return "image/png";
    if (strcmp(ext, ".css") == 0) return "text/css";
    if (strcmp(ext, ".au") == 0) return "audio/basic";
    if (strcmp(ext, ".wav") == 0) return "audio/wav";
    if (strcmp(ext, ".avi") == 0) return "video/x-msvideo";
    if (strcmp(ext, ".mpeg") == 0 || strcmp(ext, ".mpg") == 0) return "video/mpeg";
    if (strcmp(ext, ".mp3") == 0) return "audio/mpeg";
    return NULL;
}


/**This function sends the time which the file or folder was modified for the last time**/
char* get_last_modified(char *file)
{
    struct tm *clock;
    struct stat attr;

    char* time_buffer = (char*)malloc(128*sizeof(char));
    if (time_buffer == NULL)
    {
        return NULL;
    }
    bzero(time_buffer, 128*sizeof(char));

    stat(file, &attr);
    clock = gmtime(&(attr.st_mtime));
    strftime(time_buffer, 128, RFC1123FMT, clock);
    return time_buffer;
}


/**This function checks if there is a non digit char in a string**/
int is_only_digits(char* str)
{
    if (str != NULL)
    {
        char* ptr_check = str;
        while(*ptr_check != '\0')
        {
            if (*ptr_check < '0' || *ptr_check > '9')  //if returns 1 (true), then we have a non digit char in the port
                return -1;  //there is a non digit char in the string 'str'

            ptr_check+= 1*sizeof(char);
        }
        return 1;  //there is not a non digit char in the string 'str'
    }
    else
        return -1;  //the string 'str' is NULL
}