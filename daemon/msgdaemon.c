#include "apue.h"
#include <fcntl.h>
#include <assert.h>
#include <sys/resource.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pthread.h>
#include <errno.h>
#include <sys/select.h>
#include <netinet/in.h> 
#include <arpa/inet.h>
#include <poll.h>
#include "fdfactory.h"
#include "msg_define.h"
#include "msg_bus.h"
#include "minIni.h"
#include "daemonize.h"
#define sizearray(a) (sizeof(a) / sizeof((a)[0]))
#define SERVER_FIFO_NAME "/tmp/msgbus_server_%d_fifo"
#define CLIENT_FIFO_NAME "/tmp/msgbus_client_%d_fifo"

typedef struct MessageData {
    char buf[MAXBUFSIZE];
    int publisher_id;
    int buf_size;
} MessageData;

typedef struct RegReq {
    unsigned char channel_id;
    unsigned char com_pattern;
} RegReq;

typedef struct fdmsg {
    unsigned char fd;
    unsigned char com_pattern;
	int ident;
} fdmsg;

typedef struct Chan {
    unsigned int 	channel_id;
    struct pollfd 	fds[MAX_FD_SIZE];
	int				fd_ids[MAX_FD_SIZE];
    unsigned int 	pattern[MAX_FD_SIZE];
} Chan;

typedef struct ChanRecords {
    Chan channels[MAX_CHAN_SIZE]; 
    unsigned int index;
} ChanRecords;

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t ready = PTHREAD_COND_INITIALIZER;

pthread_mutex_t unreg_mut = PTHREAD_MUTEX_INITIALIZER;

void non_block_fd(int fd)
{
    int flags = fcntl(fd,F_GETFL);
	if( -1 == flags ){
		log_ret(LOG_ERR,"fcntl,F_GETFL failed");
		return;
	}

	flags = flags | O_NONBLOCK;

	if( -1 == fcntl(fd,F_SETFL,flags)  ){
		log_ret(LOG_ERR,"fcntl,F_SETFL failed");
		return;
	}

}

void close_unreg_fd(int fd)
{
    close(fd);
    pthread_mutex_lock( &unreg_mut );
    int ret = unregister_fd(fd);
	if(ret < 0)
	{
		log_msg(LOG_ERR, "unregister_fd faild, return %d", ret);
	}
    else
    {
        log_msg(LOG_NOTICE, "unregister_fd finished, fd %d", fd);
    }
    pthread_mutex_unlock( &unreg_mut );
}

ChanRecords all_channels;

void initialize()
{
    all_channels.index = 0;
    int i;
    int j;
    for(i=0; i<MAX_CHAN_SIZE; i++)
    {
        all_channels.channels[i].channel_id = 0;
        for(j=0; j<MAX_FD_SIZE; j++)
        {
            all_channels.channels[i].fds[j].fd = -1;
            all_channels.channels[i].pattern[j] = 0;
            all_channels.channels[i].fd_ids[j] = -1;
        }
    }
}

int chan_capacity_enough()
{
    if(all_channels.index >= MAX_CHAN_SIZE)
    {
         return 0;
    }
    return 1;
}

int find_channel_id(unsigned char channel_id)
{
    int i;
    for(i=0; i<all_channels.index; i++)
    {
        if(all_channels.channels[i].channel_id == channel_id)
        {
            return i;
        }
    }
    return -1;
}

int add_channel_id(unsigned char channel_id)
{
    assert(all_channels.index < MAX_CHAN_SIZE);
    all_channels.channels[all_channels.index].channel_id = channel_id;
    all_channels.index++;
    return (all_channels.index - 1);
}

