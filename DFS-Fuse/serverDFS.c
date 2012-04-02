#define FUSE_USE_VERSION 26
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

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/select.h>
#include <fuse.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <pthread.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <sys/time.h>
#include <dirent.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include<semaphore.h>

#ifdef HAVE_SETXATTR
#include <sys/xattr.h>
#endif

#define MAXCLIENTS 100
#define MAXPATHLENGTH 100
#define SIZEOF_DIRECTORY_POTATO (sizeof(int)*2 +sizeof(mode_t) + sizeof(ino_t) + sizeof(char)*256)
#define SIZEOF_AUTHENTICATION_POTATO (sizeof(char)*100)
#define SIZEOF_REQUEST_POTATO (sizeof(int)*6 + sizeof(mode_t)+ sizeof(char)*MAXPATHLENGTH+sizeof(size_t)+sizeof(off_t)+sizeof(dev_t)+sizeof(char)*256*2)
#define SIZEOF_REPLY_POTATO (sizeof(int)*3 + sizeof(unsigned long)+sizeof(dev_t)+sizeof(ino_t) + sizeof(mode_t) + sizeof(nlink_t) + sizeof(uid_t) + sizeof(gid_t) + sizeof(dev_t) + sizeof(off_t) + sizeof(blksize_t)+sizeof(blkcnt_t)+sizeof(time_t)*3+sizeof(unsigned long))

FILE *statefp;

struct prefetch_file_status{
char path[250];
int client_ssd;
int update;
struct prefetch_file_status *next;
};

struct authentication_potato
{
  char username[50];
  char password[50];
};

struct file_status_list
{
   char filename[500];
   int flag;
   int read_count,write_count;
   sem_t semaphore;
   struct file_status_list * next;
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
    int flags; //Used by open()
    int fd; //used by close()
    mode_t mode; //Used by open()
    char fpath[MAXPATHLENGTH]; //open() read() creat, getattr();
    int state;
    int count; 
    int cached_t;
    dev_t req_rdev_t;
   //for caching
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

struct reply_param
{
    struct stat Stat;
    struct fuse_file_info Fuse_file_info;

};

struct accept_thread_param
{
    int sd;
    struct sockaddr_in *clientaddr;
    int *ssd, *cnt;
};

struct service_thread_param
{
    int ssd;
    int client_id;
};


struct xmp_dirp {
	DIR *dp;
	struct dirent *entry;
	off_t offset;
};


struct prefetch_file_status * prefetch_head;


/// New functions for prefetching 

struct prefetch_file_status * find_prefetching_file(char *path,int client_ssd){
struct prefetch_file_status * temp;

