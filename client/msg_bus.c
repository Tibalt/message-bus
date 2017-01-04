#include "msg_define.h"
#include "msg_bus.h"
#include "apue.h"
#include "minIni.h"
#include <errno.h>
#include <arpa/inet.h>
#include <netinet/in.h> 
#include <fcntl.h>
#include <assert.h>
#include "fdfactory.h"
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
//#include <sys/select.h>

#define SIZEARRAY(array) sizeof array/sizeof array[0]
#define GOOUT(value) {retvalue=value;goto out;} 

typedef struct {
    unsigned int magic; 
    unsigned int size;
}packet_head;
typedef struct{
    int fd;
    unsigned char channel_id; 
} channelFD_pair;

typedef struct{
    int fd;
    unsigned int size;
    unsigned char state;
} fd_status;


static struct timeval waittime = {.tv_sec=0,.tv_usec = 1000*1000};

static int comp_fd_status(const void *a, const void *b){
    return ((fd_status *)a)->fd - ((fd_status *)b)->fd;
}

static channelFD_pair cf_pair[MAX_CHAN_SIZE] = {};

static int SwitchBlock(int fd, char flag){
    if(fd < 0)   return -1;

	int flags = fcntl(fd,F_GETFL);
	if( -1 == flags ){
		log_ret(LOG_WARNING,"fcntl,F_GETFL failed");
        return -1;
	}

    if(flag != 0)
	    flags = flags & ~O_NONBLOCK;
    else
        flags = flags | O_NONBLOCK;

	if( -1 == fcntl(fd,F_SETFL,flags)  ){
		log_ret(LOG_WARNING,"fcntl,F_SETFL failed");
        return -1;
	}
    return 0;
}
/*Maintain the fd_lst*/
static fd_status fd_lst[MAX_CHAN_SIZE*MAX_FD_SIZE] = { [ 0 ... MAX_CHAN_SIZE*MAX_FD_SIZE-1] = {-1,0,0}};

static int GetFDIndex(int fd){
    fd_status st; st.fd= fd;
    fd_status *fs =(fd_status *)bsearch(&st,fd_lst,SIZEARRAY(fd_lst),sizeof(fd_status),comp_fd_status);
    if(fs == NULL)
        return -1;
    else
        return fs - fd_lst;
}
static int unregister_channel_idx(int idx){
    if(idx <0 || idx > MAX_CHAN_SIZE) return  -1;
    
    cf_pair[idx].channel_id = 0;
    cf_pair[idx].fd = 0;
    return 0;
}

int get_fd(int ch){
    if(ch== 0) return -1;
    for(int i=0;i<MAX_CHAN_SIZE;i++){
        if(cf_pair[i].channel_id == ch){
            return cf_pair[i].fd; 
        }
    }
    return -1;
}



void set_waittime(int msec){
    waittime.tv_sec = msec/1000;
    waittime.tv_usec = (msec%1000) * 1000;
}


int register_fd(int fd,int op){
    /*
    int i =0;
    int num = SIZEARRAY(fd_lst);
    for(i=0;i<num;i++)
        //printf("%d, ",fd_lst[i].fd);
    //printf("\n");
    */

    if(fd <= 0) return -3;
    
    int idx = GetFDIndex(fd);
    if(idx >=0) return -1;

    idx = GetFDIndex(-1);
    if(idx < 0){
        return -2;
    }
    else{
        SwitchBlock(fd,op & BLOCK);
        fd_lst[idx].fd = fd;
        fd_lst[idx].size = 0;
        fd_lst[idx].state = 0;
        qsort(fd_lst,SIZEARRAY(fd_lst),sizeof(fd_status),comp_fd_status);
        return 0;
    }
}

