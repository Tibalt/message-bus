#ifndef IPC_H
#define IPC_H
#ifdef  __cplusplus
extern "C" {
#endif
#define MAXIPCDATA 102
#define MAGICSTART 0xfefefefe

typedef union{
    float f; //anlog value, like H2 ppm
    unsigned long d; //integer value, like OLTC position
    unsigned short  q; //qulity
}IPCValue;

typedef struct{
    unsigned long magic;
    unsigned long number;
}IPCHead;  //used by libmsgbus to assemble the packet

typedef struct{
    unsigned long   key_id;   //say hi to rt21
    IPCValue        value;
}IPCData;



/*An introduction to blockno related Key_id*
 * style of Key_id 0xAB00XXXX
    A is 8(1000)
    B is IPCData type. B=1(0001), IPCData represents float value;B=2(0010), IPCData represents integer value;B=4(0011) events;A=8(1000), IPCData represents quality;
    XXXX(0~65535) is the blockno following the rules defined in /trunk/document/1ZSC000734.
 * */

/**
 * @brief point packet protocl applied on msg bus
 *
 * @param Channel_id
 * @param buffer
 * @param size
 *
 * @return:  0, nothing is received
 *           >0, the number of IPCData
 *           -1, rtn(get_msg_from_bus)%sizeof IPCData != 0
 *           -2,-3, same meaning with the ones of return values of get_msg_from_bus 
 */
int get_pointPacket(int Channel_id, void *buffer, unsigned int size);

void NetToHostFunc(void *buf,int size);
void HostToNetFunc(void *buf,int size);




#ifdef  __cplusplus
};
#endif
#endif