	if(prefetch_head==NULL)
		return NULL;
	else if(prefetch_head!=NULL){
		if((strcmp(prefetch_head->path,path)==0)&&(prefetch_head->client_ssd==client_ssd)){
			return prefetch_head;
		}else{
		temp=prefetch_head->next;
			while(temp!=NULL){
					if((strcmp(temp->path,path)==0)&&(temp->client_ssd==client_ssd)){
						return temp;
					}else{
						temp=temp->next;
					}

			}
		}	
	}
	return NULL;
}

int add_prefecthing_file(char *path,int client_ssd){

struct prefetch_file_status * file_check;
struct prefetch_file_status * temp;
temp=prefetch_head;
file_check=find_prefetching_file(path,client_ssd);

	if(file_check!=NULL){
		update_prefecthing_file(path,client_ssd,0);
	}else if(file_check==NULL){
		//add a new node
		if(temp==NULL){
		//add first node
		prefetch_head=malloc(sizeof (struct prefetch_file_status)); 
		//prefetch_head->path=malloc(strlen(path)+1);
		strcpy(prefetch_head->path,path);
		prefetch_head->client_ssd=client_ssd;
		update_prefecthing_file(path,client_ssd,0);
		return 0;
		}else{
		//add a succesive position
			while(temp->next!=NULL){
			temp=temp->next;
			}
		temp->next=malloc(sizeof (struct prefetch_file_status));
		//temp->next=malloc(strlen(path)+1);
		strcpy(temp->next->path,path);
		temp->next->client_ssd=client_ssd;
		update_prefecthing_file(path,client_ssd,0);
		return 0;
		}

	}else{
		perror("It shldn;t come here");
		return -1;
	}

return 0;
}

int update_prefecthing_file(char *path,int client_ssd,int update){

struct prefetch_file_status * file_check;
struct prefetch_file_status * temp;
temp=prefetch_head;
file_check=find_prefetching_file(path,client_ssd);
	if(file_check!=NULL){
		file_check->update=update;
		return 0;
	}else if(file_check==NULL){
		perror("should come here also in file_check=NULL in update");
		//add a new node
		if(temp==NULL){
		//add first node
		prefetch_head=malloc(sizeof (struct prefetch_file_status)); 
		//prefetch_head->path=malloc(strlen(path)+1);
		strcpy(prefetch_head->path,path);
		prefetch_head->client_ssd=client_ssd;
		prefetch_head->update=update;
		}else{
		//add a succesive position
			while(temp->next!=NULL){
			temp=temp->next;
			}
		temp->next=malloc(sizeof (struct prefetch_file_status));
		//temp->next=malloc(strlen(path)+1);
		strcpy(temp->next->path,path);
		temp->next->client_ssd=client_ssd;
		temp->next->update=update;
		return -1;
		}
	}else{
		perror("It shldn;t come here");
		return -1;
	}

return 0;
}

int get_status_prefecthing_file(char *path,int client_ssd){
struct prefetch_file_status * file_check;
struct prefetch_file_status * temp;
temp=prefetch_head;
file_check=find_prefetching_file(path,client_ssd);
	if(file_check!=NULL){
		return file_check->update;
	}else if(file_check==NULL){
		perror("should come here also in file_check=NULL in get_status");
		//add a new node
		if(temp==NULL){
		//add first node
		prefetch_head=malloc(sizeof (struct prefetch_file_status)); 
		//prefetch_head->path=malloc(strlen(path)+1);
		strcpy(prefetch_head->path,path);
		prefetch_head->client_ssd=client_ssd;
		update_prefecthing_file(path,client_ssd,0);
		return prefetch_head->update;
		}else{
		//add a succesive position
			while(temp->next!=NULL){
			temp=temp->next;
			}
		temp->next=malloc(sizeof (struct prefetch_file_status));
		//temp->next=malloc(strlen(path)+1);
		strcpy(temp->next->path,path);
		temp->next->client_ssd=client_ssd;
		update_prefecthing_file(path,client_ssd,0);
		return temp->next->update;
		}
	}else{
		perror("It shldn;t come here");
		return -1;
	}


return 0;

}




/// End of New functions for prefetching

void send_reply_potato(struct reply_potato vreply_potato,int sd)
{
    void *ptr_rep = (void*)&vreply_potato;
    int sendttl,lens;
    for(sendttl=0;sendttl<SIZEOF_REPLY_POTATO;sendttl+=lens)
    {
        lens=send(sd,(ptr_rep+sendttl),SIZEOF_REPLY_POTATO,0);
        if(lens<0)
        {
          perror("Send:");
          exit(1);
        }
        printf("\nSend returned %d\n",lens);
    }
}


void service_thread(void *paramptr)
{
    FILE *fpasswords;
    int lenr,lens,recvttl=0,sendttl=0;
    char username[50], password[50];
    memset(username,NULL,50);
    memset(password,NULL,50);
    struct service_thread_param *p = (struct service_thread_param *)paramptr;
    printf("Connection Accepted from client : %d\n",p->client_id);
    struct authentication_potato vauthentication_potato;
    struct request_potato vrequest_potato;
    struct reply_potato vreply_potato;
    void *ptr_aut=(void*)&vauthentication_potato;
    void *ptr_req=(void*)&vrequest_potato;
    void *ptr_rep = (void *)&vreply_potato;
    memset(ptr_aut,NULL,sizeof(struct authentication_potato));
    char path[MAXPATHLENGTH];
    memset(path,NULL,MAXPATHLENGTH);
    strcpy(path,"/tmp/client");
    //User authentication
printf("\nSizeof dir potato: %d ,Sizeof authentication potato: %d , Sizeof reply potato %d, Sizeof request potato %d \n",SIZEOF_DIRECTORY_POTATO,SIZEOF_AUTHENTICATION_POTATO,SIZEOF_REPLY_POTATO,SIZEOF_REQUEST_POTATO );
    lenr=1;
    for(recvttl=0;recvttl< SIZEOF_AUTHENTICATION_POTATO && lenr!=0;recvttl+=lenr)
    {
        lenr=recv(p->ssd,(ptr_aut+recvttl),SIZEOF_AUTHENTICATION_POTATO-recvttl,0);
        if(lenr<0)
        {
          perror("Recv");
          exit(1);
        }
        printf("Recv returned: %d\n",lenr);
    }
    fpasswords = fopen ("passwords","r");
    if(fpasswords == 0)
    {
        printf("Password file problem\n");
        exit(0);
    }
    while(fscanf(fpasswords,"%s %s",&username,&password)>0)
          {
            if(strcmp(username, vauthentication_potato.username)==0)
                if(strcmp(password, vauthentication_potato.password)==0)
                {
                    printf("Authenticated!\n");
                    //exit(0);
                    break;
                }

            memset(username,NULL,50);
            memset(password,NULL,50);
          }


 while(1)
 {
   memset(ptr_rep,0,sizeof(struct reply_potato));
   memset(ptr_req,0,sizeof(struct request_potato));

   lenr=1;
    for(recvttl=0;recvttl< SIZEOF_REQUEST_POTATO && lenr!=0;recvttl+=lenr)
    {
        lenr=recv(p->ssd,(ptr_req+recvttl),SIZEOF_REQUEST_POTATO-recvttl,0);
        if(lenr<0)
        {
          perror("Recv");
          exit(1);
        }
        printf("Recv returned: %d\n",lenr);
    }
    printf("\nOpcode Received=%d\n",vrequest_potato.Opcode);
    memset(path,NULL,MAXPATHLENGTH);
    strcpy(path,"/tmp/client/");

    strcat(path,vauthentication_potato.username);
    if(strcmp(vrequest_potato.fpath,"")!=0)
    {
        strcat(path,vrequest_potato.fpath);
    }

    if(vrequest_potato.Opcode==0)
    {
        printf("Open implemenattion here.\n");
        int res;

		vreply_potato.rc = open(path, vrequest_potato.flags);
	if (vreply_potato.rc == -1)
		vreply_potato.errno_t=-errno;

	close(vreply_potato.rc);

       for(sendttl=0;sendttl<SIZEOF_REPLY_POTATO;sendttl+=lens)
        {
            lens=send(p->ssd,(ptr_rep+sendttl),SIZEOF_REPLY_POTATO,0);
            if(lens<0)
            {
              perror("Send:");
              exit(1);
            }
            printf("\nSend returned %d\n",lens);
        }
	//return 0;
    }
    if(vrequest_potato.Opcode==1)
    {
        printf("Close implementation here.\n");
    }
    if(vrequest_potato.Opcode==2)
    {
        printf("Read implemtation here.\n");
      /*
        int fd;
		int res;
		char *buf;

		fd = open(path, vrequest_potato.mode);
		vreply_potato.rc=fd;

		if (fd == -1)
			vreply_potato.errno_t= -errno;

		if(vreply_potato.rc!=-1){
		buf=malloc(sizeof(char)*vrequest_potato.buffer_size);
		vreply_potato.rc = pread(fd, buf, vrequest_potato.buffer_size, vrequest_potato.buffer_offset);
		

		if (vreply_potato.rc == -1)
			vreply_potato.errno_t = -errno;
		
			close(fd);
		
		//return res;
		}

        for(sendttl=0;sendttl<SIZEOF_REPLY_POTATO;sendttl+=lens)
        {
            lens=send(p->ssd,(ptr_rep+sendttl),SIZEOF_REPLY_POTATO,0);
            if(lens<0)
            {
              perror("Send:");
              exit(1);
            }
            printf("Send returned: %d",sendttl);
        }
        if(vreply_potato.rc!=-1)
        {

	        for(sendttl=0;sendttl<(sizeof(char)*vreply_potato.rc);sendttl+=lens)
	        {
	            lens=send(p->ssd,(buf+sendttl),(sizeof(char)*vreply_potato.rc),0);
	            if(lens<0)
	            {
	              perror("Send:");
	              exit(1);
	            }
	            printf("Send returned: %d",sendttl);
	        }	
        }
	//closing after  reading
	close(fd);


    //send_reply_potato(vreply_potato,p->ssd);
	*/
	int fd;
	int res;
	struct stat file_s;
	
	//open file
	fd=open(path,O_RDONLY);
	vreply_potato.rc=fd;
	if(fd==-1){
			vreply_potato.errno_t=-errno;

			//send potato
			for(sendttl=0;sendttl<SIZEOF_REPLY_POTATO;sendttl+=lens)
			{
			    lens=send(p->ssd,(ptr_rep+sendttl),SIZEOF_REPLY_POTATO,0);
			    if(lens<0)
			    {
			      perror("Send:");
			      exit(1);
			    }
			    printf("Send returned: %d",sendttl);
			}

	}

	if(vrequest_potato.cached_t==0){
			//set update to zero by def
			vreply_potato.update=0;	

			//check for file
			fstat(fd,&file_s);

			//set file size 
			vreply_potato.st_size_t=file_s.st_size;
	
				//send potato
				for(sendttl=0;sendttl<SIZEOF_REPLY_POTATO;sendttl+=lens)
				{
				    lens=send(p->ssd,(ptr_rep+sendttl),SIZEOF_REPLY_POTATO,0);
				    if(lens<0)
				    {
				      perror("Send:");
				      exit(1);
				    }
				    printf("Send returned: %d",sendttl);
				}

			//send file
			char *buffer_t;
			int read_count;
			buffer_t=malloc(sizeof(char)*vreply_potato.st_size_t);
			read_count=pread(fd,buffer_t,vreply_potato.st_size_t,0);
			if(read_count==-1){
			perror("Read failed at server");			
			}else if(read_count==vreply_potato.st_size_t){
			printf("\n Read successfull \n");
			}else{
			printf("\n read Incomplete");
			}
			for(sendttl=0;sendttl<vreply_potato.st_size_t;sendttl+=lens)
			{
				    lens=send(p->ssd,(buffer_t+sendttl),vreply_potato.st_size_t,0);
				    if(lens<0)
				    {
				      perror("Send:");
				      exit(1);
				    }
				    printf("Send returned: %d",sendttl);
			}
			
			

			//close file
			close(fd);

			//add_prefething_file(path,p->ssd);
			add_prefecthing_file(path,p->ssd);
	
		
	

	}else if(vrequest_potato.cached_t==1){
		int up_status;
		up_status=get_status_prefecthing_file(path,p->ssd);
	
		if(up_status==0){
			//set update =0
			vreply_potato.update=0;
			//send potato
			for(sendttl=0;sendttl<SIZEOF_REPLY_POTATO;sendttl+=lens)
			{
			    lens=send(p->ssd,(ptr_rep+sendttl),SIZEOF_REPLY_POTATO,0);
			    if(lens<0)
			    {
			      perror("Send:");
			      exit(1);
			    }
			    printf("Send returned: %d",sendttl);
			}
		}else if(up_status==1){
			//set update =1 
			vreply_potato.update=1;
	
			

			//get file size 
			fstat(fd,&file_s);

			//set file size
			vreply_potato.st_size_t=file_s.st_size;

			//send potato
			for(sendttl=0;sendttl<SIZEOF_REPLY_POTATO;sendttl+=lens)
			{
			    lens=send(p->ssd,(ptr_rep+sendttl),SIZEOF_REPLY_POTATO,0);
			    if(lens<0)
			    {
			      perror("Send:");
			      exit(1);
			    }
			    printf("Send returned: %d",sendttl);
			}

			//send file
			char *buffer_t;
			int read_count;
			buffer_t=malloc(sizeof(char)*vreply_potato.st_size_t);
			read_count=pread(fd,buffer_t,vreply_potato.st_size_t,0);
			if(read_count==-1){
			perror("Read failed at server");			
			}else if(read_count==vreply_potato.st_size_t){
			printf("\n Read successfull \n");
			}else{
			printf("\n read Incomplete");
			}
			for(sendttl=0;sendttl<vreply_potato.st_size_t;sendttl+=lens)
			{
				    lens=send(p->ssd,(buffer_t+sendttl),vreply_potato.st_size_t,0);
				    if(lens<0)
				    {
				      perror("Send:");
				      exit(1);
				    }
				    printf("Send returned: %d",sendttl);
			}

			//reset update status
			update_prefecthing_file(path,p->ssd,0);

		}else{
		perror("Shouln't come here, server:read:cached_t =1");	
			for(sendttl=0;sendttl<SIZEOF_REPLY_POTATO;sendttl+=lens)
			{
			    lens=send(p->ssd,(ptr_rep+sendttl),SIZEOF_REPLY_POTATO,0);
			    if(lens<0)
			    {
			      perror("Send:");
			      exit(1);
			    }
			    printf("Send returned: %d",sendttl);
			}

		}	




	}else{
		perror("It should not come here: server:read");	
	}



    }
    if(vrequest_potato.Opcode==3)
    {
        printf("Write implementation here.\n");
	int fd;
		int res;
		char *buffer_t;

	printf("Server size =%d",vrequest_potato.buffer_size);
	printf("\n Path is %s \n",path);

		fd = open(path, O_WRONLY,vrequest_potato.flags);
		vreply_potato.rc=fd;

		//if (fd == -1)
	       //  	vreply_potato.errno_t= -errno;

	
		if (vreply_potato.rc == -1){
			vreply_potato.errno_t = -errno;
			close(fd);		
		}

        for(sendttl=0;sendttl<SIZEOF_REPLY_POTATO;sendttl+=lens)
        {
            lens=send(p->ssd,(ptr_rep+sendttl),SIZEOF_REPLY_POTATO,0);
            if(lens<0)
            {
              perror("Send:");
              exit(1);
            }
            printf("Send returned: %d",lens);
        }
	        


	if(vreply_potato.rc!=-1)
        {
		buffer_t=malloc(sizeof(char)*vrequest_potato.buffer_size);
		int recvttl,lenr;
		for(recvttl=0,lenr=1;recvttl< vrequest_potato.buffer_size && lenr!=0;recvttl+=lenr)
	    	{
			lenr=recv(p->ssd,(buffer_t+recvttl),vrequest_potato.buffer_size-recvttl,0);
			if(lenr<0)
			{
	       		   perror("Recv");
	       		   exit(1);
			}
			printf("\nRecv returned: %d\nBuffer in write is %s\n",lenr,buffer_t);
	    	}
		//strcpy(buf,buffer_t);
		vreply_potato.rc = pwrite(fd, buffer_t, vrequest_potato.buffer_size, vrequest_potato.buffer_offset);
		printf("\nNo of bytes wrriten %d, to write %d\n",vreply_potato.rc,vrequest_potato.buffer_size);
	}



	
	//closing after writing
	close(fd);
	struct prefetch_file_status * check;
	struct prefetch_file_status * temp;
	temp=prefetch_head;
	
//	check=find_prefetching_file(path,p->ssd);
	if(temp!=NULL){
		while(temp!=NULL){
		if(strcmp(temp->path,path)==0)
		update_prefecthing_file(temp->path,temp->client_ssd,1);
		temp=temp->next;
		}
//		update_prefecthing_file(path,p->ssd,1);
	}else{
	// NO need to update
	}

	
    }
    if(vrequest_potato.Opcode==4)
    {
          struct stat stbuf;
          printf("getattr implementation here.\n");
          printf("pathh=%s",path);
          int rclstat=lstat(path,&stbuf);
          printf("lstat returned %d\n",rclstat);
          vreply_potato.st_dev_t=stbuf.st_dev;
          vreply_potato.st_ino_t=stbuf.st_ino;
          vreply_potato.st_mode_t=stbuf.st_mode;
          vreply_potato.st_nlink_t=stbuf.st_nlink;
          vreply_potato.st_uid_t=stbuf.st_uid;
          vreply_potato.st_gid_t=stbuf.st_gid;
          vreply_potato.st_rdev_t=stbuf.st_rdev;
          vreply_potato.st_size_t=stbuf.st_size;
          vreply_potato.st_blocks_t=stbuf.st_blocks;
          vreply_potato.st_atime_t=stbuf.st_atime;
          vreply_potato.st_mtime_t=stbuf.st_mtime;
          vreply_potato.st_ctime_t=stbuf.st_ctime;
          vreply_potato.rc=rclstat;
          if (rclstat == -1)
		vreply_potato.errno_t=-errno;

	
       for(sendttl=0;sendttl<SIZEOF_REPLY_POTATO;sendttl+=lens)
        {
            lens=send(p->ssd,(ptr_rep+sendttl),SIZEOF_REPLY_POTATO,0);
            if(lens<0)
            {
              perror("Send:");
              exit(1);
            }
            printf("\nSend returned %d\n",lens);
        }

//    send_reply_potato(vreply_potato,p->ssd);
    }
    if(vrequest_potato.Opcode==5)
    {
        printf("lookup implementation here.\n");
    }
    if(vrequest_potato.Opcode==6)
    {
    	int rc,errno_t;
        printf("mkdir implementation here.\n");
        rc=mkdir(path,vrequest_potato.mode);

	        if (rc == -1)
	        {
				errno_t=-errno;
			}
		vreply_potato.rc=rc;
		vreply_potato.errno_t=errno_t;
		for(sendttl=0;sendttl<SIZEOF_REPLY_POTATO;sendttl+=lens)
        {
            lens=send(p->ssd,(ptr_rep+sendttl),SIZEOF_REPLY_POTATO,0);
            if(lens<0)
            {
              perror("Send:");
              exit(1);
            }
            printf("\nSend returned %d\n",lens);
        }

		//return rc;



    }
    if(vrequest_potato.Opcode==7)
    {
        printf("Access implementation here.\n");
        int rca = access(path,vrequest_potato.mode);
        printf("Access returned :%d\n",rca);
        vreply_potato.rc = rca;
        for(sendttl=0;sendttl<SIZEOF_REPLY_POTATO;sendttl+=lens)
        {
            lens=send(p->ssd,(ptr_rep+sendttl),SIZEOF_REPLY_POTATO,0);
            if(lens<0)
            {
              perror("Send:");
              exit(1);
            }
            printf("\nSend returned %d\n",lens);
        }

    }
    if(vrequest_potato.Opcode==8)
    {
        printf("readdir implementation here.\n");
        if(strcmp(vrequest_potato.fpath,"/")==0){
        //path[strlen(path)-1]='\0';
         memset(path,NULL,MAXPATHLENGTH);
        strcpy(path,"/tmp/client/");

        strcat(path,vauthentication_potato.username);
        }

        DIR *dp;
        struct dirent *de;

        struct directory_potato vdirectory_potato;
        void *ptr_dir=(void*)&vdirectory_potato;
        memset(ptr_dir,0,sizeof(struct directory_potato));
        //What to do
        vreply_potato.rc = 0;
        for(sendttl=0;sendttl<SIZEOF_REPLY_POTATO;sendttl+=lens)
        {
            lens=send(p->ssd,(ptr_rep+sendttl),SIZEOF_REPLY_POTATO,0);
            if(lens<0)
            {
              perror("Send:");
              exit(1);
            }
            printf("\nSend returned %d\n",lens);
        }

        dp = opendir(path);
        if (dp == NULL)
        {
            vdirectory_potato.errno_t = -errno;
            vdirectory_potato.end=1;
            for(sendttl=0;sendttl<SIZEOF_DIRECTORY_POTATO;sendttl+=lens)
            {
                lens=send(p->ssd,(ptr_dir+sendttl),SIZEOF_DIRECTORY_POTATO,0);
                if(lens<0)
                {
                  perror("Send:");
                  exit(1);

                }
                printf("\nSend returned %d\n",lens);
            }
        }
        else
        {
            while ((de = readdir(dp)) != NULL)
            {
                memset(ptr_dir,0,sizeof(struct directory_potato));
                vdirectory_potato.st_ino_t = de->d_ino;
                vdirectory_potato.st_mode_t = de->d_type << 12;
                strcpy(vdirectory_potato.d_name_t,de->d_name);
                for(sendttl=0;sendttl<SIZEOF_DIRECTORY_POTATO;sendttl+=lens)
                {
                    lens=send(p->ssd,(ptr_dir+sendttl),SIZEOF_DIRECTORY_POTATO,0);
                    if(lens<0)
                    {
                      perror("Send:");
                      exit(1);
                    }
                    printf("\nSend returned %d\n",lens);
                }

            }
                vdirectory_potato.errno_t = -errno;
                vdirectory_potato.end=1;
                for(sendttl=0;sendttl<SIZEOF_DIRECTORY_POTATO;sendttl+=lens)
                {
                    lens=send(p->ssd,(ptr_dir+sendttl),SIZEOF_DIRECTORY_POTATO,0);
                    if(lens<0)
                    {
                      perror("Send:");
                      exit(1);
                    }
                    printf("\nSend returned %d\n",lens);
                }
        }




    printf("Filename recieved: %s \nand converted to %s \n",(vrequest_potato.fpath),path);
    printf("Flags: %d\tfd: %d\tstate: %d\tcount: %d\n\n",vrequest_potato.flags, vrequest_potato.fd, vrequest_potato.state, vrequest_potato.count);
	}
	if(vrequest_potato.Opcode==9)
    	{
    	//DIR *dp;
        printf("Opendir implementation here.\n");
        struct xmp_dirp *d = malloc(sizeof(struct xmp_dirp));

        if(strcmp(vrequest_potato.fpath,"/")==0)
        {
	        memset(path,NULL,MAXPATHLENGTH);
		    strcpy(path,"/tmp/client/");
	        strcat(path,vauthentication_potato.username);
        }
    
        int rca =0;
		d->dp = opendir(path);
		if (d->dp == NULL) {
			//res = -errno;
			//free(d);
			rca=-errno;
		}else	
		{
		//vreply_potato.offset = 0;
		}
		d->offset = 0;
		d->entry = NULL;
		vreply_potato.d=(unsigned long)d;
	    vreply_potato.rc = rca;

        for(sendttl=0;sendttl<SIZEOF_REPLY_POTATO;sendttl+=lens)
        {
            lens=send(p->ssd,(ptr_rep+sendttl),SIZEOF_REPLY_POTATO,0);
            if(lens<0)
            {
              perror("Send:");
              exit(1);
            }
            printf("\nSend returned %d\n",lens);
        }

   	}
	if(vrequest_potato.Opcode==12){
	  
		printf("Create implementation here.\n");
		int fd;
		fd=open(path,vrequest_potato.flags,vrequest_potato.mode);
		vreply_potato.rc=fd;
		if(fd==-1){
		vreply_potato.errno_t= -errno;
		}
		for(sendttl=0;sendttl<SIZEOF_REPLY_POTATO;sendttl+=lens)
		{
		    lens=send(p->ssd,(ptr_rep+sendttl),SIZEOF_REPLY_POTATO,0);
		    if(lens<0)
		    {
		      perror("Send:");
		      exit(1);
		    }
		    printf("\nSend returned %d\n",lens);
		}
		close(fd);

	}if(vrequest_potato.Opcode==13){
	  
		printf("Mknod implementation here.\n");
		int fd;
		if (S_ISREG(vrequest_potato.mode)) {
		fd = open(path, O_CREAT | O_EXCL | O_WRONLY, vrequest_potato.mode);
		vreply_potato.rc=fd;
		if (fd >= 0)
			fd = close(fd);
		} else if (S_ISFIFO(vrequest_potato.mode))
			fd = mkfifo(path, vrequest_potato.mode);
		else
			fd = mknod(path, vrequest_potato.mode, vrequest_potato.req_rdev_t);
		if (fd == -1)
			vreply_potato.errno_t= -errno;

		for(sendttl=0;sendttl<SIZEOF_REPLY_POTATO;sendttl+=lens)
		{
		    lens=send(p->ssd,(ptr_rep+sendttl),SIZEOF_REPLY_POTATO,0);
		    if(lens<0)
		    {
		      perror("Send:");
		      exit(1);
		    }
		    printf("\nSend returned %d\n",lens);
		}
		//close(fd);

	}if(vrequest_potato.Opcode==14){
	printf("rm dir implementation");
		int res;

		res = rmdir(path);
		vreply_potato.rc=res;
		if (res == -1)
			vreply_potato.errno_t= -errno;

		
		for(sendttl=0;sendttl<SIZEOF_REPLY_POTATO;sendttl+=lens)
		{
		    lens=send(p->ssd,(ptr_rep+sendttl),SIZEOF_REPLY_POTATO,0);
		    if(lens<0)
		    {
		      perror("Send:");
		      exit(1);
		    }
		    printf("\nSend returned %d\n",lens);
		}

	}
	if(vrequest_potato.Opcode==15)
    	{
	int fd;
	printf("Unlink implementation here.\n");
	fd = unlink(path);
	vreply_potato.rc=fd;
	if (fd == -1)
		vreply_potato.errno_t= -errno;
	for(sendttl=0;sendttl<SIZEOF_REPLY_POTATO;sendttl+=lens)
		{
		    lens=send(p->ssd,(ptr_rep+sendttl),SIZEOF_REPLY_POTATO,0);
		    if(lens<0)
		    {
		      perror("Send:");
		      exit(1);
		    }
		    printf("\nSend returned %d\n",lens);
		}
	

	}if(vrequest_potato.Opcode==17){

		int res;
	char *new_from = malloc(sizeof(char)*512);
	char *new_to = malloc(sizeof(char)*512);
	memset(new_from,NULL,512);
	memset(new_to,NULL,512);
	strcpy(new_from,"/tmp/client/");
	strcpy(new_to,"/tmp/client/");

    strcat(new_from,vauthentication_potato.username);
    strcat(new_to,vauthentication_potato.username);
    if(strcmp(vrequest_potato.rename_from,"")!=0)
    {
        strcat(new_from,vrequest_potato.rename_from);
    }
    if(strcmp(vrequest_potato.rename_to,"")!=0)
    {
        strcat(new_to,vrequest_potato.rename_to);
    }
	printf("\n changes from %s to %s \n",new_from,new_to);
		res = rename(new_from, new_to);	
		vreply_potato.rc=res;
		if (res == -1)
			vreply_potato.errno_t= -errno;

		for(sendttl=0;sendttl<SIZEOF_REPLY_POTATO;sendttl+=lens)
		{
		    lens=send(p->ssd,(ptr_rep+sendttl),SIZEOF_REPLY_POTATO,0);
		    if(lens<0)
		    {
		      perror("Send:");
		      exit(1);
		    }
		    printf("\nSend returned %d\n",lens);
		}
		

	
	}

  
 }
}

void accept_thread(void *paramptr)
{
    printf("Entered\n");
    struct accept_thread_param *p = (struct accept_thread_param *)paramptr;
    socklen_t size;
    size=sizeof(struct sockaddr_in);
    p->ssd[*(p->cnt)]=accept(p->sd, (struct sockaddr *)&(p->clientaddr[*(p->cnt)]), &size);
    if(p->ssd[*(p->cnt)]<0)
    {
      perror("Accept:");
      exit(p->ssd[*(p->cnt)]);
    }
    *(p->cnt)++;
    printf("Connection Accepted!\n");
    exit(0);
}

int main(int argc, char *argv[])
{
    char hostofserver[50];
//printf("%d",SIZEOF_REPLY_POTATO);
    int sd,port,constatefp, ssd[MAXCLIENTS],cnt=0,rcb,rcl,lenr,lens,i,plcnt,ringset=0,big=0,rcs,first,sendttl,rcvttl,hopcnt,x=-1,size;
    struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
    struct fuse_chan *ch;
    struct service_thread_param vservice_thread_parameters;
    struct hostent *hentptr,*plhentptr;
    struct sockaddr_in serveraddr,clientaddr[MAXCLIENTS];
    prefetch_head=NULL;
    pthread_t tid[MAXCLIENTS];
    pthread_attr_t attr;
    if ( argc != 1 )
    {
      fprintf(stderr, "Usage: %s\n", argv[0]);
      exit(1);
    }

    printf("Hello World\n");
    constatefp=open("con_state",O_RDWR);
    if(constatefp==-1)
    constatefp=open("con_state",O_CREAT);

    gethostname(hostofserver,50);
    hentptr = gethostbyname(hostofserver);
    port=20100;
    serveraddr.sin_family=AF_INET;
    serveraddr.sin_port = htons(port);
    memcpy(&serveraddr.sin_addr, hentptr->h_addr_list[0], hentptr->h_length);

    //Create socket sd to connect with right neighbour
    sd=socket(AF_INET,SOCK_STREAM,0);
    if(sd<0)
    {
      perror("Socket:");
      exit(sd);
    }

    x=-1;
    if (setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &x,sizeof(int)) == -1)
    {
      perror("setsockopt");
      exit(1);
    }

    rcb=bind(sd,(struct sockaddr *)&serveraddr,sizeof(serveraddr));
    if(rcb<0)
    {
      perror("Bind:");
      exit(rcb);
    }

    rcl=listen(sd,5);
    if(rcl<0)
    {
      perror("Listen:");
      exit(rcl);
    }

    while(1)
    {
    size=sizeof(struct sockaddr_in);
    ssd[cnt]=accept(sd, (struct sockaddr *)&clientaddr[cnt], &size);
    vservice_thread_parameters.ssd=ssd[cnt];
    vservice_thread_parameters.client_id=cnt;
    pthread_create(&tid[cnt], NULL, &service_thread, (void *)&vservice_thread_parameters);
    cnt++;
    }

}



