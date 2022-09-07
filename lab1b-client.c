/*NAME: Cao Xu
EMAIL: cxcharlie@gmail.com
ID: 704551688*/

#include <unistd.h>
#include <getopt.h>
#include <termios.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <ulimit.h>
#include <zlib.h>
#include <errno.h>
#include <poll.h>

struct termios ori;
z_stream in_strm;
z_stream out_strm;
bool compressed;
int sockfd;

//confirm non-echo, behavior

void original()
{
  if(compressed)
    {
      deflateEnd(&out_strm);
      inflateEnd(&in_strm);
    }
  if (sockfd != 0)
    {
      close(sockfd);
    }
  if (tcsetattr(STDIN_FILENO, TCSANOW, &ori)<0)
    {
      fprintf(stderr, "Error occured with restoring termios: %s", strerror(errno));
    }
}

void terminal_setup()
{
  struct termios tmp;
  if (tcgetattr(STDIN_FILENO, &tmp)<0)//returns negative if failed
    {
      fprintf(stderr, "Error Occurred with tcgetattr: %s", strerror(errno));
      exit(1);
    }
  if (tcgetattr(STDIN_FILENO, &ori)<0)//returns negative if failed
    {
      fprintf(stderr, "Error Occurred with tcgetattr: %s", strerror(errno));
      exit(1);
    }
  atexit(original);

  tmp.c_iflag = ISTRIP;
  tmp.c_oflag = 0;
  tmp.c_lflag = 0;

  if (tcsetattr(STDIN_FILENO, TCSANOW, &tmp) <0)
    {
      fprintf(stderr, "Error occurred with tcsetattr: %s", strerror(errno));
      exit(1);
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

int main(int argc, char **argv)
{

  int port;
  sockfd = 0;
struct option long_options[] = {
				{"port", required_argument, 0, 'p'},
				{"log", required_argument, 0, 'l'},
				{"compress", no_argument, 0, 'c'},
				{0,0,0,0}
};


 bool has_port = false;
 bool log = false;
 int logfile;
 compressed = false;
 int i;
 while( (i = getopt_long(argc, argv, "", long_options, NULL)) != -1) {
   switch (i)
     {
     case 'p':
       has_port = true;
       port = atoi(optarg);
       break;
     case 'l':
       ulimit(UL_SETFSIZE, 10000);
       log = true;
       
       if ((logfile = creat(optarg,0666)) < 0)
	 {
	   fprintf(stderr, "Error occurred while opening/creating file: %s", strerror(errno));
	   exit(1);
	 }
       break;
     case 'c':
       compressed = true;
       compress_time();
       break;
     default:
       fprintf(stderr, "Incorrect Usage: --port=PORT, --log=FILE, --compress are the available options");
       exit(1);
     }
 }
 if (!has_port)
   {
     fprintf(stderr, "ERROR: --port=PORT option is mandatory");
     exit(1);
   }

 terminal_setup();
 ///////////////////////CREATE THE SOCKET AND CONNECT////////////////////////
 
 struct sockaddr_in serv_addr;
 struct hostent *server;

 if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
   {
     fprintf(stderr, "Error occured with socket creation: %s", strerror(errno));
     exit(1);
   }

 server = gethostbyname("localhost");
 
 memset(&serv_addr, 0, sizeof(struct sockaddr_in));
 serv_addr.sin_family = AF_INET;
 memcpy(&serv_addr.sin_addr.s_addr, server->h_addr, server->h_length);
 serv_addr.sin_port = htons(port);

 if (connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) <0)
   {
     fprintf(stderr, "Error occurred while connecting %s", strerror(errno));
     exit(1);
   }
 
 ///////////////////////////////////////////////////////////////////////

 struct pollfd poller[2];
 poller[0].fd = STDIN_FILENO;
 poller[0].events = POLLIN + POLLHUP + POLLERR;
 poller[1].fd = sockfd;
 poller[1].events = POLLIN + POLLHUP + POLLERR;
 //dont forget special characters ^C ^D lr/cr
 while(1){
   if (poll(poller, 2, -1) < 0)
     {
       fprintf(stderr, "Error occured whiling polling: %s", strerror(errno));
       exit(1);
     }
   if (poller[0].revents & POLLIN)
     {
       char buffer[256];
       char comp_buf[256];
       int num = read(STDIN_FILENO, buffer, sizeof(buffer));
       if (num<0)
	 {
	   fprintf(stderr, "Error occurred while reading: %s", strerror(errno));
	   exit(1);
	 }
       else if (num == 0)
	 break;
       int i = 0;
       for (i = 0; i < num; i++)
	 {

	   char x = buffer[i];
	   if ( x == '\r' || x == '\n')
	     {
	       if(write(STDOUT_FILENO, "\r\n", 2) < 0)
		 {
		   fprintf(stderr, "Error occurred while writing: %s", strerror(errno));
		   exit(1);
		 }
	     }
	   else if( (write(STDOUT_FILENO, &x, sizeof(char))) < 0)
	     {
	       fprintf(stderr, "Error occured while writing: %s", strerror(errno));
	       exit(1);
	     }//check again
	   if(!compressed && (write(sockfd, &x, sizeof(char))) < 0)
	     {
	       fprintf(stderr, "Error occured while writing: %s", strerror(errno));
	       exit(1);
	     }
	 }

       if (compressed)
	 {
	   out_strm.avail_in = num;
	   out_strm.next_in = (unsigned char *)buffer;
	   out_strm.avail_out = 256;
	   out_strm.next_out = (unsigned char *)comp_buf;

	   do{
	     if (deflate(&out_strm, Z_SYNC_FLUSH) == Z_STREAM_ERROR)
	       {
		 fprintf(stderr, "Error occured while deflating");
		 exit(1);
	       }
	   }while(out_strm.avail_in > 0);

	   for (int j = 0; j < 256 - (int)(out_strm.avail_out); j++)
	     {
	       if (write(sockfd, &comp_buf[j], sizeof(char)) < 0)
		 {
		   fprintf(stderr, "Error occurred while writing: %s", strerror(errno));
		   exit(1);
		 }
	     }
	 }
       if (log)
	 {
	   if (write(logfile, "SENT ", 5) < 0)
	     {
	       fprintf(stderr, "Error occured while writing: %s", strerror(errno));
	       exit(1);
	     }
	   char bytenum[3];//compressed option
	   if(compressed)
	     sprintf(bytenum, "%d", 256-((int)(out_strm.avail_out)));
	   else
	     sprintf(bytenum, "%d", num);
	   if (write(logfile, bytenum, strlen(bytenum)) < 0)
             {
               fprintf(stderr, "Error occured while writing: %s", strerror(errno));
               exit(1);
             }
     	   if (write(logfile, " bytes: ", 8) < 0)
	     {
	       fprintf(stderr, "Error occured while writing: %s", strerror(errno));
	       exit(1);
	     }
	   if (!compressed)
	     {
	       if (write(logfile, buffer, num) < 0)
		 {
		   fprintf(stderr, "Error occurred while writing: %s", strerror(errno));
		   exit(1);
		 }
	     }
	   else
	     {
	       if (write(logfile, comp_buf, 256-out_strm.avail_out) < 0)
		 {
                   fprintf(stderr, "Error occurred while writing: %s", strerror(errno));
                   exit(1);
                 }
	     }
	   if (write(logfile, "\n", 1) < 0)
	     {
	       fprintf(stderr, "Error occurred while writing: %s", strerror(errno));
	       exit(1);
	     }
	 }
	       
     }
   if (poller[1].revents & POLLIN)
     {
       char buffer[512];
       char comp_buf[512];
       //       char logbuf[512]; //potentially double if all 256 characters are /r /n
       int num = read(sockfd, buffer, sizeof(buffer));
       if (num<0)
         {
           fprintf(stderr, "Error occurred while reading: %s", strerror(errno));
           exit(1);
         }
       else if (num == 0)
	 {
	   exit(0);
	 }

       if(compressed)
	 {
	   in_strm.avail_in = num;
	   in_strm.next_in = (unsigned char *) buffer;
	   in_strm.avail_out = 512;
	   in_strm.next_out = (unsigned char *) comp_buf;

	   do {
	   if (inflate(&in_strm, Z_SYNC_FLUSH) == Z_STREAM_ERROR)
               {
                 fprintf(stderr, "Error occured while inflating");
                 exit(1);
               }
           }while(in_strm.avail_in > 0);
	   int k = (int)(512-in_strm.avail_out);
	   for (int j = 0; j < k; j++)
	     {
	       
	       if (write(STDOUT_FILENO, &comp_buf[j], sizeof(char)) < 0)
		 {
		   fprintf(stderr, "Error occurred while printing decompressed data: %s", strerror(errno));
		   exit(1);
		 }
	     }
	}
       else
	 {
	   int i = 0;
	   for (i = 0; i < num; i++)
	     {
	       char x = buffer[i];
	       if(write(STDOUT_FILENO, &x, sizeof(char)) < 0)
		 {
		   fprintf(stderr, "Error occured while writing: %s", strerror(errno));
		   exit(1);
		 }
	     }
	 }
       if (log)//check for compression
         {
           if (write(logfile, "RECEIVED ", 9) < 0)
             {
               fprintf(stderr, "Error occured while writing: %s", strerror(errno));
               exit(1);
             }
           char bytenum[3];
           sprintf(bytenum, "%d", num);
	   if(write(logfile, bytenum, strlen(bytenum)) < 0)
	     {
	       fprintf(stderr, "Error occurred while writing: %s", strerror(errno));
	       exit(1);
	     }
	   if (write(logfile, " bytes: ", 8) < 0)
             {
               fprintf(stderr, "Error occured while writing: %s", strerror(errno));
               exit(1);
             }
           if (write(logfile, buffer, num) < 0)
             {
               fprintf(stderr, "Error occurred while writing: %s", strerror(errno));
               exit(1);
             }
           if (write(logfile, "\n", 1) < 0)
             {
               fprintf(stderr, "Error occurred while writing: %s", strerror(errno));
               exit(1);
             }
         }
       
       if (poller[0].revents & POLLHUP || poller[0].revents & POLLERR)
         {
	   // exit(1);
	   break;
         }//piazza

       if (poller[1].revents & POLLHUP || poller[1].revents & POLLERR)
	 {
	   exit(0);
	 }
       //////////////////////POLLHUP AND POLLERR????
       /////////////////IF ERROR READ ALL THESTUFF OUT FIRSt???

     }

 }
 
 return 0;
}

