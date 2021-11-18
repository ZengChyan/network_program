/*
 * This program displays the names of all files in the current directory.
 */

#include <dirent.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    DIR *d;
    struct dirent *dir;
    d = opendir("./file");
    FILE *fp = fopen("www/show_uploaded.html", "a+");
    char buf[1000];
    fseek(fp, 0, SEEK_END);

    char end_buf[1000] = {"</tbody></table></table></div></div></div></html>"};
    char write[512];
    int counter = 0;
    if (d)
    {
        while ((dir = readdir(d)) != NULL)
        {

            printf("%s\n", dir->d_name);
            if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0)
                continue;
            counter++;
            sprintf(write, "<tr><th scope="
                           "row"
                           ">%d</th><td>%s</td><td><a href="
                           "/file/%s"
                           ">%s</a></td></tr>",
                    counter, dir->d_name, dir->d_name, dir->d_name);
            for (int i = 0; i < strlen(write); i++)
            {
                fputc(write[i], fp);
            }
        }
        for (int i = 0; i < strlen(end_buf); i++)
        {
            fputc(end_buf[i], fp);
        }
        closedir(d);
    }
    return (0);
}