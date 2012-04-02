/*
  FUSE: Fileloostem in Userspace
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>

  This program can be distributed under the terms of the GNU GPL.
  See the file COPYING.

  gcc -Wall `pkg-config fuse --cflags --libs` fusexmp.c -o fusexmp
*/

#define FUSE_USE_VERSION 26

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef linux
/* For pread()/pwrite() */
#define _XOPEN_SOURCE 500
#endif

/*For struct stat*/
#include <sys/stat.h>

/*For sockets*/
#include <sys/select.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <sys/time.h>
#ifdef HAVE_SETXATTR
#include <sys/xattr.h>
#endif


struct hostent* server_info;
int server_port,server_conn,server_sock;
struct sockaddr_in server;
struct dfs_ds{
char path[1000] ;
};
#define MAXPATHLENGTH 100
#define SIZEOF_DIRECTORY_POTATO (sizeof(int)*2 +sizeof(mode_t) + sizeof(ino_t) + sizeof(char)*256)
#define SIZEOF_AUTHENTICATION_POTATO (sizeof(char)*100)
#define SIZEOF_REQUEST_POTATO (sizeof(int)*6 + sizeof(mode_t)+ sizeof(char)*MAXPATHLENGTH+sizeof(size_t)+sizeof(off_t)+sizeof(dev_t)+ sizeof(char)*256*2)
#define SIZEOF_REPLY_POTATO (sizeof(int)*3 + sizeof(unsigned long)+sizeof(dev_t)+sizeof(ino_t) + sizeof(mode_t) + sizeof(nlink_t) + sizeof(uid_t) + sizeof(gid_t) + sizeof(dev_t) + sizeof(off_t) + sizeof(blksize_t)+sizeof(blkcnt_t)+sizeof(time_t)*3+sizeof(unsigned long))

struct authentication_potato
{
  char username[50];
  char password[50];
};

struct directory_potato
{
    int end;
    int errno_t;
    mode_t st_mode_t;
    ino_t  st_ino_t;
    char d_name_t[256];
};

struct request_potato
{
    int Opcode;
    int flags;
    int fd;
    mode_t mode;
    char fpath[MAXPATHLENGTH];
    int state;
    int count;
    int cached_t;
    dev_t req_rdev_t;
    size_t buffer_size;
    off_t buffer_offset;
    char rename_from[256];
    char rename_to[256];
};

struct reply_potato
{
   int rc,errno_t;
   int update;
   unsigned long filesize;

   dev_t     st_dev_t;     /* ID of device containing file */
   ino_t     st_ino_t;     /* inode number */
   mode_t    st_mode_t;    /* protection */
   nlink_t   st_nlink_t;   /* number of hard links */
   uid_t     st_uid_t;     /* user ID of owner */
   gid_t     st_gid_t;     /* group ID of owner */
   dev_t     st_rdev_t;    /* device ID (if special file) */
   off_t     st_size_t;    /* total size, in bytes */
   blksize_t st_blksize_t; /* blocksize for file system I/O */
   blkcnt_t  st_blocks_t;  /* number of 512B blocks allocated */
   time_t    st_atime_t;   /* time of last access */
   time_t    st_mtime_t;   /* time of last modification */
   time_t    st_ctime_t;   /* time of last status change */
   unsigned long d;

};


static char* strip_filename(const char*path){
char *path_token,*old_token,*temp_path;
temp_path=malloc((sizeof(char)*strlen(path))+2);
strcpy(temp_path,path);
path_token=strtok(temp_path,"/");
old_token=(char *)malloc(sizeof(char)*strlen(temp_path));

if(path_token!=NULL)
	strcpy(old_token,path_token);
else
	old_token=NULL;

while(path_token!=NULL)
{
	path_token=strtok(NULL,"/");

	if(path_token==NULL)
		break;
	else
	{
		free(old_token);
		old_token=(char *)malloc(sizeof(char)*strlen(temp_path));
		strcpy(old_token,path_token);
	}
}
printf("\nOld%s path%s",old_token,path_token,temp_path);
return old_token;

}

