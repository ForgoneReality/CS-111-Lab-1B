/*NAME: CAO XU
EMAIL: cxcharlie@gmail.com
ID: 704551688*/

#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <stdbool.h>
#include <fcntl.h>
#include <sys/wait.h>
#include "zlib.h"
#include <errno.h>
#include <ctype.h>
#include <poll.h>

int sock_fd;
int listenfd;
pid_t ret;
int to_shell_pipe[2];
int from_shell_pipe[2];

z_stream in_strm;
z_stream out_strm;

bool compressed;

void comp_exitor()
{
  inflateEnd(&in_strm);
  deflateEnd(&out_strm);
}

void socketcloser()
{
    if(close(sock_fd)<0)
    {
      fprintf(stderr,"Error closings sock_fd on exit: %s", strerror(errno));
      exit(1);
    }
    if(close(listenfd)<0)
    {
      fprintf(stderr,"Error closing listenfd on exit: %s", strerror(errno));
      exit(1);
    }
}

void exitor()
{
  int status;
  if (waitpid(ret, &status, 0)< 0)
    {
      fprintf(stderr, "Error occurred with waitpid in parent");
      exit(1);
    }
  int a1 = status & 0x00ff;
  int a2 = (status & 0xff00) >> 8;
  fprintf(stderr, "SHELL EXIT SIGNAL=%d STATUS=%d",a1 , a2);
}

void handler(int sig)
{
  
  if (sig == SIGPIPE)
    {
      atexit(exitor);
      exit(0);
    }

}

void compress_time()
{
    out_strm.zalloc = Z_NULL;
    out_strm.zfree = Z_NULL;
    out_strm.opaque = Z_NULL;
    if(deflateInit(&out_strm, Z_DEFAULT_COMPRESSION) != Z_OK)
      {
        fprintf(stderr, "Error occurred with deflateInit(): %s", strerror(errno));
        exit(1);
      }

    in_strm.zalloc = Z_NULL;
    in_strm.zfree = Z_NULL;
    in_strm.opaque = Z_NULL;
    in_strm.avail_in = 0;//
    in_strm.next_in = Z_NULL;//
    if(inflateInit(&in_strm) != Z_OK)
      {
        fprintf(stderr, "Error occurred with inflateInit(): %s", strerror(errno));
        exit(1);
      }
}


