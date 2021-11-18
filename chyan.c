#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <ctype.h>
#include <dirent.h> 

void content_type(char *path, char *str)
{
    printf("path: %s\n", path);
    if (strstr(path, ".ico") != NULL)
    {
        strcpy(str, "Content-Type: image/x-icon\n\n");
        return;
    }
    if (strstr(path, ".html") != NULL)
    {
        strcpy(str, "Content-Type: text/html\n\n");
        return;
    }
    if (strstr(path, ".mp4") != NULL)
    {
        strcpy(str, "Content-Type: video/mp4\n\n");
        return;
    }
    if (strstr(path, ".png") != NULL)
    {
        strcpy(str, "Content-Type: image/png\n\n");
        return;
    }
    if (strstr(path, ".jpg") != NULL)
    {
        strcpy(str, "Content-Type: image/jpeg\n\n");
        return;
    }
    if (strstr(path, ".pdf") != NULL)
    {
        strcpy(str, "Content-Type: application/pdf\n\n");
        return;
    }
    if (strstr(path, ".mp3") != NULL)
    {
        strcpy(str, "Content-Type: audio/mp3\n\n");
        return;
    }
    if (strstr(path, ".txt") != NULL)
    {
        strcpy(str, "Content-Type: text/plain\n\n");
        return;
    }
    if (strstr(path, ".docx") != NULL)
    {
        strcpy(str, "Content-Type: application/msword\n\n");
        return;
    }
}

void bad_request(int client)
{
    char buf[1024];

    sprintf(buf, "HTTP/1.0 400 BAD REQUEST\r\n");
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "Content-type: text/html\r\n");
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "<P>Your browser sent a bad request, ");
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "such as a POST without a Content-Length.\r\n");
    send(client, buf, sizeof(buf), 0);
}

void not_found(int socket)
{
    printf("hi\n");
    char buf[1024];
    long ret;
    int file_fd = open("www/404.html", O_RDONLY);
    char message[10000];

    off_t size = lseek(file_fd, 0, SEEK_END);
    lseek(file_fd, 0, SEEK_SET);

    char header[1000];

    sprintf(header, "Content-length: %lld\n", size);

    write(socket, "HTTP/1.1 200 OK\n", strlen("HTTP/1.1 200 OK\n"));
    write(socket, header, strlen(header));

    write(socket, "Content-Type: text/html\n\n", strlen("Content-Type: text/html\n\n"));

    while ((ret = read(file_fd, message, 1000)) > 0)
    {
        write(socket, message, ret);
    }

    // reload many times, it would crash
    close(file_fd);
}

/*
 * The memmem() function finds the start of the first occurrence of the
 * substring 'needle' of length 'nlen' in the memory area 'haystack' of
 * length 'hlen'.
 *
 * The return value is a pointer to the beginning of the sub-string, or
 * NULL if the substring is not found.
 */
void *my_memmem(const void *haystack, size_t hlen, const void *needle, size_t nlen)
{
    int needle_first;
    const void *p = haystack;
    size_t plen = hlen;

    if (!nlen)
        return NULL;

    needle_first = *(unsigned char *)needle;

    while (plen >= nlen && (p = memchr(p, needle_first, plen - nlen + 1)))
    {
        if (!memcmp(p, needle, nlen))
            return (void *)p;

        p++;
        plen = hlen - (p - haystack);
    }

    return NULL;
}

char *getword(char *str, char *word)
{
    char *ptr = str, *qtr = word;
    char *textend = ptr + strlen(str);

    while (isspace(*ptr))
        ptr++;
    while (!isspace(*ptr) && ptr < textend)
    {
        *qtr++ = *ptr++;
    }

    *qtr = '\0';
    if (word[0] == '\0')
        return NULL;
    else
        return ptr;
}

int get_line(int sock, char *buf, int size)
{
    int i = 0;
    char c = '\0';
    int n;

    while ((i < size - 1) && (c != '\n'))
    {
        n = recv(sock, &c, 1, 0);
        /* DEBUG printf("%02X\n", c); */
        if (n > 0)
        {
            if (c == '\r')
            {
                n = recv(sock, &c, 1, MSG_PEEK);
                /* DEBUG printf("%02X\n", c); */
                if ((n > 0) && (c == '\n'))
                    recv(sock, &c, 1, 0);
                else
                    c = '\n';
            }
            buf[i] = c;
            i++;
        }
        else
            c = '\n';
    }
    buf[i] = '\0';

    return (i);
}

void response(int socket, char *file_path)
{
    char message[10000];
    long ret;
    int file_fd;
    printf("path: %s\n", file_path);
    if ((file_fd = open(file_path, O_RDONLY)) == -1)
    {
        not_found(socket);
        perror("open file error");
        return;
    }

    off_t size = lseek(file_fd, 0, SEEK_END);
    lseek(file_fd, 0, SEEK_SET);

    char header[1000];

    sprintf(header, "Content-length: %lld\n", size);

    write(socket, "HTTP/1.1 200 OK\n", strlen("HTTP/1.1 200 OK\n"));
    write(socket, header, strlen(header));

    char content_type_str[128];

    content_type(file_path, content_type_str);

    printf("%s\n", content_type_str);

    write(socket, content_type_str, strlen(content_type_str));

    while ((ret = read(file_fd, message, 1000)) > 0)
    {
        write(socket, message, ret);
    }

    // reload many times, it would crash
    close(file_fd);
}

