/* msg_bus.i */
%module msg_bus
%{
#include "channel_define.h"
#include "msg_define.h"
#include "msg_bus.h"
#include "apue.h"
#include "minIni.h"
#include "blocknum.h"
#include <errno.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <assert.h>
#include "fdfactory.h"
#include "PointPacket.h"
    char buf[4096];
    int putmsg(int channel_id, char *msg, int size)
    {
        return put_msg_to_bus(channel_id,(void *)msg, size, NULL);
    }
    char *getmsg(int channel_id)
    {
        int ret = get_msg_from_bus(channel_id, (void *)buf, 4096, NULL);
        if(ret > 0)
            return buf;
        return NULL;
    }
    IPCData package[MAXIPCDATA];
    int putipcdata(int channel_id, int number_of_data)
    {
        assert(number_of_data <= MAXIPCDATA);
        return put_msg_to_bus(channel_id,(void *)package, number_of_data * sizeof(IPCData), HostToNetFunc);
    }
    int getipcdata(int channel_id)
    {
        int ret = get_pointPacket(channel_id, package, sizeof(IPCData) * MAXIPCDATA);
        return ret;
    }
    IPCData *accessipcdata(int idx)
    {
        assert(idx <= MAXIPCDATA && idx >= 0);
        return package + idx; 
    }
%}

%include "channel_define.h"
%include "msg_define.h"
%include "msg_bus.h"
%include "PointPacket.h"
%include "blocknum.h"

char buf[4096];
int putmsg(int channel_id, char *msg, int size)
{
    return put_msg_to_bus(channel_id,(void *)msg, size, NULL);
}
char *getmsg(int channel_id)
{
    int ret = get_msg_from_bus(channel_id, (void *)buf, 4096, NULL);
    if(ret > 0)
        return buf;
    return NULL;
}
IPCData package[MAXIPCDATA];
int putipcdata(int channel_id, int number_of_data)
{
    assert(number_of_data <= MAXIPCDATA);
    return put_msg_to_bus(channel_id,(void *)package, number_of_data * sizeof(IPCData), HostToNetFunc);
}
int getipcdata(int channel_id)
{
    int ret = get_pointPacket(channel_id, package, sizeof(IPCData) * MAXIPCDATA);
    return ret;
}
IPCData *accessipcdata(int idx)
{
    assert(idx <= MAXIPCDATA && idx >= 0);
    return package + idx; 
}