int unregister_fd(int fd){
    int idx = -1;
    if(fd <= 0)  
        return -3;
    if((idx = GetFDIndex(fd)) >= 0){
        fd_lst[idx].fd = -1;
        fd_lst[idx].size = 0;
        fd_lst[idx].state = 0;
        qsort(fd_lst,SIZEARRAY(fd_lst),sizeof(fd_status),comp_fd_status);
        return 0;
    }
    else
        return -1;
}
int unregister_channel(unsigned char  channel_id){
    if(channel_id == 0) return -1;
    for(int i=0;i<MAX_CHAN_SIZE;i++){
        if(cf_pair[i].channel_id == channel_id){
            close(cf_pair[i].fd);
            //try to unregister fd
            unregister_fd(cf_pair[i].fd); 
            unregister_channel_idx(i);
            //try to free the channel
            return 0;
        }
    }
    return -1;
}
/*Maintain the fd_lst over*/



static int IsRegistered(unsigned char channel_id,int *fd){
    *fd = -1;
    if(channel_id == 0) return -1;
    int i = 0;
    for(i=0;i<MAX_CHAN_SIZE;i++){
        if(cf_pair[i].channel_id == channel_id){
            *fd = cf_pair[i].fd; 
           return 0; 
        }
    }
    return -2;
}

static int read_fd(int fd,void *buf, int size,int flag){
    fd_set rset;
    FD_ZERO(&rset);

    FD_SET(fd, &rset);
    int maxfd = fd + 1;

    if(flag & O_NONBLOCK){
        return read(fd,buf,size);
    } 
    else{
        struct timeval waittime_bk = waittime;
        int rtn = select(maxfd,&rset,NULL,NULL,&waittime);    
        waittime = waittime_bk;
        if(rtn < 0){
            log_ret(LOG_ERR, " error from select:");
            return -1;
        }
        else if(rtn == 0){
            errno = EAGAIN;
            return -1;
        }
        else
            return read(fd,buf,size);
    }
}