struct reply_potato pass_msg(struct request_potato vrequest_potato)
{
    int sendttl,lens;
    int recvttl,lenr;
    //struct request_potato vrequest_potato;

     /*int send_len;
     //char buf[20]="hello";
     struct dfs_ds send_msg;
     strcpy(send_msg.path,message);
     unsigned char buf[sizeof(send_msg)];
          memcpy(&buf,&send_msg,sizeof(send_msg));
     //send_len=send(server_sock,buf,sizeof(send_msg),0);
     send_len=send(server_sock,&send_msg,sizeof(send_msg),0);
     printf("\n%d Send length\n",send_len);
     if(send_len<0){
      perror("connect:");
      exit(0);
     }*/
      //Send request_potato
    void *ptr_req = (void*)&vrequest_potato;
    struct reply_potato vreply_potato;
    void *ptr_rep = (void*)&vreply_potato;

    for(sendttl=0;sendttl<SIZEOF_REQUEST_POTATO;sendttl+=lens)
    {
        lens=send(server_sock,(ptr_req+sendttl),SIZEOF_REQUEST_POTATO,0);
        if(lens<0)
        {
          perror("Send:");
          exit(1);
        }
	printf("Recv returned: %d\n",lenr);
        printf("Filename sent: %s\n",vrequest_potato.fpath);

    }

    for(recvttl=0,lenr=1;recvttl< SIZEOF_REPLY_POTATO && lenr!=0;recvttl+=lenr)
    {
        lenr=recv(server_sock,(ptr_rep+recvttl),SIZEOF_REPLY_POTATO-recvttl,0);
        if(lenr<0)
        {
          perror("Recv");
          exit(1);
        }
        printf("Recv returned: %d\n",lenr);
    }

    return vreply_potato;
}


void copy_stat(struct stat *stbuf_dest,struct reply_potato stbuf_source){
stbuf_dest->st_dev=stbuf_source.st_dev_t;
    stbuf_dest->st_ino=stbuf_source.st_ino_t;
    stbuf_dest->st_mode=stbuf_source.st_mode_t;
    stbuf_dest->st_nlink=stbuf_source.st_nlink_t;
    stbuf_dest->st_uid=stbuf_source.st_uid_t;
    stbuf_dest->st_gid=stbuf_source.st_gid_t;
    stbuf_dest->st_rdev=stbuf_source.st_rdev_t;
    stbuf_dest->st_size=stbuf_source.st_size_t;
    stbuf_dest->st_blocks=stbuf_source.st_blocks_t;
    stbuf_dest->st_atime=stbuf_source.st_atime_t;
    stbuf_dest->st_mtime=stbuf_source.st_mtime_t;
    stbuf_dest->st_ctime=stbuf_source.st_ctime_t;
}

static int xmp_getattr(const char *path, struct stat *stbuf)
{
int res;
//pass_msg("getattr");
struct request_potato vrequest_potato;
struct reply_potato vreply_potato;
vrequest_potato.Opcode=4;
strcpy(vrequest_potato.fpath,path);
vreply_potato.rc=0;
vreply_potato=pass_msg(vrequest_potato);

printf("\npath=%s\n",path);

//populate
  copy_stat(stbuf,vreply_potato);

res=vreply_potato.rc;
printf("\nattr_value=%d\n",res);
//res = lstat(path, stbuf);

if (vreply_potato.rc == -1)
return vreply_potato.errno_t;

return 0;
}

static int xmp_access(const char *path, int mask)
{
int res;
//pass_msg("access");
struct request_potato vrequest_potato;
struct reply_potato vreply_potato;
vrequest_potato.Opcode=7;
strcpy(vrequest_potato.fpath,path);
vrequest_potato.mode=mask;
vreply_potato=pass_msg(vrequest_potato);

res=vreply_potato.rc;
printf("\naccess_value=%d\n",res);
//res = access(path, mask);

if (res == -1)
return -errno;

return 0;
}