void *ServiceHandle(void *param);
void res_not_enough(unsigned char channel_id, int fd)
{
    log_msg(LOG_ERR, "the resource is not enough for channel id: %d, fd: %d", channel_id, fd);
    unsigned char error_code = ERR_RES_REACH_MAX;
    if( put_msg_to_fd(fd, (void *)&error_code, sizeof(error_code), NULL) != 0 )
    {
        log_msg(LOG_ERR, "put_msg_to_fd failed, close the fd: %d", fd);
        close_unreg_fd(fd);
    }
}
void ReceiveRequest(RegReq reg, int fd, int ident)
{
    int index = find_channel_id(reg.channel_id);
    if(index == -1)
    {
        if(chan_capacity_enough())
        {
            //new channel id   
            index = add_channel_id(reg.channel_id);
    		all_channels.channels[index].fds[1].fd 	= fd;
			all_channels.channels[index].fds[1].events = POLLIN;
    		all_channels.channels[index].pattern[1] = reg.com_pattern;
    		all_channels.channels[index].fd_ids[1] 	= ident;
			//start a new thread to service this channel id
			pthread_t threadId;
			pthread_mutex_lock(&lock);
			int create_retval = pthread_create(&threadId, NULL, ServiceHandle, NULL);
			if(create_retval != 0)
			{
				pthread_mutex_unlock(&lock);
				log_msg(LOG_WARNING, "pthread_create failed: Insufficient resources to create another service thread.");
				res_not_enough(reg.channel_id, fd);
				//roll back
				all_channels.channels[index].fds[1].fd 	= -1;
				all_channels.channels[index].pattern[1] = 0;
				all_channels.index--;
			}
			else
			{
				/* added by felix 8/23/2013 */
				pthread_cond_wait(&ready, &lock);
				pthread_mutex_unlock(&lock);

				unsigned char error_code = 0;
				if( put_msg_to_fd(fd , (void *)&error_code, sizeof(error_code), NULL) != 0 )
				{
					log_msg(LOG_ERR, "put_msg_to_fd failed, the register is failed for channel id: %d, fd: %d", reg.channel_id, fd);
					close_unreg_fd(fd);
					all_channels.channels[index].fds[1].fd 	= -1;
					all_channels.channels[index].pattern[1] = 0;
					all_channels.index--;
				}
				else
				{
                    if (reg.com_pattern & MSG_STATUS)
                    {
                        char diag_msg[256]="";
                        sprintf(diag_msg, "\tchannel(%d) is not in service now\n", reg.channel_id);
                        sprintf(diag_msg+strlen(diag_msg), "\tEND of channel(%d)\n", reg.channel_id);
                        put_msg_to_fd(fd, (void *)(diag_msg), strlen(diag_msg), NULL);
                        close_unreg_fd(fd);
                        all_channels.channels[index].fds[1].fd 	= -1;
                        all_channels.channels[index].pattern[1] = 0;
                    }
                    else
                    {
                        //The register is finished successfully
                        log_msg(LOG_NOTICE, "the register is finished sucessfully for channel id: %d, fd: %d, ident: %d", reg.channel_id, fd, ident);
                    }
				}
			}
		}
        else
        {
            res_not_enough(reg.channel_id, fd);
        }
    }
    else
    {
        //already has this channel id, don't need to start a new thread to service
		log_msg(LOG_NOTICE, "waiting for adding fd. channel id: %d, fd: %d", reg.channel_id, fd);
		fdmsg message;
		message.fd = fd;
		message.com_pattern = reg.com_pattern;
		message.ident = ident;
		int ret = write(all_channels.channels[index].fds[0].fd, (void *)&message, sizeof(fdmsg));
		if(ret != sizeof(fdmsg))
		{
			log_msg(LOG_ERR, "write message failed. channel id: %d, fd: %d", reg.channel_id, fd);
			res_not_enough(reg.channel_id, fd);
		}
	}
}

