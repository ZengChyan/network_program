#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <ctype.h>

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

int main(int argc, char *argv[])
{
    int socket_desc, new_socket, c;
    struct sockaddr_in server, client;
    char message[10000];
    long ret;

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

        // int read_len = read(new_socket, message, 10000);

        int des_fd = open("test.txt", O_WRONLY | O_CREAT, 0700);
        // int read_len = read(new_socket, message, 2000);

        char buf[1024];
        int index = 0;

        int status = 0;
        char single_word;

        int numchars = 0;

        char header_info[100][100], word[100];

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

            sprintf(path, "www%s", header_info[1]);

            if (path[strlen(path) - 1] == '/')
            {
                strcat(path, "index.html");
            }

            int file_fd;
            if ((file_fd = open(path, O_RDONLY)) == -1)
            {
                write(new_socket, "Failed to open file", 19);
                perror("open file error");
            }

            off_t size = lseek(file_fd, 0, SEEK_END);
            lseek(file_fd, 0, SEEK_SET);

            char header[1000];

            sprintf(header, "Content-length: %d\n", size);

            write(new_socket, "HTTP/1.1 200 OK\n", strlen("HTTP/1.1 200 OK\n"));
            write(new_socket, header, strlen(header));

            char content_type_str[128];

            content_type(path, content_type_str);

            printf("%s\n", content_type_str);

            write(new_socket, content_type_str, strlen(content_type_str));

            while ((ret = read(file_fd, message, 1000)) > 0)
            {
                write(new_socket, message, ret);
            }

            // reload many times, it would crash
            close(file_fd);
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
                // printf("%s\n", buf);

                // if (boundary_ptr != NULL && strstr(buf, boundary_ptr) != NULL)
                // {
                //     char filename[512];
                //     int start_store_file = 0;
                //     int fd;
                //     while ((numchars = get_line(new_socket, buf, sizeof(buf))) && numchars > 0)
                //     {
                //         // if (strstr(buf, "filename") != NULL)
                //         // {
                //         //     char *ptr = buf;
                //         //     word_counter = 0;
                //         //     char filename_[10][512];

                //         //     memset(word, 0, sizeof(word));
                //         //     while ((ptr = getword(ptr, word)) != NULL)
                //         //     {
                //         //         strcpy(filename_[word_counter++], word);
                //         //         memset(word, '\0', sizeof(word));
                //         //     }
                //         //     // boundary[2] -> filename
                //         //     strcpy(filename, filename_[3]);
                //         //     printf("%s\n", filename);
                //         //     numchars = get_line(new_socket, buf, sizeof(buf));

                //         //     // printf("1%s\n", buf);
                //         //     numchars = get_line(new_socket, buf, sizeof(buf));
                //         //     // start_store_file = 1;
                //         //     // fd = open("tempfile.pdf", O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
                //         //     // continue;

                //         //     break;
                //         //     // printf("2%s\n", buf);
                //         //     // printf("4%s\n", buf);
                //         // }

                //         // if (start_store_file)
                //         // {

                //         //     if (strstr(buf, boundary_ptr) != NULL)
                //         //     {
                //         //         start_store_file = 0;
                //         //         printf("Hello~%s\n", buf);
                //         //         break;
                //         //     }
                //         //     else
                //         //     {
                //         //         write(fd, buf, numchars);
                //         //     }
                //         // }

                //         // if (strstr(buf, "\r\n\r\n") != NULL)
                //         // {
                //         //     printf("hi\n");
                //         // }
                //         // printf("Hi\n");
                //         // printf("next:%s\n", buf);

                //         printf("begin of boundary\n");
                //         break;
                //     }
                // }
            }

            char message[1000005];
            char *ptr;
            int counter = 0;
            int _counter = 0;
            int fd = open("tempfile.pdf", O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);

            get_line(new_socket, buf, sizeof(buf));
            get_line(new_socket, buf, sizeof(buf));

            while ((numchars > 0) && strcmp("\n", buf)) /* read & discard headers */
                numchars = get_line(new_socket, buf, sizeof(buf));
            get_line(new_socket, buf, sizeof(buf));
            while ((numchars > 0) && strcmp("\n", buf)) /* read & discard headers */
                numchars = get_line(new_socket, buf, sizeof(buf));

            int message_counter = 0;
            int finished = 0;
            while ((numchars = recv(new_socket, &message, 1000000, 0)) && numchars > 0)
            {
                // printf("%d\n", numchars);

                // printf("%s\n", message);

                // printf("%s\n", ptr);
                message_counter = 0;
                char *ptr = my_memmem(message, numchars, boundary_ptr, strlen(boundary_ptr));
                char *ori = message;
                if(ptr != NULL){
                    // printf("大小為 %d\n", ptr - ori);

                    // printf("找到 %d\n", ptr - message);
                    // ptr -= 4;

                    write(fd, message, ptr - message - 4);
                    char path[512];
                        sprintf(path, "www%s", header_info[1]);

                        if (path[strlen(path) - 1] == '/')
                        {
                            strcat(path, "index.html");
                        }
                        int file_fd;
                        if ((file_fd = open(path, O_RDONLY)) == -1)
                        {
                            write(new_socket, "Failed to open file", 19);
                            perror("open file error");
                        }

                        off_t size = lseek(file_fd, 0, SEEK_END);
                        lseek(file_fd, 0, SEEK_SET);
                        char header[1000];

                        sprintf(header, "Content-length: %d\n", size);

                        write(new_socket, "HTTP/1.1 200 OK\n", strlen("HTTP/1.1 200 OK\n"));
                        write(new_socket, header, strlen(header));

                        char content_type_str[128];

                        content_type(path, content_type_str);

                        printf("高歌離席\n");

                        write(new_socket, content_type_str, strlen(content_type_str));

                        while ((ret = read(file_fd, message, 1000)) > 0)
                        {
                            write(new_socket, message, ret);
                        }

                        // reload many times, it would crash
                        close(file_fd);
                        close(fd);
                    break;
                }else{
                    write(fd, message, numchars);
                }
                // printf("content: %d\n", content_length);
                // while (counter < numchars)
                // {
                //     if (message[message_counter] == '-')
                //     {
                //         _counter++;
                //     }
                //     else if (_counter > 0 && message[message_counter] != '-')
                //     {
                //         // printf("ININDER %d\n", _counter);;
                //         char _str[1] = {"-"};
                //         for (int i = 0; i < _counter; i++)
                //             write(fd, _str, 1);
                //         _counter = 0;
                //     }
                //     // printf("%d ", counter);
                //     if (_counter == 5)
                //     {
                //         char path[512];
                //         sprintf(path, "www%s", header_info[1]);

                //         if (path[strlen(path) - 1] == '/')
                //         {
                //             strcat(path, "index.html");
                //         }
                //         int file_fd;
                //         if ((file_fd = open(path, O_RDONLY)) == -1)
                //         {
                //             write(new_socket, "Failed to open file", 19);
                //             perror("open file error");
                //         }

                //         off_t size = lseek(file_fd, 0, SEEK_END);
                //         lseek(file_fd, 0, SEEK_SET);
                //         char header[1000];

                //         sprintf(header, "Content-length: %d\n", size);

                //         write(new_socket, "HTTP/1.1 200 OK\n", strlen("HTTP/1.1 200 OK\n"));
                //         write(new_socket, header, strlen(header));

                //         char content_type_str[128];

                //         content_type(path, content_type_str);

                //         printf("高歌離席\n");

                //         write(new_socket, content_type_str, strlen(content_type_str));

                //         while ((ret = read(file_fd, message, 1000)) > 0)
                //         {
                //             write(new_socket, message, ret);
                //         }

                //         // reload many times, it would crash
                //         close(file_fd);
                //         close(fd);
                //         finished = 1;
                //         break;
                //     }
                //     if (message[message_counter] == '\n' && message[message_counter + 1] == '-' && message[message_counter + 2] == '-')
                //     {
                //         printf("fuck\n");
                //     }
                //     if (_counter == 0)
                //     {
                //         if (!(message[message_counter] == '\r' && message[message_counter + 1] == '\n' && message[message_counter + 2] == '-') && !(message[message_counter] == '\n' && message[message_counter + 1] == '-' && message[message_counter + 2] == '-'))
                //         {
                //             char ptr[1];
                //             ptr[0] = message[message_counter];
                //             write(fd, ptr, 1);
                //         }
                //     }
                //     counter++;
                //     message_counter++;
                // }
                // counter = 0;
                // if (finished)
                //     break;
            }

            // numchars = get_line(new_socket, buf, sizeof(buf));

            // printf("123%s\n", buf);

            // while (recv(new_socket, &single_word, 1, 0) > 0)
            // {
            //     if (single_word == '\r')
            //     {
            //         if (recv(new_socket, &single_word, 1, 0) > 0 && single_word == '\n')
            //         {
            //             if (recv(new_socket, &single_word, 1, 0) > 0 && single_word == '\r')
            //             {
            //                 if (recv(new_socket, &single_word, 1, 0) > 0 && single_word == '\n')
            //                 {
            //                     break;
            //                 }
            //             }
            //         }
            //     }
            //     printf("%c", single_word);
            // }

            // int fd = open("tempfile.pdf", O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
            // printf("\nBody:\n");
            // while (recv(new_socket, &single_word, 1, 0) > 0)
            // {
            //     if (single_word == '\r' && recv(new_socket, &single_word, 1, 0) > 0)
            //     {
            //         if (single_word == '\n' && recv(new_socket, &single_word, 1, 0) > 0)
            //         {
            //             if (single_word == '\r' && recv(new_socket, &single_word, 1, 0) > 0)
            //             {
            //                 if (single_word == '\n')
            //                 {
            //                     break;
            //                 }
            //             }
            //         }
            //     }
            // }

            // while (counter <= 999999999 && (recv(new_socket, &message, 1, 0) > 0))
            // {

            //     if (message[0] == '-')
            //     {
            //         // printf("Hi\n");
            //         memset(message, 0, sizeof(message));
            //         recv(new_socket, &message, 27, 0);
            //         if (strcmp(message, "---------------------------") == 0)
            //         {
            //             printf("%s\n", message);
            //             // recv(new_socket, &message, 100, 0);
            //             // printf("%s\n", message);
            //             printf("88\n");
            //             break;
            //         }
            //         else
            //         {
            //             write(fd, message, 27);
            //             // printf("hi%d\n", counter);
            //             // printf("%s", message);
            //             counter++;
            //             continue;
            //         }
            //         // if (message[0] == '\n' && recv(new_socket, &message, 1, 0) > 0)
            //         // {
            //         //     if (message[0] == '\r' && recv(new_socket, &message, 1, 0) > 0)
            //         //     {
            //         //         if (message[0] == '\n')
            //         //         {
            //         //             printf("88\n");
            //         //             break;
            //         //         }
            //         //     }
            //         // }
            //     }
            //     // printf("%c", message[0]);
            //     write(fd, message, 1);
            //     // printf("%d\n", counter);
            //     counter++;
            //     memset(message, 0, sizeof(message));
            //     // printf("hi\n");
            // }

            if (!des_fd)
            {
                perror("dest_fd error : ");
                break;
            }
            counter = 0;
            // while ((ret = read(new_socket, message, 10000)) > 0)
            // {
            //     write(des_fd, message, ret);
            //     printf("ret: %d\n", ret);
            //     counter += ret;

            //     // break;
            // }

            //Reply to the client
            //message = "Hello Client , I have received your connection. But I have to go now, bye\n";
            // message = "我好帥";
            // sprintf(message, "<html><head><meta charset=%s></head>我好帥</html>", "utf8");

            // int file_fd;
            // if ((file_fd = open("index.html", O_RDONLY)) == -1)
            // {
            //     write(new_socket, "Failed to open file", 19);
            //     perror("open file error");
            // }

            // off_t size = lseek(file_fd, 0, SEEK_END);
            // lseek(file_fd, 0, SEEK_SET);
            // printf("hello\n");
            // sprintf(header, "Content-length: %d\n", size);

            // write(new_socket, "HTTP/1.1 200 OK\n", strlen("HTTP/1.1 200 OK\n"));
            // write(new_socket, header, strlen(header));
            // write(new_socket, "Content-Type: text/html\n\n", 25);

            // while ((ret = read(file_fd, message, 8000)) > 0)
            // {
            //     write(new_socket, message, ret);
            // }
            close(new_socket);
        }

        if (new_socket < 0)
        {
            perror("accept failed");
            return 1;
        }
    }
    return 0;
}