static int xmp_readlink(const char *path, char *buf, size_t size)
{
int res;
//pass_msg("readlink");
struct request_potato vrequest_potato;
struct reply_potato vreply_potato;
vrequest_potato.Opcode=10;
strcpy(vrequest_potato.fpath,path);
pass_msg(vrequest_potato);

res = readlink(path, buf, size - 1);

if (res == -1)
return -errno;

buf[res] = '\0';
return 0;
}

static int xmp_opendir(const char *path, struct fuse_file_info *fi)
{
	int res;
	/*struct request_potato vrequest_potato;
	struct reply_potato vreply_potato;
	vrequest_potato.Opcode=9;
	strcpy(vrequest_potato.fpath,path);
	//pass_msg(vrequest_potato);

	
	vreply_potato=pass_msg(vrequest_potato);
	//struct xmp_dirp dp;
	//struct xmp_dirp *d = malloc(sizeof(struct xmp_dirp));
	//if (d == NULL)
	//	return -ENOMEM;

	//d->dp = opendir(path);
	//if (d->dp == NULL) {
	//	res = -errno;
	//	free(d);
	//	return res;
	//}
	//d->offset = 0;
	//d->entry = NULL;
	//dp=vreply_potato.dp;
	//d->dp=vreply_potato.dp;
	//d->offset=0;
	//d->entry=NULL;

	res=vreply_potato.rc;

	fi->fh = (unsigned long) vreply_potato.d;
	return res;*/
	return 0;
}

static int xmp_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
      off_t offset, struct fuse_file_info *fi)
{
DIR *dp;
struct dirent *de;
struct stat st;
struct request_potato vrequest_potato;
struct reply_potato vreply_potato;
struct directory_potato vdirectory_potato;
void *ptr_dir =  (void*)&vdirectory_potato;
vrequest_potato.Opcode=8;
strcpy(vrequest_potato.fpath,path);
vreply_potato=pass_msg(vrequest_potato);

(void) offset;
(void) fi;

do
{
    int recvttl,lenr;
    for(recvttl=0,lenr=1;recvttl< SIZEOF_DIRECTORY_POTATO && lenr!=0;recvttl+=lenr)
    {
        lenr=recv(server_sock,(ptr_dir+recvttl),SIZEOF_DIRECTORY_POTATO-recvttl,0);
        if(lenr<0)
        {
          perror("Recv");
          exit(1);
        }
        printf("\nRecv returned: %d\n",lenr);
    }
    if(vdirectory_potato.end!=1){
    memset(&st, 0, sizeof(st));
    st.st_ino = vdirectory_potato.st_ino_t;
    st.st_mode = vdirectory_potato.st_mode_t << 12;
    printf("\nReceived file name:%s\n",vdirectory_potato.d_name_t);
    if (filler(buf, vdirectory_potato.d_name_t, &st, 0))
    break;
	}
}
while(vdirectory_potato.end!=1);

/*dp = opendir(path);
if (dp == NULL)
return -errno;

while ((de = readdir(dp)) != NULL) {
struct stat st;
memset(&st, 0, sizeof(st));
st.st_ino = de->d_ino;
st.st_mode = de->d_type << 12;


closedir(dp);*/
return 0;
}


static int xmp_mkdir(const char *path, mode_t mode)
{

int res;
struct request_potato vrequest_potato;
struct reply_potato vreply_potato;
vrequest_potato.Opcode=6;
strcpy(vrequest_potato.fpath,path);
vrequest_potato.mode=mode;
vreply_potato=pass_msg(vrequest_potato);

res=vreply_potato.rc;

//res = mkdir(path, mode);
if (res == -1)
return vreply_potato.errno_t;

return 0;
}

static int xmp_unlink(const char *path)
{
int res;
struct request_potato vrequest_potato;
struct reply_potato vreply_potato;
vrequest_potato.Opcode=15;
strcpy(vrequest_potato.fpath,path);
vreply_potato=pass_msg(vrequest_potato);
res=vreply_potato.rc;
//res = unlink(path);
if (res == -1)
return vreply_potato.errno_t;

return 0;
}

