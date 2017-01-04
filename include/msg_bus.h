//#include "buffer.h"

#ifndef _MSG_BUS_H
#define _MSG_BUS_H
#ifdef  __cplusplus
extern "C" {
#endif

#define    MSG_GET      0x01
#define    MSG_PUT      0x02
#define    LOOP         0x04
#define    NO_HISTORY   0x08
#define    REMOTE       0x10
#define    BLOCK        0x20
#define    MSG_STATUS   0x40



/**
 * @brief Callback      Function which process buf. 
 *
 * @param buf           In get_msg_from_bus, the buf is what it get from msg bus. In put_msg_to_bus, the buf will be processed by this function before put it to msg bus.
 * @param size          The size of buf counting in byte.
 *
 * @return 
 */
typedef void  (*CALLBACK) (void *buf, const int size);
 
/**
 * @brief               Talk to msg bus daemon to get a fd, will block if no reply for the init packet from dameon 
 *
 * @param channel_id 
 * @param op_flag       MSG_GET,MSG_PUT or MSG_GET|MSG_PUT
 *                      REMOTE refers to connecting to the msg bus daemon using internet socket
 *                      LOOP referts to send the msg to itself
 *                      NO_HISTORY If there is content in the data buffer in msg bus daemon, drop the content; Otherwise gets the history data if there is in the buffer.
 *                      NON_BLOCK Fd works in the non block way. 
 * @return              0 successful; -2 falied as MAX_CHAN_SIZE has been registered; -1 failed to talk to msg bus daemon; -3 invalid parameters or errors in msg.ini or talk to FDFac failed
 */
int msgbus_init(unsigned char channel_id,unsigned char op_flag);

/**
 * @brief               Get message from bus
 *
 * @param channel_id    Channel_id which has been registered 
 * @param buf           An empty buffer allocated by user
 * @param size          Size of buffer, 
 * @param func          CALLBACK function pointer, process the data in buf before the get_msg_from_bus returns.
 *
 * @return              >0 actual received data size; 0 nothing is received; -1,connection lost; -2, the parameter size is smaller than the actual data size in packet, get_msg_from_bus will drop this packet; -3 invalid parameters(such as buf == NULL or size == 0); -4, read error from read() or accroding to message bus protocol
 */
int get_msg_from_bus(unsigned char channel_id, void *buf, unsigned int size,CALLBACK func);



/**
 * @brief       Instead of send/recv to/from msg bus, the user could register own file descriptor and use this function to communicate. The benefit is data is sent in the "packet" format. 
 *              Notice  1: No checksum is done in this function, so NOT to use this function in serial communication.
 *                      2: Would NOT close the fd no matter succeed or not 
 *
 * @param fd    File descriptor
 * @param buf   Same as get_msg_from_bus
 * @param size  Same as get_msg_from_bus
 * @param func  Same as get_msg_from_bus
 *
 * @return      Same as get_msg_from_bus. Besides, if fd is not intialized, will return -1
 */
int get_msg_from_fd(int fd, void *buf, unsigned int size,CALLBACK func);

/**
 * @brief                Put message to bus. 
 *
 * @param channel_id     Channel_id which has been registered
 * @param buf            Data buffer
 * @param size           Data buffer size,when the buf == NULL && size == 0, the packet without packet boady will be sent
 * @param func           CALLBACK function pointer, process the data in buf before put it on to msg bus.
 *
 * @return               same as put_msg_to_fd, and when channel_id is not registered, return -3 
 */
int put_msg_to_bus(unsigned char channel_id,void *buf, unsigned int size,CALLBACK func);
/**
 * @brief             Instead of send/recv to/from msg bus, the user could initialize own file descriptor and use this function to communicate. The benefit is data is sent in the "packet" format. 
 *                    Notice  1: No checksum is done in this function, so NOT to use this function in serial communication.
 *                            2: Would NOT close the fd no matter succeed or not        
 *
 * @param fd          File descriptor
 * @param buf         Same as put_msg_to_bus
 * @param size        Same as put_msg_to_bus
 * @param func        Same as put_msg_to_bus
 *                            
 * @return            0, good;
 *                    -1, reading end is closed
 *                    -2, the size is too large to put onto msg bus (limit is 4096 - 8);
 *                    -3, invalid parameters; 
                      -4,if errno is other than EAGAIN and EWOULDBLOCK when write failure 
 *                    -5,if errno is EAGAIN or EWOULDBLOCK when write failure, or paritally write
 *                      
 */
int put_msg_to_fd(int fd,void *buf, unsigned int size,CALLBACK func);


/**
 * @brief Register fd which is intialized before use put_msg_to_fd or get_msg_from fd.
 *
 * @param fd
 * @param op, NON-BLOCK: op & 0x20 == 1, BLOCK: op & 0x20 == 0
 *
 * @return 0, success, -1 has been registered,-2 can not register due to resource limit, -3 fd <= 0 . 
 */
int register_fd(int fd,int op);


/**
 * @brief Cancel the registeration of fd when do not use it anymore. Would NOT close fd. DIY. 
 *
 * @param fd
 *
 * @return 0, success, -1 has not been registered, -3 fd <= 0 . 
 */

int unregister_fd(int fd);

/**
* @brief Do as what the name says
*
* @param channel_id
*
* @return 0,success, -1 invalid channel id(id >0 && id < MAX_CHAN_SIZE) or channel id is not registered
*/
int unregister_channel(unsigned char  channel_id);


/**
* @brief Get the fd of the channel
*
* @param channel
*
* @return >=0, fd descriptor; -1,channel is invalid  
*/
int get_fd(int channel_id);

/**
* @brief Set the select block time, only valid when set the flag BLOCK
*
* @param timeval
*
* @return void
*/

void set_waittime(int millisecond);

#ifdef  __cplusplus
};
#endif

#endif