int get_msg_from_fd(int fd, void *buf, unsigned int size,CALLBACK func){


    int retvalue = 0;
    int flags = fcntl(fd,F_GETFL);
    if( -1 == flags ){
        log_ret(LOG_WARNING,"fcntl,F_GETFL failed");
        GOOUT(-1)  
    }

    if(buf == NULL || size == 0) return -3;

    int idx = GetFDIndex(fd);
    if(idx < 0){
        log_msg(LOG_WARNING,"unregistered fd");
        return -3;
    }


    unsigned int *s = &fd_lst[idx].size;
    unsigned char *state = &fd_lst[idx].state;

    //printf("index is %d, size is %d, state is %d\n",idx,*s,*state);


    //static char data_buf[MAXBUFSIZE] = {};
    //static packet_head head;
    //static unsigned char state = 0;
    //static unsigned int buf_read = 0;

    while(1){
        switch(*state){
            case 0:
                *s = 0;
                *state = 1;
                break;
            case 1:{
                       unsigned long magic = 0;
                       while(ntohl(magic) != MAGICSTART){
                           memmove((char *)&magic+1,&magic,3);
                           int rtn = read_fd(fd,&magic,1,flags);
                           //printf("read 0x%x!\n",*((unsigned char *)&magic));
                           //unsigned char a = (*(unsigned char *)(&head.magic));
                           ////printf("in magic 0x%x\n",a);
                           int local_errno = GET_LAST_ERROR();
                           if(rtn == 0)
                           {   
                               /* we lost the connection */
                               log_msg(LOG_WARNING,"get_msg connection lost: in state 1");
                               GOOUT(-1)
                           }
                           else if(rtn == -1)
                           {   
                               if( !(local_errno == EAGAIN || local_errno == EWOULDBLOCK ))
                               {   
                                   log_ret(LOG_WARNING,"get_msg error,read:");
                                   GOOUT(-4)
                               }
                               else
                               {   
                                   /* nothing to read right now */
                                   GOOUT(0)
                               }
                           }

                       }
                       *state = 2;
                       break;
                   }
            case 2:{                
                       //printf("fd is %d\n",fd);
                       int rtn = read_fd(fd, s,4,flags);
                       int local_errno = GET_LAST_ERROR();

                       if(rtn == -1){
                               if( !(local_errno == EAGAIN || local_errno == EWOULDBLOCK )){   
                                   log_ret(LOG_WARNING,"read packet size:");
                                   GOOUT(-4)
                               }
                               else{
                                   GOOUT(0)  
                               }
                       }
                       else if(rtn == 0) {
                               log_msg(LOG_WARNING,"get_msg connection lost: in state 2");
                               GOOUT(-1)
                       }


                       if(rtn != 4){
                           log_msg(LOG_WARNING,"read size in packet head failed,read return %d",rtn);
                           GOOUT(-4)
                       }
                       *s= ntohl(*s);
                       if( *s > size){
                           log_msg(LOG_WARNING,"the buf size is %u, the packet body size is %u",size, *s);
                           GOOUT(-2)
                       }
                       if(0 == *s){
                           log_msg(LOG_WARNING,"The size in packet head is 0 which is not allowed!");
                           state = 0;
                           GOOUT(-4)
                       }
                       *state = 3;
                       break;
                   }
            case 3:{
                       int numofbytes;
                       if(flags & O_NONBLOCK){
                           ////printf("state = 4\n");
                           if(ioctl(fd, FIONREAD, &numofbytes) != 0){
                               log_ret(LOG_ERR,"get_msg_from_fd: ioctl:");
                               GOOUT(-1)
                           }
                           ////printf("ioctl return num %d\n",numofbytes);
                           if(numofbytes < *s) GOOUT(0)
                       }
                       int received = 0;
                       do{
                           int rtn = read_fd(fd,(char *)buf + received,*s-received,flags);
                           int local_errno = GET_LAST_ERROR();
                           if(rtn == 0){
                               log_msg(LOG_NOTICE,"get_msg connection lost: in state 3");
                                GOOUT(-1)
                           }
                           else if(rtn == -1)
                           {   
                               if( !(local_errno == EAGAIN || local_errno == EWOULDBLOCK ))
                               {   
                                   log_ret(LOG_WARNING,"get_msg error,read:");
                                   GOOUT(-4)
                               }
                               else //only O_NONBLOCK fd will drop in
                               {   
                                   /* noting to read right now */
                                   log_msg(LOG_WARNING,"Ioctl tells could read %d,size of packet body is %d,return of read is %d.Check the ioctl usage",numofbytes,*s,rtn);
                                   GOOUT(-4)
                               }
                           }
                           else
                                received += rtn;
                       }while(received < *s);

                   //    if( != *s){
                    //       log_msg(LOG_WARNING,"Check the ioctl usage, icctl tells could read %d,size of packet body is %d,return of read is %d",numofbytes,*s,rtn);
                     //      GOOUT(-1);
                    //   }
                       GOOUT(received)
                   }
            default:
                   log_msg(LOG_WARNING,"Invalid state %d",*state);
                   GOOUT(-1)
        }
    }
out:
    if(retvalue == -1 || retvalue == -2 || retvalue == -3 || retvalue == -4){
        *state = 0;
    }
    if(retvalue >0){
        *state = 0;
        if(func) func(buf,*s);
    }
    return retvalue;
}
int get_msg_from_bus(unsigned char channel_id, void *buf, unsigned int size,CALLBACK func){

    int retvalue = 0;
    int fd = -1;


    if(IsRegistered(channel_id,&fd)){ 
        retvalue = -3;
        goto out;
    }

    retvalue = get_msg_from_fd(fd,buf,size,func);
out:
    return retvalue;
}

