/*
 * =====================================================================================
 *
 *       Filename:  msgst.c
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  04/14/2016 04:17:40 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Dr. Fritz Mehner (mn), mehner@fh-swf.de
 *        Company:  FH Südwestfalen, Iserlohn
 *
 * =====================================================================================
 */

#include "channel_define.h"
#include "msg_bus.h"
#include <stdio.h>

int main()
{
    char msg[4096]="";
    int rtn = 0;
    rtn = msgbus_init(CHANNEL_CAN, MSG_STATUS|BLOCK);
    if(rtn < 0)
    {
        printf("register CHANNEL_CAN failed, exit\n");
        return -1;
    }
    while(1)
    {
        rtn = get_msg_from_bus(CHANNEL_CAN, msg, 4096, NULL);
        if(rtn < 0)
        {
            break;
        }
        else if(rtn > 0)
        {
            printf("CHANNEL_CAN:\n%s", msg);
        }
    }
    
    rtn = msgbus_init(CHANNEL_MODBUS, MSG_STATUS|BLOCK);
    if(rtn < 0)
    {
        printf("register CHANNEL_MODBUS failed, exit\n");
        return -1;
    }
    while(1)
    {
        rtn = get_msg_from_bus(CHANNEL_MODBUS, msg, 4096, NULL);
        if(rtn < 0)
        {
            break;
        }
        else if(rtn > 0)
        {
            printf("CHANNEL_MODBUS:\n%s", msg);
        }
    }
    
    rtn = msgbus_init(CHANNEL_61850, MSG_STATUS|BLOCK);
    if(rtn < 0)
    {
        printf("register CHANNEL_61850 failed, exit\n");
        return -1;
    }
    while(1)
    {
        rtn = get_msg_from_bus(CHANNEL_61850, msg, 4096, NULL);
        if(rtn < 0)
        {
            break;
        }
        else if(rtn > 0)
        {
            printf("CHANNEL_61850:\n%s", msg);
        }
    }
    
    rtn = msgbus_init(CHANNEL_DNP3, MSG_STATUS|BLOCK);
    if(rtn < 0)
    {
        printf("register CHANNEL_DNP3 failed, exit\n");
        return -1;
    }
    while(1)
    {
        rtn = get_msg_from_bus(CHANNEL_DNP3, msg, 4096, NULL);
        if(rtn < 0)
        {
            break;
        }
        else if(rtn > 0)
        {
            printf("CHANNEL_DNP3:\n%s", msg);
        }
    }
    
    rtn = msgbus_init(CHANNEL_DOM, MSG_STATUS|BLOCK);
    if(rtn < 0)
    {
        printf("register CHANNEL_DOM failed, exit\n");
        return -1;
    }
    while(1)
    {
        rtn = get_msg_from_bus(CHANNEL_DOM, msg, 4096, NULL);
        if(rtn < 0)
        {
            break;
        }
        else if(rtn > 0)
        {
            printf("CHANNEL_DOM:\n%s", msg);
        }
    }
    
    rtn = msgbus_init(CHANNEL_TEC, MSG_STATUS|BLOCK);
    if(rtn < 0)
    {
        printf("register CHANNEL_TEC failed, exit\n");
        return -1;
    }
    while(1)
    {
        rtn = get_msg_from_bus(CHANNEL_TEC, msg, 4096, NULL);
        if(rtn < 0)
        {
            break;
        }
        else if(rtn > 0)
        {
            printf("CHANNEL_TEC:\n%s", msg);
        }
    }
    
    rtn = msgbus_init(CHANNEL_CONTROL, MSG_STATUS|BLOCK);
    if(rtn < 0)
    {
        printf("register CHANNEL_CONTROL failed, exit\n");
        return -1;
    }
    while(1)
    {
        rtn = get_msg_from_bus(CHANNEL_CONTROL, msg, 4096, NULL);
        if(rtn < 0)
        {
            break;
        }
        else if(rtn > 0)
        {
            printf("CHANNEL_CONTROL:\n%s", msg);
        }
    }
    
    rtn = msgbus_init(CHANNEL_BACKBONE, MSG_STATUS|BLOCK);
    if(rtn < 0)
    {
        printf("register CHANNEL_BACKBONE failed, exit\n");
        return -1;
    }
    while(1)
    {
        rtn = get_msg_from_bus(CHANNEL_BACKBONE, msg, 4096, NULL);
        if(rtn < 0)
        {
            break;
        }
        else if(rtn > 0)
        {
            printf("CHANNEL_BACKBONE:\n%s", msg);
        }
    }
    
    rtn = msgbus_init(CHANNEL_SYS, MSG_STATUS|BLOCK);
    if(rtn < 0)
    {
        printf("register CHANNEL_SYS failed, exit\n");
        return -1;
    }
    while(1)
    {
        rtn = get_msg_from_bus(CHANNEL_SYS, msg, 4096, NULL);
        if(rtn < 0)
        {
            break;
        }
        else if(rtn > 0)
        {
            printf("CHANNEL_SYS:\n%s", msg);
        }
    }
    
    rtn = msgbus_init(CHANNEL_DB_SS, MSG_STATUS|BLOCK);
    if(rtn < 0)
    {
        printf("register CHANNEL_DB_SS failed, exit\n");
        return -1;
    }
    while(1)
    {
        rtn = get_msg_from_bus(CHANNEL_DB_SS, msg, 4096, NULL);
        if(rtn < 0)
        {
            break;
        }
        else if(rtn > 0)
        {
            printf("CHANNEL_DB_SS:\n%s", msg);
        }
    }
    
    rtn = msgbus_init(CHANNEL_DB_WWW, MSG_STATUS|BLOCK);
    if(rtn < 0)
    {
        printf("register CHANNEL_DB_WWW failed, exit\n");
        return -1;
    }
    while(1)
    {
        rtn = get_msg_from_bus(CHANNEL_DB_WWW, msg, 4096, NULL);
        if(rtn < 0)
        {
            break;
        }
        else if(rtn > 0)
        {
            printf("CHANNEL_DB_WWW:\n%s", msg);
        }
    }
    return 0;
}
