#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>

typedef struct {
  char* filename; /* this file is the pipe (set by user) */
  char is_server; /* this is set by open_control_file */
  int fd; /* this is set by open_control_file */
} single_instance_struct;

/* returns fd, is_server is set to -1 if server, 0 if client */
int open_control_file(single_instance_struct* str)
{
  struct stat buf;

  if(stat(str->filename,&buf)) {
    mkfifo(str->filename,128|256);
    str->is_server=-1;
    str->fd=open(str->filename,O_NONBLOCK|O_RDONLY);
  } else {
    str->fd=open(str->filename,O_NONBLOCK|O_WRONLY);
    if(errno==ENXIO) {
      str->is_server=-1;
      str->fd=open(str->filename,O_NONBLOCK|O_RDONLY);
    } else
      str->is_server=0;
  }

  return(str->fd);
}

void delete_control_file(single_instance_struct* str)
{
  remove(str->filename);
}

void close_control_file(single_instance_struct* str)
{
  close(str->fd);
}

typedef void (*event_dispatcher)(char* message);

int get_next_message(char* buffer,int len,single_instance_struct* str,int usecs)
{
  struct timeval tv;
  fd_set fdset;
  int num_fds;

  FD_ZERO(&fdset);
  FD_SET(str->fd,&fdset);
  tv.tv_sec=0;
  tv.tv_usec=usecs;

  num_fds=select(str->fd+1,&fdset,NULL,NULL,&tv);
  if(num_fds) {
    int reallen;

    reallen=read(str->fd,buffer,len);
    if(reallen==0) {
      close(str->fd);
      str->fd=open(str->filename,O_NONBLOCK|O_RDONLY);
      num_fds--;
    }
    buffer[reallen]=0;
#ifdef DEBUG_1INSTANCE
    if(reallen!=0) rfbLog("message received: %s.\n",buffer);
#endif
  }

  return(num_fds);
}

int dispatch_event(single_instance_struct* str,event_dispatcher dispatcher,int usecs)
{
  char buffer[1024];
  int num_fds;

  if((num_fds=get_next_message(buffer,1024,str,usecs)) && buffer[0])
    dispatcher(buffer);

  return(num_fds);
}

int loop_if_server(single_instance_struct* str,event_dispatcher dispatcher)
{
  open_control_file(str);
  if(str->is_server) {
    while(1)
      dispatch_event(str,dispatcher,50);
  }

  return(str->fd);
}

void send_message(single_instance_struct* str,char* message)
{
#ifdef DEBUG_1INSTANCE
  int i=
#endif
  write(str->fd,message,strlen(message));
#ifdef DEBUG_1INSTANCE
  rfbLog("send: %s => %d(%d)\n",message,i,strlen(message));
#endif
}

#ifdef DEBUG_MAIN

#include <stdio.h>
#include <stdlib.h>

single_instance_struct str1 = { "/tmp/1instance" };

void my_dispatcher(char* message)
{
#ifdef DEBUG_1INSTANCE
  rfbLog("Message arrived: %s.\n",message);
#endif
  if(!strcmp(message,"quit")) {
    delete_control_file(str1);
    exit(0);
  }
}

int main(int argc,char** argv)
{
  int i;

  loop_if_server(str1,my_dispatcher);

  for(i=1;i<argc;i++)
    send_event(str1,argv[i]);

  return(0);
}

#endif