static int xmp_rmdir(const char *path)
{
int res;
struct request_potato vrequest_potato;
struct reply_potato vreply_potato;
vrequest_potato.Opcode=14;
strcpy(vrequest_potato.fpath,path);
vreply_potato=pass_msg(vrequest_potato);

res=vreply_potato.rc;
// res = rmdir(path);
if (res == -1)
return -errno;

return 0;


}



static int xmp_open(const char *path, struct fuse_file_info *fi)
{
int res;
struct request_potato vrequest_potato;
struct reply_potato vreply_potato;
vrequest_potato.Opcode=0;
vrequest_potato.mode=fi->flags;
strcpy(vrequest_potato.fpath,path);
vreply_potato=pass_msg(vrequest_potato);

//res = open(path, fi->flags);
if (vreply_potato.rc == -1)
return vreply_potato.errno_t;

//close(res);
return 0;
}

static int xmp_read(const char *path, char *buf, size_t size, off_t offset,
   struct fuse_file_info *fi)
{
int fd;
int res;
char *buffer_t;
char *temp_file_name;
char *new_file;
(void) fi;
struct request_potato vrequest_potato;
struct reply_potato vreply_potato;
vrequest_potato.Opcode=2;
strcpy(vrequest_potato.fpath,path);
vrequest_potato.buffer_size=size;
vrequest_potato.buffer_offset=offset;
vrequest_potato.mode=fi->flags;

//check with tmp

int temp_filefd;
new_file=malloc(strlen(path)+2);
new_file=strip_filename(path);
temp_file_name=malloc((sizeof(char)*strlen(new_file))+5);
strcpy(temp_file_name,"/tmp/");
strcat(temp_file_name,new_file);
printf("temp file name = %s",temp_file_name);


temp_filefd=open(temp_file_name,O_RDONLY,0666);
//check file in tmp
if(temp_filefd==-1){
printf("\nFile not found in local");
vrequest_potato.cached_t=0;
}else{
printf("\nFile found in local");
vrequest_potato.cached_t=1;
close(temp_filefd);
}

vreply_potato=pass_msg(vrequest_potato);
if(vrequest_potato.cached_t==1){

	if(vreply_potato.update==1){
	//Receive file 
	char *buffer_t;
	printf("Bytes to be received = %d",vreply_potato.st_size_t);
	buffer_t=malloc(sizeof(char)*vreply_potato.st_size_t);
	int recvttl,lenr;
	for(recvttl=0,lenr=1;recvttl< vreply_potato.st_size_t && lenr!=0;recvttl+=lenr)
	    {
		lenr=recv(server_sock,(buffer_t+recvttl),vreply_potato.st_size_t-recvttl,0);
		if(lenr<0)
		{
		  perror("Recv");
		  exit(1);
		}
		printf("Recv returned: %d\n",lenr);
	    }

	//Put it in local 
	int local_fd;
	if((local_fd=open(temp_file_name,O_CREAT|O_RDWR|O_TRUNC,0666))<0){
	perror("\nLocal File cannot be created");
	}else{
	printf("\nfile created in local\n");
	printf("\nreceived buffer %s\n",buffer_t);
	write(local_fd,buffer_t,vreply_potato.st_size_t);
	close(local_fd);
	printf("\nfile written in local\n");
	}
	//Read file and close
	fd=open(temp_file_name,fi->flags);
		if (fd == -1)
			return -errno;
		res = pread(fd, buf, size, offset);
		if (res == -1)
			return -errno;
		close(fd);


	}else if(vreply_potato.update==0){
	//Reads from local
		fd=open(temp_file_name,fi->flags);
		if (fd == -1)
			return -errno;
		res = pread(fd, buf, size, offset);
		if (res == -1)
			return -errno;
		close(fd);
	}else{
		perror("read failed");
	}

}else if(vrequest_potato.cached_t==0){
	//Receive file 
	char *buffer_t;
	printf("Bytes to be received = %d",vreply_potato.st_size_t);
	buffer_t=malloc(sizeof(char)*vreply_potato.st_size_t);
	int recvttl,lenr;
	for(recvttl=0,lenr=1;recvttl< vreply_potato.st_size_t && lenr!=0;recvttl+=lenr)
	    {
		lenr=recv(server_sock,(buffer_t+recvttl),vreply_potato.st_size_t-recvttl,0);
		if(lenr<0)
		{
		  perror("Recv");
		  exit(1);
		}
		printf("Recv returned: %d\n",lenr);
	    }

	//Put it in local 
	int local_fd;
	if((local_fd=open(temp_file_name,O_CREAT|O_RDWR|O_TRUNC,0666))<0){
	perror("\nLocal File cannot be created");
	}else{
	printf("\nfile created in local\n");
	printf("\nreceived buffer %s\n",buffer_t);
	write(local_fd,buffer_t,vreply_potato.st_size_t);
	close(local_fd);
	printf("\nfile written in local\n");
	}
	//Read file and close
	char * read_buffer;
	read_buffer=malloc(sizeof(char)*size);
	fd=open(temp_file_name,O_RDONLY);
		if (fd == -1)
			return -errno;
		res = pread(fd, read_buffer, size, offset);
		if (res == -1)
			return -errno;
		strcpy(buf,read_buffer);
		close(fd);
}else{
perror("Should not come here");
}


return vreply_potato.rc;

/*

vreply_potato=pass_msg(vrequest_potato);




if(vreply_potato.rc==-1)
return vreply_potato.errno_t;

buffer_t=malloc(sizeof(char)*vreply_potato.rc);
int recvttl,lenr;
for(recvttl=0,lenr=1;recvttl< vreply_potato.rc && lenr!=0;recvttl+=lenr)
    {
        lenr=recv(server_sock,(buffer_t+recvttl),vreply_potato.rc-recvttl,0);
        if(lenr<0)
        {
          perror("Recv");
          exit(1);
        }
        printf("Recv returned: %d\n",lenr);
    }
strcpy(buf,buffer_t);

return vreply_potato.rc;
*/

}