int main(int argc, char *argv[])
{
    int socket_desc, new_socket, c, pid;
    struct sockaddr_in server, client;
    char message[10000];

    srand(time(NULL));

    //Create socket
    socket_desc = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (socket_desc == -1)
    {
        printf("Could not create socket");
    }

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    int random_port = rand() % 60000;
    server.sin_port = htons(random_port);

    //Bind
    if (bind(socket_desc, (struct sockaddr *)&server, sizeof(server)) < 0)
    {
        puts("bind failed");
        return 1;
    }
    puts("bind done");
    printf("%d\n", random_port);

    listen(socket_desc, 5);

    puts("Waiting for incoming connections...");
    c = sizeof(struct sockaddr_in);
    while ((new_socket = accept(socket_desc, (struct sockaddr *)&client, (socklen_t *)&c)))
    {
        puts("Connection accepted");
        char buf[1024];
        int numchars = 0;
        char header_info[100][100], word[100];
        if ((pid = fork()) < 0)
        {
            exit(0);
        }
        else
        {
            if (pid == 0)
            {
                close(socket_desc);
                /*
            0 -> (default) GET
            1 -> POST
        */
                int method = 0;

                // get header first
                numchars = get_line(new_socket, buf, sizeof(buf));
                char *ptr = buf;
                int word_counter = 0;
                while ((ptr = getword(ptr, word)) != NULL)
                {
                    strcpy(header_info[word_counter++], word);
                    memset(word, '\0', sizeof(word));
                }

                for (int i = 0; i < word_counter; i++)
                {
                    printf("%s\n", header_info[i]);
                }

                // check for GET / POST
                if (strcmp(header_info[0], "POST") == 0)
                {
                    method = 1;
                }

                if (!method)
                {
                    //    GET
                    char path[512];

                    printf("%s\n", header_info[1]);

                    if (strncmp(header_info[1], "/file/", 6) == 0)
                    {
                        for (int i = 1; i < strlen(header_info[1]); i++)
                        {
                            path[i - 1] = header_info[1][i];
                        }
                    }
                    else
                    {
                        sprintf(path, "www%s", header_info[1]);
                        if (path[strlen(path) - 1] == '/')
                        {
                            strcat(path, "index.html");
                        }
                    }

                    if(strncmp(header_info[1], "/show_uploaded", 14) == 0){
                        system("rm www/show_uploaded.html");
                        system("cp www/show_uploaded.html.bak www/show_uploaded.html");
                        system("./show_upload");
                    }

                    response(new_socket, path);
                }

                // POST
                if (method)
                {
                    int content_length = 0;
                    char *boundary_ptr = NULL;
                    while ((numchars = get_line(new_socket, buf, sizeof(buf))) && numchars > 0)
                    {
                        if (strstr(buf, "Content-Length") != NULL)
                        {
                            content_length = atoi(&buf[16]);
                        }

                        if (content_length == -1)
                        {
                            bad_request(new_socket);
                        }
                        char boundary[10][512];

                        if (strstr(buf, "boundary=") != NULL)
                        {
                            char *ptr = buf;
                            word_counter = 0;

                            memset(word, 0, sizeof(word));
                            while ((ptr = getword(ptr, word)) != NULL)
                            {
                                strcpy(boundary[word_counter++], word);
                                memset(word, '\0', sizeof(word));
                            }

                            // boundary[2] -> boundary
                            boundary_ptr = boundary[2] + 9;
                            break;
                        }
                    }

                    char message[1000005];
                    char *ptr;
                    int counter = 0;
                    char filename[512];

                    get_line(new_socket, buf, sizeof(buf));
                    get_line(new_socket, buf, sizeof(buf));

                    while ((numchars > 0) && strcmp("\n", buf))
                    { /* read & discard headers */
                        numchars = get_line(new_socket, buf, sizeof(buf));
                    }
                    get_line(new_socket, buf, sizeof(buf));
                    while ((numchars > 0) && strcmp("\n", buf))
                    { /* read & discard headers */
                        numchars = get_line(new_socket, buf, sizeof(buf));

                        ptr = strstr(buf, "filename=");
                        if (ptr != NULL)
                        {
                            for (char *i = ptr + 10; i < buf + strlen(buf) - 2; i++)
                            {
                                filename[counter++] = *i;
                            }
                        }
                    }

                    char file_path[512] = {0};
                    strcat(file_path, "./file/");
                    strcat(file_path, filename);
                    int fd = open(file_path, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
                    if (!fd)
                    {
                        perror("dest_fd error : ");
                    }
                    int message_counter = 0;
                    int finished = 0;
                    while ((numchars = recv(new_socket, &message, 1000000, 0)) && numchars > 0)
                    {
                        message_counter = 0;
                        char *ptr = my_memmem(message, numchars, boundary_ptr, strlen(boundary_ptr));
                        char *ori = message;

                        if (ptr != NULL)
                        {
                            write(fd, message, ptr - message - 4);

                            response(new_socket, "www/upload.html");

                            // reload many times, it would crash
                            close(fd);
                            memset(file_path, 0, strlen(file_path));
                            memset(filename, 0, strlen(filename));
                            break;
                        }
                        else
                        {
                            write(fd, message, numchars);
                        }
                    }

                    // close(new_socket);

                    if (new_socket < 0)
                    {
                        perror("accept failed");
                        return 1;
                    }
                }
                exit(0);
            }
            else //parent
            {
                close(new_socket);
            }
        }
    }
    return 0;
}