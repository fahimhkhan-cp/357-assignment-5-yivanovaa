#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <fcntl.h>

#define MAX_BUFFER_SIZE 1024
#define RESPONSE_BUFFER_SIZE 2048

// Functions
void handle_request(int client_fd);
void send_response(int client_fd, const char *status, const char *content_type, const char *content, size_t content_length);
void send_error_response(int client_fd, const char *status, const char *message);
void send_file_response(int client_fd, const char *filename);

int main(int argc, char *argv[]) {
    if (argc != 2) { //validate arguments
        fprintf(stderr, "Invalid argument count");
        return 1;
    }

    int port = atoi(argv[1]);
    if (port < 1024 || port > 65535) {
        fprintf(stderr, "Port number has to be between 1024 and 65535.\n");
        return 1;
    }

    //Initialize variables
    int server_socket, client_socket;
    struct sockaddr_in server_address, client_address;
    socklen_t client_len = sizeof(client_address);

    //server socket
    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    //family address, IP address and port
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(port);

    //Bind or error message
    if (bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
        perror("Bind failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    //if everything is ok, print message
    printf("Server is listening on port %d...\n", port);

    //listen on the specified port
    if (listen(server_socket, 10) < 0) {
        perror("Listen failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    //accept requests
    while (1) {
        client_socket = accept(server_socket, (struct sockaddr *)&client_address, &client_len);
        
        if (client_socket < 0) {
            perror("accept request failed");
            continue;
        }

        // proces the request
        handle_request(client_socket);

        close(client_socket); // close client socket after
    }

    close(server_socket); //close server soket
    return 0;
}

// handle get/head request
void handle_request(int client_fd) {
    char buffer[MAX_BUFFER_SIZE];
    ssize_t nread;

    nread = read(client_fd, buffer, sizeof(buffer) - 1);
    if (nread <= 0) {
        send_error_response(client_fd, "400", "Bad Request");
        return;
    }
    buffer[nread] = '\0';  // Null-termination

    char method[5], filename[100], version[100]; //to store request method, path name and version(idk how much the lentgh should be)
    if (sscanf(buffer, "%s %s %s", method, filename, version) != 3) {
        send_error_response(client_fd, "400", "Bad Request");
        return;
    }

    //check for contents outside the current dir and send no permission message
    if (strstr(filename, "..")) {
        send_error_response(client_fd, "403", "Permission Denied");
        return;
    }

    //execute actual request
    //-------------NEEDS IMPROVEMENT---------------
    if (strcmp(method, "GET") == 0 || strcmp(method, "HEAD") == 0) {
        send_file_response(client_fd, filename + 1); // remove '/' from beginning
        if (strcmp(method, "HEAD") == 0) {
            return; //if head, do not send contents
        }
    } else {
        send_error_response(client_fd, "501", "Not Implemented");
    }
}

// response to client
void send_response(int client_fd, const char *status, const char *content_type, const char *content, size_t content_length) {
    dprintf(client_fd, "HTTP/1.0 %s\r\n", status);
    dprintf(client_fd, "Content-Type: %s\r\n", content_type);
    dprintf(client_fd, "Content-Length: %zu\r\n", content_length);
    dprintf(client_fd, "\r\n");

    if (content && content_length > 0) {
        write(client_fd, content, content_length);
    }
}

//error response
void send_error_response(int client_fd, const char *status, const char *message) {
    char body[RESPONSE_BUFFER_SIZE];
    snprintf(body, sizeof(body), "<html><body><h1>%s</h1><p>%s</p></body></html>", status, message);
    send_response(client_fd, status, "text/html", body, strlen(body));
}

//file contents as response
void send_file_response(int client_fd, const char *filename) {
    struct stat file_stat;
    if (stat(filename, &file_stat) < 0) {
        send_error_response(client_fd, "404", "Not Found"); //non-existent
        return;
    }

    //check the permissions
    if (access(filename, R_OK) != 0) {
        send_error_response(client_fd, "403", "Permission Denied");
        return;
    }

    //open the file
    int file_fd = open(filename, O_RDONLY);
    if (file_fd < 0) {
        send_error_response(client_fd, "500", "Internal Error");
        return;
    }

    char file_buffer[MAX_BUFFER_SIZE];
    ssize_t bytes_read;

    // header as first response
    send_response(client_fd, "200 OK", "text/html", NULL, file_stat.st_size);

    //file content
    while ((bytes_read = read(file_fd, file_buffer, sizeof(file_buffer))) > 0) {
        write(client_fd, file_buffer, bytes_read);
    }

    close(file_fd);
}
