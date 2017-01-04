#include "msg_bus.h"
#include "apue.h"
#include "channel_define.h"



int main(){
    log_open("init_msgbus");
    log_msg(LOG_INFO,"test init msg bus");
    int rtn = msgbus_init(CHANNEL_SS61850S,MSG_PUT|MSG_GET|LOOP);

    char msg[50] = "hello daemon!";
    if(rtn == 0){
		print_elapsed_time();printf("before put\n");
        rtn = put_msg_to_bus(CHANNEL_SS61850S,msg,strlen(msg)+1,NULL);
		print_elapsed_time();printf("after put\n");
	}

    sleep(1);

    log_msg(LOG_INFO,"put_msg_to_bus, return %d",rtn);
    char msg1[50]={};
    if(rtn == 0){
        do{
            rtn = get_msg_from_bus(CHANNEL_SS61850S,msg1,sizeof msg1/sizeof(char),NULL);
            log_msg(LOG_INFO,"get_msg_from_bus, return %d",rtn);
            if(rtn >0) log_msg(LOG_INFO,"msg is \"%s\"",msg1);
            sleep(1);
        }while(1);
    }


    return rtn;
}
