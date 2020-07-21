#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "compat.h"

#ifdef _WIN32 

#include <errno.h>
#include <io.h>
#include <winsock2.h>

int wstartup()
{
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2,2),&wsa) != 0)
    {
        fprintf(stderr, "wsastartup failed!\n");
        return 1;
    }
    return 0;
}  

int checkblock(int unused)
{
    if (WSAGetLastError() == WSAEWOULDBLOCK)
        return 0;
    fprintf(stderr, "%s error!\n", __func__ );
    return 1;
}

void wcleanup()
{
    WSACleanup();
}

void make_nonblocking(SOCKET s)
{
  unsigned long one = 1;
  ioctlsocket(s, FIONBIO, &one);
}


int exec(char *cmd, ...)
{
    int i, cnt = 0;
    char *ptr = NULL;
    char **arglist;
    va_list lst;
    va_list lst_2;
    va_start(lst, cmd);
    va_copy(lst_2, lst);
    ptr = va_arg(lst, char*);
    while (ptr != NULL)
    {
        cnt++;
        ptr = va_arg(lst, char*);
    }
    va_end(lst);

    arglist = malloc(sizeof(char*) * (cnt + 2));
    
    arglist[0] = cmd;

    i = 1;
    ptr = va_arg(lst_2, char*);
    while (ptr != NULL)
    {
        arglist[i++] = ptr;
        ptr = va_arg(lst_2, char*);
    }
    arglist[i] = NULL;
    va_end(lst_2);

    intptr_t ret = _spawnvp(_P_NOWAIT, cmd, (const char**)arglist);
    if (ret == -1)
    {
        return 0;
    }
    return ret;
}

void wait_app(PID_TYPE pid)
{
    if (pid != 0)
    {
        WaitForSingleObject((HANDLE)pid, INFINITE);
    }
}

void for_each_file(const char *folder, void (*fn) (const char* file, void *context), void *context)
{
    struct _finddata_t data = {0};
    char text_buf[256] = {0};
    snprintf(text_buf, 255, "%s/*.*", folder);
    intptr_t find_handle = _findfirst(text_buf, &data);
    
    if (find_handle == -1)
    {
        if (errno != ENOENT)
        {
            fprintf(stderr, "findfirst failed: %d", errno);
        }
        return;
    }
    while (1)
    {
        if ((data.attrib & _A_SUBDIR) == 0)
        {
            if (strlen(folder) + strlen(data.name) >= 250)
            {
                fprintf(stderr, "buffer overflow on %s\n", data.name);
                continue;
            }
            // Here is warning
            snprintf(text_buf, 250, "%s/%s", folder, data.name);
            fn(text_buf, context);
        }
        if (_findnext(find_handle, &data) == -1)
        {
            if (errno != ENOENT)
            {
                fprintf(stderr, "findnext failed: %d", errno);
            }
            break;
        }
    }
    _findclose(find_handle);
}

#else
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
#include <dirent.h>

int wstartup() { return 0; }
void wcleanup() {}

#define SOCKET_ERROR -1
#define INVALID_SOCKET -1

typedef int SOCKET;

int checkblock(int rval)
{
  if (errno == EAGAIN)
    return 0;
  fprintf(stderr, "%s error! %d, %d\n", __func__ , EAGAIN, rval);
  return 1;
}

void make_nonblocking(SOCKET s)
{
    int flags = fcntl(s, F_GETFL, 0);
    flags |= O_NONBLOCK;
    fcntl(s, F_SETFL, flags);
}

void for_each_file(const char *folder, void (*fn) (const char* file, void *context), void *context)
{
    DIR *dir = opendir(folder);
    for (struct dirent *ent = readdir(dir); ent != NULL; ent = readdir(dir))
    {
        if (ent->d_type == DT_REG)
        {
            char text_buf[256];
            if (strlen(folder) + strlen(ent->d_name) >= 250)
            {
                fprintf(stderr, "buffer overflow on %s\n", ent->d_name);
                continue;
            }
            // Here is warning
            snprintf(text_buf, 250, "%s/%s", folder, ent->d_name);
            fn(text_buf, context);
        }
    }
    closedir(dir);
}

int exec(char *cmd, ...)
{
    int i, cnt = 0, ret, fd;
    char *ptr = NULL;
    char **arglist = NULL;
    va_list lst;
    va_list lst2;
    va_start(lst, cmd);
    va_copy(lst2, lst);
    ptr = va_arg(lst, char*);
    while (ptr != NULL)
    {
        cnt++;
        ptr = va_arg(lst, char*);
    }
    va_end(lst);

    arglist = malloc(sizeof(char*) * (cnt + 2));
    
    arglist[0] = cmd;

    va_start(lst2, cmd);
    i = 1;
    ptr = va_arg(lst2, char*);
    while (ptr != NULL)
    {
        arglist[i++] = ptr;
        ptr = va_arg(lst2, char*);
    }
    arglist[i] = NULL;
    va_end(lst2);

    ret = fork();
    if (ret == 0)
    {
        fd = creat("run.log", 0664); //close stdout
        dup2(fd, 1);
        dup2(fd, 2);
        close(fd);
        ret = execvp(cmd, arglist);
        fprintf(stderr, "exec error %d %d\n", ret, errno); 
        exit(1);
    }
    else if (ret > 0)
    {
        return ret;
    }
    else
    {
        fprintf(stderr, "fork error %d\n", ret); 
        exit(1);
    }
    return 0;
}

void wait_app(PID_TYPE pid)
{
    int status = 0;
    if (pid != 0)
    {
        waitpid(pid, &status, 0);
    }
}
#endif