void AcceptHandle(int fd, int ident)
{
    //log_msg(LOG_NOTICE, "start a new thread to handle a new incoming connection. fd is %d", fd);

    int retval;
    fd_set rset;
    FD_ZERO(&rset);
    struct timeval waittime;

    FD_SET(fd, &rset);
    int maxfd = fd + 1;
    waittime.tv_sec = 0;
    waittime.tv_usec = 2000 * 1000;
    retval = select(maxfd, &rset, NULL, NULL, &waittime);
    if(retval < 0)
    {
        log_ret(LOG_ERR, "error from select, close the fd: %d", fd);
        close(fd);
    }
    else if(retval == 0)
    {
        log_msg(LOG_WARNING, "no receiving in 2 seconds, close the fd: %d", fd);
        close(fd);
    }
    else
	{
		int reg_ret = register_fd(fd, 0);
		if(reg_ret < 0)
		{
			log_ret(LOG_ERR, "failed to register fd: %d, return: %d", fd, reg_ret);
			close(fd);
			return;
		}
		//printf("register return : %d\n", register_fd(fd));
        RegReq pkg_body;
        int returnval = get_msg_from_fd(fd, (void *)&pkg_body, sizeof(RegReq), NULL);
        if( returnval != sizeof(RegReq) )
        {
            log_msg(LOG_ERR, "length of register request is incorrect, return value is: %d, close the fd: %d", returnval, fd);
            close_unreg_fd(fd);
        }
        else
        {
            //verify if the channel id is valid
            if(pkg_body.channel_id == 0)
            {
                log_msg(LOG_ERR, "channel id is invalid, the fd: %d", fd);
                unsigned char error_code = ERR_CHANNEL_ID_INVALID;
                if( put_msg_to_fd(fd, (void *)&error_code, sizeof(error_code), NULL) != 0 )
                {
                    log_msg(LOG_ERR, "put_msg_to_fd failed, close the fd: %d", fd);
                    close_unreg_fd(fd);
                }
            }
            else
            {
                //verify if the pattern is valid
                if( (pkg_body.com_pattern & 0x80) != 0 )
                {
                    log_msg(LOG_ERR, "the pattern is invalid, the fd: %d", fd);
                    unsigned char error_code = ERR_PATTERN_INVALID;
                    if( put_msg_to_fd(fd, (void *)&error_code, sizeof(error_code), NULL) != 0 )
                    {
                        log_msg(LOG_ERR, "put_msg_to_fd failed, close the fd: %d", fd);
                        close_unreg_fd(fd);
                    }
                }
                else
                {
                    ReceiveRequest(pkg_body, fd, ident);
                }
            }
        }
    }
}