int connect_server(unsigned int port_num)
{
  int retfd = 0;
  struct sockaddr_in serv_addr, cli_addr;
  unsigned int cli_len = sizeof(struct sockaddr_in);
  
  if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
      fprintf(stderr, "Error occured with socket creation: %s", strerror(errno));
      exit(1);
    }
     
  memset(&serv_addr, 0, sizeof(struct sockaddr_in));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = INADDR_ANY;
  serv_addr.sin_port = htons(port_num);

  if (bind(listenfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
    {
      fprintf(stderr, "Erorr occurred with binding socket: %s", strerror(errno));
      exit(1);
    }
  if (listen(listenfd, 5) < 0)
    {
      fprintf(stderr, "Error occurred while listening %s", strerror(errno));
      exit(1);
    }

  if ((retfd = accept(listenfd, (struct sockaddr *)&cli_addr, &cli_len)) < 0 )
    {
      fprintf(stderr, "Error occured while accepting: %s", strerror(errno));
      exit(1);
    }

  atexit(socketcloser);
  return retfd;
}

int main(int argc, char** argv){
  int port;
  bool has_port = false;
  struct option long_options[] = {
				   {"port", required_argument, NULL, 'p'},
				   {"shell", required_argument, NULL, 's'},
				   {"compress", no_argument, NULL, 'c'},
				   {0,0,0,0}
  };

  char* shellname="/bin/bash";
  compressed = false;
  int i;
  while( (i = getopt_long(argc, argv, "", long_options, NULL)) != -1) {
    switch (i)
      {
      case 'p':
	has_port = true;
	port = atoi(optarg);
	break;
      case 's':
	shellname=optarg;
	break;
      case 'c':
	atexit(comp_exitor);
	compressed = true;
	compress_time();
	break;
      default:
	fprintf(stderr, "Incorrect Usage: --port=PORT, --shell=SHELL, --compress are the available options");
	exit(1);
      }
  }

  if(!has_port)
    {
      fprintf(stderr, "ERROR: --port=PORT option is mandatory");
      exit(1);
    }
  
  sock_fd = connect_server(port);

  if (pipe(to_shell_pipe) < 0 || pipe(from_shell_pipe) < 0)
    {
      fprintf(stderr, "Error occured with pipe initialization: %s", strerror(errno));
      exit(1);
    }

  signal(SIGPIPE, handler);

  ret = fork();

  if (ret == 0)//child process
    {
      if (close(to_shell_pipe[1])<0)
	{
	  fprintf(stderr, "Error occurred with closing to_shell_pipe[1]: %s", strerror(errno));
	  exit(1);
	}

      if (close(from_shell_pipe[0])<0)
	{
	  fprintf(stderr, "Error occurred with closing from_shell_pipe[0]: %s", strerror(errno));
	  exit(1);
	}
      if (close(STDIN_FILENO)<0)
	{
	  fprintf(stderr, "Error occurred with closing STDIN_FILENO: %s", strerror(errno));
	  exit(1);
	}
      if (dup(to_shell_pipe[0]) < 0)
	{
	  fprintf(stderr, "Error occurred with duping to_shell_pipe[0]: %s", strerror(errno));
	  exit(1);
	}
      if (close(to_shell_pipe[0])< 0)
	{
	  fprintf(stderr, "Erorr occurred with closing to_shell_pipe[0]: %s", strerror(errno));
	  exit(1);
	}
      if (close(STDOUT_FILENO)< 0)
	{
	  fprintf(stderr, "Erorr occurred with closing STDOUT_FILENO: %s", strerror(errno));
	  exit(1);
	}
      if (dup(from_shell_pipe[1]) < 0)
	{
	  fprintf(stderr, "Error occurred with duping from_shell_pipe[1]: %s", strerror(errno));
	  exit(1);
	}
      close(STDERR_FILENO);
      if (dup(from_shell_pipe[1]) < 0)
	{
	  fprintf(stderr, "Error occurred with duping from_shell_pipe[1]: %s", strerror(errno));
	  exit(1);
	}
      if (close(from_shell_pipe[1]) < 0)
	{
	  fprintf(stderr, "Error occurred with closing from_shell_pipe[1]): %s", strerror(errno));
	  exit(1);
	}

      if (execlp(shellname, shellname, NULL) < 0)
	{
	  fprintf(stderr, "Error executing shell: %s", strerror(errno));
	  exit(1);
	} 
    }
  else if (ret > 0) //parent process
    {
      bool closed = false;
      struct pollfd poller[2];
      poller[0].fd = sock_fd;
      poller[0].events = POLLIN + POLLHUP + POLLERR;
      poller[1].fd = from_shell_pipe[0];
      poller[1].events = POLLIN + POLLHUP + POLLERR;

      if (close(to_shell_pipe[0])<0)
	{
	  fprintf(stderr, "Error occurred with closing to_shell_pipe[0]: %s", strerror(errno));
	  exit(1);
	}

      if (close(from_shell_pipe[1]) < 0)
	{
	  fprintf(stderr, "Error occurred with closing from_shell_pipe[1]: %s", strerror(errno));
	  exit(1);
	}
      
      while(1)
	{
	  if (poll(poller, 2, -1) < 0)
	    {
	      fprintf(stderr, "Error occured whiling polling: %s", strerror(errno));
	      exit(1);
	    }
	  if (poller[0].revents & POLLIN && !closed) {
	    char buffer[256];
	    char comp_buf[256];
	    int num = read(sock_fd, buffer, sizeof(buffer));
	    if (num<0)
	      {
		fprintf(stderr, "Error occurred while reading: %s", strerror(errno));
		exit(1);
	      }
	    if (num == 0)
	      {
		closed = true;
		break;
	      }
	    if (compressed)
	      {/*
		for (int i = 0; i<num; i++)
		  {
		    printf("%c", buffer[i]);
		    }*/
		
		in_strm.avail_in = num;
		in_strm.next_in = (unsigned char *) buffer;
		in_strm.avail_out = 256;
		in_strm.next_out = (unsigned char *) comp_buf;

		do {
		  if (inflate(&in_strm, Z_SYNC_FLUSH) == Z_STREAM_ERROR)
		    {
		      fprintf(stderr, "Error occurred in decompressing");
		      exit(1);
		    }
		}while(in_strm.avail_in > 0);
	      }
	    int count;

	    if (compressed)
	      count = (int)(256 - in_strm.avail_out);  //no overflow to worry about
	    else
	      count = num;
	    int i = 0;
	    for (i = 0; i < count; i++)
	      {
		char x;
		if (!compressed)
		  x = buffer[i];
		else
		  x = comp_buf[i];
		if (x == 0x03)
		  {
		    //DO WE SEND THESE OUT TOO???
		    if (kill(ret, SIGINT) == -1)
		      {
			fprintf(stderr, "Error occured while killing child: %s", strerror(errno));
			exit(1);
		      }
		  }
		else if (x == 0x04)
		  {//here too
		    if(close(to_shell_pipe[1]) < 0)
		      {
			fprintf(stderr, "Error while closing pipe: %s", strerror(errno));
			exit(1);
		      }
		    closed = true;
		    break;
		  }
		else if (x == '\r' || x == '\n')
		  {
		    if (write(to_shell_pipe[1], "\n", 1)< 0)
		      {
			fprintf(stderr, "Error while writing to pipe: %s", strerror(errno));
			exit(1);
		      }
		  }
		else
		  {
		    if (write(to_shell_pipe[1], &x, 1) <0)
		      {
			fprintf(stderr, "Error occured while writing to stdout: %s", strerror(errno));
			exit(1);
		      }
		  }
	      }
	    
	  }
	  
	  if (poller[1].revents & POLLIN)
	    {
	      char buffer[256];
	      char comp_buf[512]; //potentially double b/c \n becomes \r\n
	      int num = read(from_shell_pipe[0], buffer, sizeof(buffer));
	      if (num<0)
		{
		  fprintf(stderr, "Error occured while reading: %s", strerror(errno));
		  exit(1);
		}
	      else if (num == 0)
		{
		  atexit(exitor);
		  exit(0);
		}
	      
	      int j = 0;
	      int i = 0;
	      for (i = 0; i < num; i++)
		{
		  //handling special characters? confirm
		  char x = buffer[i];
		  
		  if (x == '\n')
		    {
		      if (!compressed)
			{
			  if (write(sock_fd, "\r\n", 2) < 0)
			    {
			      fprintf(stderr, "Error while writing: %s", strerror(errno));
			      exit(1);
			    }
			}
		      else //compressed
			{
			  comp_buf[j] = '\r';
			  j++;
			  comp_buf[j] = '\n';
			  j++;
			}
		    }
		  else if (x == 0x04)//YOOOOOOOOOOOOOOOOOOOOO MAYBE NOT IMMEDIATELY?
		    {//if above, note different implemetnation of compressed/not compressed. 
		      atexit(exitor);
		      exit(0);
		    }
		  
		  else
		    {
		      if (!compressed)
			{
			  if ((write(sock_fd, &x, 1)) < 0)
			    {
			      fprintf(stderr, "Error occurred while writing to sock_fd: %s", strerror(errno));
			      exit(1);
			    }
			}
		      else
			{
			  comp_buf[j] = x;
			  j++;
			}
		    }
	      
		}
	      if (compressed)
		{
		  char temp[512];
		  out_strm.avail_in = j;
		  out_strm.next_in = (unsigned char *) comp_buf;
		  out_strm.avail_out = 512;
		  out_strm.next_out = (unsigned char *) temp;

		  do {
		    if (deflate(&out_strm, Z_SYNC_FLUSH) == Z_STREAM_ERROR)
		      {
			fprintf(stderr, "Error occurred in compressing");
			exit(1);
		      }
		  }while(out_strm.avail_in > 0);
		  /*		  for (int i = 0; i < 512-(int)(out_strm.avail_out); i++)
		    {
		      if (write(sock_fd, &temp[i], sizeof(char)) < 0)
			{
			  fprintf(stderr, "Error occured while writing: %s", strerror(errno));
			  exit(1);
			}

			}*/
		  if (write(sock_fd, temp, 512-(int)out_strm.avail_out) < 0)
		    {
		      fprintf(stderr, "Error occured while writing: %s", strerror(errno));
		      exit(1);
		    }

		  
		}
	    }
	
	  if(poller[0].revents & (POLLHUP | POLLERR))
	    {
	      if(!closed)
		{
		  if (close(to_shell_pipe[1])<0)
		    {
		      fprintf(stderr, "Error occurred while closing pipe: %s", strerror(errno));
		      exit(1);
		    }
		  closed = true;
		}
	    }
	  if(poller[1].revents & (POLLHUP | POLLERR))
	    {
	      atexit(exitor);
	      exit(1);
	    }
	  
	}
    }
  else//dun goofed
    {
      fprintf(stderr, "Error occurred while forking: %s", strerror(errno));
      exit(1);
    }

 return 0;
}