static int xmp_write(const char *path, const char *buf, size_t size,
    off_t offset, struct fuse_file_info *fi)
{

int fd;
int res;
char *buffer_t;
(void) fi;
struct request_potato vrequest_potato;
struct reply_potato vreply_potato;
vrequest_potato.Opcode=3;
strcpy(vrequest_potato.fpath,path);
vrequest_potato.buffer_size=size;
vrequest_potato.buffer_offset=offset;
vrequest_potato.mode=fi->flags;
printf("Client size =%d",vrequest_potato.buffer_size);
vreply_potato=pass_msg(vrequest_potato);

if(vreply_potato.rc==-1)
return vreply_potato.errno_t;


int sendttl,lens;

        for(sendttl=0;sendttl<(sizeof(char)*vrequest_potato.buffer_size);sendttl+=lens)
        {
            lens=send(server_sock,(buf+sendttl),(sizeof(char)*vrequest_potato.buffer_size),0);
            if(lens<0)
            {
              perror("Send:");
              exit(1);
            }
	            printf("Send returned: %d",sendttl);
        }	
//strcpy(buf,buffer_t);
/*
int recvttl,lenr;
    //struct reply_potato vreply_potato;
    void *ptr_rep = (void*)&vreply_potato;

 for(recvttl=0,lenr=1;recvttl< SIZEOF_REPLY_POTATO && lenr!=0;recvttl+=lenr)
    {
        lenr=recv(server_sock,(ptr_rep+recvttl),SIZEOF_REPLY_POTATO-recvttl,0);
        if(lenr<0)
        {
          perror("Recv");
          exit(1);
        }
        printf("Recv returned: %d\n",lenr);
    }
*/

return vrequest_potato.buffer_size;
//return vreply_potato.rc;






}
static int xmp_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
struct request_potato vrequest_potato;
struct reply_potato vreply_potato;
vrequest_potato.Opcode=12;
strcpy(vrequest_potato.fpath,path);
vrequest_potato.flags=fi->flags;
vrequest_potato.mode=mode;
vreply_potato=pass_msg(vrequest_potato);