void *ServiceHandle(void *param)
{
    Chan *chan = &all_channels.channels[all_channels.index - 1];
    assert(chan != NULL);
    log_msg(LOG_NOTICE, "start a new thread to service channel id: %d.", chan->channel_id);
	char pathname[255];
	int retval;
	sprintf(pathname, SERVER_FIFO_NAME, chan->channel_id);
	if(access(pathname, F_OK) == -1)
	{
		retval = mkfifo(pathname, 0777);
		if(retval != 0)
		{
			log_sys(LOG_CRIT, "failed to mkfifo");
		}
	}
	
	chan->fds[0].fd = open(pathname, O_RDWR|O_NONBLOCK);
	chan->fds[0].events = POLLIN;

	pthread_cond_signal(&ready);

    int nready;
    int i;
	// added by felix on 12/12/2013
#define MSG_BUF_NUM 201
	MessageData messages[MSG_BUF_NUM];
	int w_idx = 0;
	int r_idx = 0;

	fdmsg added_info;
	added_info.fd = -1;

    /* diagnosis information */
    unsigned int buffer_full_happens = 0;
    unsigned int fd_unregistered_happens = 0;
    unsigned int msg_send_failed_happens = 0;
    unsigned int msg_send_bufferfull_happens = 0;
    unsigned int buffer_clear_happens = 0;
    unsigned int poll_error_happens = 0;

	while(1)
	{
		/* First, get messages from this channel */
        nready = poll(chan->fds, MAX_FD_SIZE-1, -1);
        if(nready > 0)
        {
			//printf("nready = %d\n", nready); 
            for(i=0; i < MAX_FD_SIZE && nready > 0; i++)
            {
                if(chan->fds[i].revents & POLLERR )
                {
                    log_msg(LOG_NOTICE, "*%d* poll error, close the fd: %d, remove this fd", chan->channel_id, chan->fds[i].fd);
                    close_unreg_fd(chan->fds[i].fd);
                    chan->fds[i].fd = -1;
					nready--;
                    fd_unregistered_happens++;
                    poll_error_happens++;
                }
                else if(chan->fds[i].revents & POLLHUP )
				{
					//printf("POLLHUP i=%d\n", i);
					log_msg(LOG_NOTICE, "*%d* POLLHUP, close the fd: %d, remove this fd", chan->channel_id, chan->fds[i].fd);
					close_unreg_fd(chan->fds[i].fd);
					chan->fds[i].fd = -1;
					nready--;
                    fd_unregistered_happens++;
				}
                else if(chan->fds[i].revents & POLLNVAL )
				{
					//printf("POLLNVAL i=%d\n", i);
					log_msg(LOG_NOTICE, "*%d* POLLNVAL, close the fd: %d, remove this fd", chan->channel_id, chan->fds[i].fd);
					close_unreg_fd(chan->fds[i].fd);
					chan->fds[i].fd = -1;
					nready--;
                    fd_unregistered_happens++;
				}

                else if(chan->fds[i].revents & POLLIN)
				{
					//printf("POLLIN i=%d\n", i);
					if(i == 0)
					{
						read(chan->fds[0].fd, &added_info, sizeof(fdmsg));
						log_msg(LOG_NOTICE, "*%d* , add the fd: %d, pattern: %04x, ident: %d", chan->channel_id, added_info.fd, added_info.com_pattern, added_info.ident);
                        /* check if MSG_STATUS firstly */
                        if (added_info.com_pattern & MSG_STATUS)
                        {
							log_msg(LOG_NOTICE, "The register is for MGS_STATUS. channel id: %d, fd: %d", chan->channel_id, added_info.fd);
							unsigned char error_code = 0;
							if( put_msg_to_fd(added_info.fd , (void *)&error_code, sizeof(error_code), NULL) != 0 )
							{
								log_msg(LOG_ERR, "put_msg_to_fd failed, the registing is failed for channel id: %d, fd: %d", chan->channel_id, added_info.fd);
								close_unreg_fd(added_info.fd);
							}
                            else
                            {
                                int fd_numbers = 0;
                                int fd_MSG_GET = 0;
                                int fd_MSG_PUT = 0;
                                for(int j=1; j<MAX_FD_SIZE; j++)
                                {
                                    if(chan->fds[j].fd != -1 && chan->fds[j].fd != chan->fds[i].fd)
                                    {
                                        fd_numbers++;
                                        if(chan->pattern[j] & MSG_GET)
                                        {
                                            fd_MSG_GET++;
                                        }
                                        if(chan->pattern[j] & MSG_PUT)
                                        {
                                            fd_MSG_PUT++;
                                        }
                                    }
                                }
                                char diag_msg[4096]="";
                                sprintf(diag_msg, "\tchannel(%d) diagnosis info{\n", chan->channel_id);
                                sprintf(diag_msg+strlen(diag_msg), "\ttotal fd numbers: %d\n", fd_numbers);
                                sprintf(diag_msg+strlen(diag_msg), "\t\twith MSG_GET numbers: %d\n", fd_MSG_GET);
                                sprintf(diag_msg+strlen(diag_msg), "\t\twith MSG_PUT numbers: %d\n", fd_MSG_PUT);
                                sprintf(diag_msg+strlen(diag_msg), "\tbuffer overflow happened: %d\n", buffer_full_happens);
                                sprintf(diag_msg+strlen(diag_msg), "\tmsg send failed happened: %d\n", msg_send_failed_happens);
                                sprintf(diag_msg+strlen(diag_msg), "\t\tdue to write buffer overflow happened: %d\n", msg_send_bufferfull_happens);
                                sprintf(diag_msg+strlen(diag_msg), "\tbuffer clear happened: %d\n", buffer_clear_happens);
                                sprintf(diag_msg+strlen(diag_msg), "\tpoll error happened: %d\n", poll_error_happens);
                                sprintf(diag_msg+strlen(diag_msg), "\tunregister fd happened: %d\n", fd_unregistered_happens);
                                sprintf(diag_msg+strlen(diag_msg), "\t}END of channel(%d)\n", chan->channel_id);
                                put_msg_to_fd(added_info.fd, (void *)(diag_msg), strlen(diag_msg), NULL);
                                close_unreg_fd(added_info.fd);
                            }
                        }
                        else
                        {
                            // add this fd to fd list
                            // find a properate place
                            int j;
                            for(j=1; j<MAX_FD_SIZE; j++)
                            {
                                if(chan->fds[j].fd == -1)
                                {
                                    chan->fds[j].fd 	= added_info.fd;
                                    chan->pattern[j] 	= added_info.com_pattern;
                                    chan->fd_ids[j]		= added_info.ident;
                                    chan->fds[j].events	= POLLIN;
                                    chan->fds[j].revents= 0;
                                    break;
                                }
                            }
                            if( j<MAX_FD_SIZE )
                            {
                                log_msg(LOG_NOTICE, "The register has been finished successfully. channel id: %d, fd: %d", chan->channel_id, added_info.fd);
                                //The register is finished successfully. fd has been added to fd list
                                unsigned char error_code = 0;
                                if( put_msg_to_fd(chan->fds[j].fd , (void *)&error_code, sizeof(error_code), NULL) != 0 )
                                {
                                    log_msg(LOG_ERR, "put_msg_to_fd failed, the registing is failed for channel id: %d, fd: %d", chan->channel_id, chan->fds[j].fd);
                                    close_unreg_fd(chan->fds[j].fd);
                                    chan->fds[j].fd = -1;
                                    fd_unregistered_happens++;
                                }
                                else if(added_info.com_pattern & NO_HISTORY)
                                {
                                    //clear the buffer
                                    log_msg(LOG_NOTICE, "Clear the buffer for channel id: %d", chan->channel_id);
                                    w_idx = r_idx;
                                    buffer_clear_happens++;
                                }
                            }
                            else
                            {
                                log_msg(LOG_ERR, "MAX_FD_SIZE reached for channel id: %d", chan->channel_id);
                                res_not_enough(chan->channel_id, added_info.fd);
                            }
                        }
					}
					else
					{
						do
						{
                            //non_block_fd(chan->fds[i].fd);
                            //printf("before retval = %d\n", retval);
							retval = get_msg_from_fd(chan->fds[i].fd, (void *)(messages[w_idx].buf), MAXBUFSIZE, NULL);
                            //printf("retval = %d\n", retval);
							if( retval > 0 )
							{
								//printf("get one message from chan->fds[%d].fd, .publisher_id=%d\n", i, chan->fd_ids[i]);
								messages[w_idx].buf_size = retval;
								messages[w_idx].publisher_id = chan->fd_ids[i];
								messages[w_idx].buf[retval] = 0;
								w_idx = (w_idx + 1) % MSG_BUF_NUM;
								if(w_idx == r_idx)
                                {
									r_idx = (r_idx + 1) % MSG_BUF_NUM;
                                    buffer_full_happens++;
								    //log_msg(LOG_ERR, "*%d* buffer overflow", chan->channel_id, retval, chan->fds[i].fd);
                                }
								//printf("w_idx = %d\n", w_idx);
							}
							else if(retval < 0)
							{
								//remove this fd from fd's list
								log_msg(LOG_ERR, "*%d* get_msg_from_fd failed, return: %d, close the fd: %d, remove this fd", chan->channel_id, retval, chan->fds[i].fd);
								close_unreg_fd(chan->fds[i].fd);
								chan->fds[i].fd = -1;
                                fd_unregistered_happens++;
							}
						} while(retval > 0);
					}
					nready--;
				}
            }
        }
        else if(nready < 0)
        {
            log_ret(LOG_WARNING, "*%d* poll error", chan->channel_id);
            poll_error_happens++;
        }
		/* Second, put these messages */
		while(w_idx != r_idx)  //got messages to send
		{
			//printf("w_idx != r_idx\n");
			int flag = 0;
			for(i=1; i<MAX_FD_SIZE; i++)
			{
				if(chan->fds[i].fd != -1)
				{
                    //printf("chan->fds[%d].fd != -1\n", i);
					if(chan->pattern[i] & MSG_GET)
					{
                        //printf("chan->pattern[%d] & MSG_GET\n", i);
                        //printf("chan->fd_ids[%d]=%d messages[%d].publisher_id=%d\n", i, chan->fd_ids[i], r_idx, messages[r_idx].publisher_id);
						if(chan->pattern[i] & LOOP)
						{
                            int put_ret = put_msg_to_fd(chan->fds[i].fd, (void *)(messages[r_idx].buf), messages[r_idx].buf_size, NULL);
                            if( put_ret == 0 )
                            {
                                //send this message sucessfully!
                                flag = 1;
                            }
                            else //error happened
                            {
                                if( put_ret == -1 )
                                {
                                    log_msg(LOG_WARNING, "*%d* send msg failed due to: reading end is closed, close the fd: %d, remove this fd", chan->channel_id, chan->fds[i].fd);
                                    close_unreg_fd(chan->fds[i].fd);
                                    chan->fds[i].fd = -1;
                                    msg_send_failed_happens++;
                                    fd_unregistered_happens++;
                                }
                                else if( put_ret == -2 )
                                {
                                    log_msg(LOG_WARNING, "*%d* send msg failed due to: the size(%d) is too large to put (limit is 4096 - 8), the fd: %d", chan->channel_id, messages[r_idx].buf_size, chan->fds[i].fd);
                                    flag = 1;
                                    msg_send_failed_happens++;
                                }
                                else if( put_ret == -3 )
                                {
                                    log_msg(LOG_WARNING, "*%d* send msg failed due to: invalid parameters, msg size is: %d, the fd: %d", chan->channel_id, messages[r_idx].buf_size, chan->fds[i].fd);
                                    flag = 1;
                                    msg_send_failed_happens++;
                                }
                                else if( put_ret == -4 )
                                {
                                    log_msg(LOG_WARNING, "*%d* send msg failed due to: other reason, msg size is: %d, close the fd: %d, remove this fd", chan->channel_id, messages[r_idx].buf_size, chan->fds[i].fd);
                                    close_unreg_fd(chan->fds[i].fd);
                                    chan->fds[i].fd = -1;
                                    msg_send_failed_happens++;
                                    fd_unregistered_happens++;
                                }
                                else if( put_ret == -5 )
                                {
                                    log_msg(LOG_WARNING, "*%d* send msg failed due to sending buffer is full, msg size is: %d, the fd: %d", chan->channel_id, messages[r_idx].buf_size, chan->fds[i].fd);
                                    flag = 1;
                                    msg_send_failed_happens++;
                                    msg_send_bufferfull_happens++;
                                }
                                else
                                {
                                    log_msg(LOG_WARNING, "*%d* send msg failed due to: unknown reason, msg size is: %d,  close the fd: %d, remove this fd", chan->channel_id, messages[r_idx].buf_size, chan->fds[i].fd);
                                    close_unreg_fd(chan->fds[i].fd);
                                    chan->fds[i].fd = -1;
                                    msg_send_failed_happens++;
                                    fd_unregistered_happens++;
                                }
                            }
						}
						else if(chan->fd_ids[i] != messages[r_idx].publisher_id)
						{
                            int put_ret = put_msg_to_fd(chan->fds[i].fd, (void *)(messages[r_idx].buf), messages[r_idx].buf_size, NULL);
                            if( put_ret == 0 )
                            {
                                //send this message sucessfully!
                                flag = 1;
                            }
                            else //error happened
                            {
                                if( put_ret == -1 )
                                {
                                    log_msg(LOG_WARNING, "*%d* send msg failed due to: reading end is closed, close the fd: %d, remove this fd", chan->channel_id, chan->fds[i].fd);
                                    close_unreg_fd(chan->fds[i].fd);
                                    chan->fds[i].fd = -1;
                                    msg_send_failed_happens++;
                                    fd_unregistered_happens++;
                                }
                                else if( put_ret == -2 )
                                {
                                    log_msg(LOG_WARNING, "*%d* send msg failed due to: the size(%d) is too large to put (limit is 4096 - 8), the fd: %d", chan->channel_id, messages[r_idx].buf_size, chan->fds[i].fd);
                                    flag = 1;
                                    msg_send_failed_happens++;
                                }
                                else if( put_ret == -3 )
                                {
                                    log_msg(LOG_WARNING, "*%d* send msg failed due to: invalid parameters, msg size is: %d, the fd: %d", chan->channel_id, messages[r_idx].buf_size, chan->fds[i].fd);
                                    flag = 1;
                                    msg_send_failed_happens++;
                                }
                                else if( put_ret == -4 )
                                {
                                    log_msg(LOG_WARNING, "*%d* send msg failed due to: other reason, msg size is: %d, close the fd: %d, remove this fd", chan->channel_id, messages[r_idx].buf_size, chan->fds[i].fd);
                                    close_unreg_fd(chan->fds[i].fd);
                                    chan->fds[i].fd = -1;
                                    msg_send_failed_happens++;
                                    fd_unregistered_happens++;
                                }
                                else if( put_ret == -5 )
                                {
                                    log_msg(LOG_WARNING, "*%d* send msg failed due to sending buffer is full, msg size is: %d, the fd: %d", chan->channel_id, messages[r_idx].buf_size, chan->fds[i].fd);
                                    flag = 1;
                                    msg_send_failed_happens++;
                                    msg_send_bufferfull_happens++;
                                }
                                else
                                {
                                    log_msg(LOG_WARNING, "*%d* send msg failed due to: unknown reason, msg size is: %d,  close the fd: %d, remove this fd", chan->channel_id, messages[r_idx].buf_size, chan->fds[i].fd);
                                    close_unreg_fd(chan->fds[i].fd);
                                    chan->fds[i].fd = -1;
                                    msg_send_failed_happens++;
                                    fd_unregistered_happens++;
                                }
                            }
						}
					}
				}
			}//end of FOR

            
			if(flag)
			{
				r_idx = (r_idx + 1) % MSG_BUF_NUM;
			}
            else
            {
                break;
            }
		}
    }
}