int put_msg_to_fd(int fd,void *buf, unsigned int size, CALLBACK func){

    int retvalue = 0;
    if(buf == NULL || size == 0){
        retvalue = -3;
        goto out;
    }

   
    
    if(size > MAXBUFSIZE - sizeof (packet_head)){
        retvalue = -2;
        goto out;
    }

	//print_elapsed_time();printf("before call back\n");
    if(NULL != func)
        func(buf,size);
	//print_elapsed_time();printf("after call back\n");

	//print_elapsed_time();printf("before memory op\n");
   char packet_buf[MAXBUFSIZE]; 

    packet_head head;
    head.magic = MAGICSTART;
    head.size = htonl(size);
    memcpy(packet_buf,&head,sizeof head);
    memcpy(packet_buf+sizeof head,buf,size);
	//print_elapsed_time();printf("after memory op\n");
    
    int data_size = sizeof(packet_head) + size;
	//print_elapsed_time();printf("before write\n");
    int ret = write(fd,packet_buf, data_size);
	//print_elapsed_time();printf("after write\n");
    //int i = 0;
    //printf("data size %d\n",data_size);
//    for(i=0;i<data_size;i++){
 //       if(i<4)
            //printf("%d ",packet_buf[i]);
  //      else if(i==4){
            //printf("%d ",ntohl(*((int *)(&packet_buf[i]))));
   //         i+=3;
    //    }
     //   else{
            //printf("%s\n",&packet_buf[i]);
      //      break;
       // }
   // }



    if(ret < 0){
        if(errno == EPIPE)
            GOOUT(-1)
        else if(errno == EAGAIN || errno == EWOULDBLOCK)        
            GOOUT(-5)
        else{
            log_ret(LOG_ERR,"put_msg_to_fd writes packet error:");
            GOOUT(-4)
        }
    }
    else{ 
        if(ret != data_size){
            log_msg(LOG_WARNING, "put_msg_to_fd writes part of the packet to the socket.");
            GOOUT(-5)
        }
        else
            GOOUT(0)
    }
    //log_msg(LOG_DEBUG,"send buf to daemon successfully!");
out: 
    return retvalue;
}

int put_msg_to_bus(unsigned char channel_id,void *buf, unsigned int size, CALLBACK func){
    int retvalue = 0;
    int fd = -1;
//print_elapsed_time();printf("before isregistered\n");
    if(IsRegistered(channel_id,&fd)){ 
        retvalue = -3;
        goto out;
    }
//print_elapsed_time();printf("after isregistered\n");
    retvalue = put_msg_to_fd(fd,buf,size,func);

out: 
    return retvalue;
}