if(vreply_potato.rc==-1)
return vreply_potato.errno_t;

	return 0;
}
static int xmp_mknod(const char *path, mode_t mode, dev_t rdev)
{
	int res;
	struct request_potato vrequest_potato;
	struct reply_potato vreply_potato;
	vrequest_potato.Opcode=13;
	strcpy(vrequest_potato.fpath,path);
	vrequest_potato.req_rdev_t=rdev;
	vrequest_potato.mode=mode;
	vreply_potato=pass_msg(vrequest_potato);
	/* On Linux this could just be 'mknod(path, mode, rdev)' but this
	   is more portable */
	if(vreply_potato.rc==-1)
	return vreply_potato.errno_t;
	
	return 0;
}
static int xmp_rename(const char *from, const char *to)
{
	int res;

	struct request_potato vrequest_potato;
	struct reply_potato vreply_potato;
	vrequest_potato.Opcode=17;
	//strcpy(vrequest_potato.fpath,path);
	strcpy(vrequest_potato.rename_from,from);
	strcpy(vrequest_potato.rename_to,to);

	vreply_potato=pass_msg(vrequest_potato);
	/* On Linux this could just be 'mknod(path, mode, rdev)' but this
	   is more portable */
	if(vreply_potato.rc==-1)
	return vreply_potato.errno_t;
	
	return 0;
}


static struct fuse_operations xmp_oper = {
.getattr = xmp_getattr,
.access = xmp_access,
.readlink = xmp_readlink,
.opendir = xmp_opendir,
.readdir = xmp_readdir,
.mkdir = xmp_mkdir,
.unlink = xmp_unlink,
.rmdir = xmp_rmdir,
.open = xmp_open,
.read = xmp_read,
.write = xmp_write,
.create = xmp_create,
.mknod = xmp_mknod,
.rename=xmp_rename,
};


void initiate_connection(char * servername)
{
int lenr,lens,sendttl;
struct authentication_potato vauthentication_potato;
void *ptr_aut=&vauthentication_potato;

//Provide server name
server_info= gethostbyname(servername);

if ( server_info == NULL ) {
    fprintf(stderr, "localhost: host not found\n");
    exit(1);

  }
server_port=20100;
// pretend we've connected both to a server at this point
//speak to server using this
server_sock = socket(AF_INET, SOCK_STREAM, 0);
if ( server_sock < 0 ) {
    perror("socket:");
    exit(server_sock);
  }

server.sin_family = AF_INET;
//Provide server port
    server.sin_port = htons(server_port);
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    memcpy(&server.sin_addr, server_info->h_addr_list[0], server_info->h_length);

  //To talk to the server
  /* connect to socket at above addr and port */
  server_conn = connect(server_sock, (struct sockaddr *)&server, sizeof(server));
 if ( server_conn < 0 ) {
   perror("connect:");
   exit(server_conn);
 }
printf("Connection Success");
 //pass_msg("Connection Initiated");

printf("Enter username:\n");
scanf("%s",&vauthentication_potato.username);
printf("Enter password:\n");
scanf("%s",&vauthentication_potato.password);

for(sendttl=0;sendttl<SIZEOF_AUTHENTICATION_POTATO;sendttl+=lens)
    {
        lens=send(server_sock,(ptr_aut+sendttl),SIZEOF_AUTHENTICATION_POTATO,0);
        if(lens<0)
        {
          perror("Send:");
          exit(1);
        }
        printf("\nSend returned %d\n",lens);
    }

}

int main(int argc, char *argv[])
{
char * temp=malloc(strlen(argv[argc-1]+1));
strcpy(temp,argv[argc-1]);
umask(0);
printf("Reply potato: %d\nRequest potato: %d\n",SIZEOF_REPLY_POTATO,SIZEOF_REQUEST_POTATO);
initiate_connection(temp);
return fuse_main(argc-1, argv, &xmp_oper, NULL);
}