int main(int argc, char **argv)
{
	//printf("int is %d bytes\nunsigned int is %d bytes\npthread_mutex_t is %d bytes\npollfd is %d bytes\nChan is %d bytes\n",
	//		sizeof(int),
	//		sizeof(unsigned int),
	//		sizeof(pthread_mutex_t),
    //		sizeof(struct pollfd),
	//		sizeof(struct Chan));

	signal(SIGPIPE, SIG_IGN);
	log_open("msgdaemon",LOG_LOCAL1);
    char *TECHOME = getenv("TEC");
    if(TECHOME == NULL)
        log_quit(LOG_CRIT, "Can't find environment variable 'TEC' - exiting");
/*
	char *TECHOME = getenv("TECHOME");
	char pwd[64];
	if(getcwd(pwd,64) == NULL)
		log_sys(LOG_NOTICE,"current path name is too long, switch to a simple pos and reboot the program!");
*/

    
	/*
	 * Test if become a daemon process.
	 */
    BECOMEDAEMON
/*
	if( chdir(TECHOME) < 0){
        log_sys(LOG_ERR,"IPCInit chdir error,the path is %s,pls make sure the env \"TECHOME\" refer to the location of  arm.out", TECHOME);
    }
*/
    /*
     * Make sure only one copy of the daemon is running.
     */
    TESTUNIQUE("msgdaemon")

    initialize();

    char filepath[128] = {};
    strcat(filepath,TECHOME);strcat(filepath,"/etc/");strcat(filepath,CONFIGFILE);
    /*
     * Create two sockets:
     * one is UNIX domain socket,
     * the other is Internet socket.
     */
    int fd1, fd2;
    char pathName[128];
    int port;
    ini_gets("Local", "file", "dummy", pathName, sizearray(pathName), filepath);
    if(strcmp(pathName, "dummy") == 0)
    {
        log_sys(LOG_CRIT, "not find file descriptor in %s", filepath);
    }
    port = ini_getl("Remote", "port", -1, filepath);
    if(port == -1)
    {
        log_sys(LOG_CRIT, "not find port number in %s", filepath);
    }
    
    FileInfo file;
    file.type = SocketUn;
    file.AddrDes.uDes.ifHost = 1;
    ((struct sockaddr_un *)&(file.AddrDes.uDes.uaddr))->sun_family = AF_UNIX;
    strcpy(((struct sockaddr_un *)&(file.AddrDes.uDes.uaddr))->sun_path, pathName);
    fd1 = GetFDfromFAC(&file);
    if(fd1 < 0)
    {
        log_sys(LOG_CRIT, "error from  GetFDfromFAC in un_socket");
    }
	log_msg(LOG_NOTICE, "GetFDfromFAC in un_socket return fd1: %d", fd1);

    file.type = SocketIn;
    file.AddrDes.uDes.ifHost = 1;
    ((struct sockaddr_in *)&(file.AddrDes.iDes.iaddr))->sin_family = AF_INET;
    ((struct sockaddr_in *)&(file.AddrDes.iDes.iaddr))->sin_port = htons(port);
    inet_pton(AF_INET, "0.0.0.0", &( ((struct sockaddr_in *)&(file.AddrDes.iDes.iaddr))->sin_addr ));

    fd2 = GetFDfromFAC(&file);
    if(fd2 < 0)
    {
        log_sys(LOG_CRIT, "error from  GetFDfromFAC in in_socket");
    }
	log_msg(LOG_NOTICE, "GetFDfromFAC in in_socket return fd2: %d", fd2);
    

    /*
     * Start main loop.
     */ 
    struct pollfd listen_fd[2];
    listen_fd[0].fd = fd1;
    listen_fd[1].fd = fd2;
    listen_fd[0].events = listen_fd[1].events = POLLRDNORM;
    int nready;
#define MAX_IDEN 65535
	int identifer = 0;

    while(1)
    {
        nready = poll(listen_fd, 2, -1);
        if(nready > 0)
        {
            if(listen_fd[0].revents & POLLRDNORM)
            {
                int fd3 = accept(fd1, NULL, NULL);
                if(fd3 < 0)
                {
                    log_ret(LOG_ERR, "accept fd1");
                }
                log_msg(LOG_NOTICE, "receive a connection from local listening. The returned fd3 is %d", fd3);
                //non_block_fd(fd3);
                AcceptHandle(fd3, identifer);
				identifer = (identifer + 1) % 65535;

            }
            else if(listen_fd[0].revents & (POLLERR | POLLNVAL))
            {
                log_sys(LOG_CRIT, "error from local listening");
            }

            if(listen_fd[1].revents & POLLRDNORM)
            {
                int fd4 = accept(fd2, NULL, NULL);
                if(fd4 < 0)
                {
                    log_ret(LOG_ERR, "accept fd2");
                }
                log_msg(LOG_NOTICE, "receive a connection from remote listening. The returned fd4 is %d", fd4);
                //non_block_fd(fd4);
                AcceptHandle(fd4, identifer);
				identifer = (identifer + 1) % 65535;

            }
            else if(listen_fd[1].revents & (POLLERR | POLLNVAL))
            {
                log_sys(LOG_CRIT, "error from remote listening");
            }
        }
        else
        {
            log_sys(LOG_CRIT, "error from poll");
            //continue;
        }

    }
}