int msgbus_init(unsigned char channel_id,unsigned char op_flag){

    int retvalue = 0;

    if(channel_id == 0){
        retvalue = -3;
        goto out;
    }
    int empty_idx = -1;
    int i =0;
    for(i=0;i<MAX_CHAN_SIZE;i++){
        if(cf_pair[i].channel_id == channel_id){
            GOOUT(-3)
        }
        if(empty_idx == -1 && cf_pair[i].channel_id == 0)     //empty position
            empty_idx=i;
    } 
    cf_pair[empty_idx].channel_id = channel_id;
    if(empty_idx == -1){
        log_msg(LOG_NOTICE,"Only %d channels could be registered by one process!",MAX_CHAN_SIZE);
        GOOUT(-2)
    }
    char *TECHOME = getenv("TEC");
    char filename[128] = {};
    strcat(filename,TECHOME);strcat(filename,"/etc/");strcat(filename,CONFIGFILE);
    ////printf("config file name %s\n",filename);

    FileInfo fInfo;
    if(op_flag & REMOTE){
        char ip_char[32];
        unsigned long port,ip;
        ini_gets("Remote","ip","127.0.0.1",ip_char,32,filename);
        port = ini_getl("Remote","port",10000,filename);
        if(port > 65535){
            log_msg(LOG_WARNING,"The port in msg.ini is invalid!");
            GOOUT(-3)
        }
        unsigned short p = port; 
        int s = inet_pton(AF_INET,ip_char,&ip);
        if (s <= 0) {
            if (s == 0)
                log_msg(LOG_WARNING,"the ip string in msg.ini is not in presentation format");
            else
                log_ret(LOG_WARNING,"inet_pton");
            GOOUT(-3)
        } 
        fInfo.type = SocketIn;
        fInfo.AddrDes.iDes.ifHost = 0; 
        fInfo.AddrDes.iDes.iaddr.sin_family = AF_INET;
        fInfo.AddrDes.iDes.iaddr.sin_port = htons(p);
        fInfo.AddrDes.iDes.iaddr.sin_addr.s_addr = ip;
    }
    else{
        char file[108];
        ini_gets("Local", "file", "", file,128,filename); 
        fInfo.type = SocketUn;
        fInfo.AddrDes.uDes.ifHost = 0;
        fInfo.AddrDes.uDes.uaddr.sun_family = AF_UNIX;
        strcpy(fInfo.AddrDes.uDes.uaddr.sun_path, file);
        ////printf("%s\n",file);
    }

    cf_pair[empty_idx].fd = GetFDfromFAC(&fInfo);
    if(cf_pair[empty_idx].fd < 0){
        log_msg(LOG_WARNING,"GetFDfromFAC error!");
        GOOUT(-3)
    }

    assert(register_fd(cf_pair[empty_idx].fd,op_flag) == 0);
    /*Since the fd from FDFac is nonblock and here we need talk with msg daemon in block way, change it to block and after the talk change it back*/
    if(!(op_flag & BLOCK))
        assert(SwitchBlock(cf_pair[empty_idx].fd,1) == 0);


    /*begin talk with daemon*/
    unsigned char firstPacket[2];
    //*((int *)firstPacket) = 0xfefefefe;
    //int size = 2;
    //*((int *)(firstPacket+4)) = htonl(size);
    
    firstPacket[0] = channel_id;
    firstPacket[1] = op_flag & ~REMOTE;
    
    int rtn = put_msg_to_fd(cf_pair[empty_idx].fd,firstPacket,2,NULL); 




    //int rtn = write(cf_pair[i].fd, firstPacket,SIZEARRAY(firstPacket));
    if(rtn != 0)    GOOUT(-1) 
    
    memset(firstPacket,0,SIZEARRAY(firstPacket)); 
	//printf("before get_msg_from_fd\n");
    do{
	  //  printf("waiting for register response\n");
        rtn = get_msg_from_fd(cf_pair[empty_idx].fd,firstPacket,SIZEARRAY(firstPacket),NULL);
	    usleep(10*1000);
    }while(rtn == 0);
    //printf("rtn from register is %d\n",rtn);

  
    if(rtn == 1){
        unsigned char code = firstPacket[0];
        switch(code){
            case REG_CHANNEL_SUCCESS:
                log_msg(LOG_INFO,"init channel %d successfully!",channel_id);
		        GOOUT(0) 
            case ERR_CHANNEL_ID_INVALID:
                log_msg(LOG_WARNING, "The channel_id %d is invalid!",channel_id);
		        GOOUT(-1) 
            case ERR_RES_REACH_MAX:
                log_msg(LOG_WARNING, "The msg bus daemon refuses to server for the resource limit!");
		        GOOUT(-1) 
            case ERR_PATTERN_INVALID:
                log_msg(LOG_WARNING, "Msgbus daemon says invalid pattern!");
		        GOOUT(-1) 
            default:
                log_msg(LOG_WARNING, "unknown error code %d from msg bus daemon!",code);
		        GOOUT(-1) 
        }
    }
    else{
        log_msg(LOG_WARNING,"unexpected answer from server,return size is %d",rtn);
		GOOUT(-1) 
    }
out:
    if(retvalue == -1){
        unregister_channel(cf_pair[empty_idx].channel_id); 
    }
    if(retvalue == 0 && (!(op_flag & BLOCK)))
        assert(SwitchBlock(cf_pair[empty_idx].fd,0) == 0);
    return retvalue;
}

