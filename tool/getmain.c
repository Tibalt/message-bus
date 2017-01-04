/*test get msg interface of msg_bus lib, run as client*/
#include <netinet/in.h> 
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include "apue.h"
#include "fdfactory.h"
#include "msg_bus.h"
#include "PointPacket.h"
IPCData Data;
int main()
{
	int rtn = msgbus_init(255, MSG_GET| LOOP /*| NO_HISTORY*/);
	if(rtn < 0)
		printf("msg_init error\n");
	while(1)
	{
		rtn = get_pointPacket(255, &Data, sizeof(IPCData));
		if(rtn > 0)
		{
			printf("time: %d,%d\n", Data.timestamp.second, Data.timestamp.millisecond );
			printf("value: %f\n", Data.value.f);
		}
		usleep(1);
	}
	return 0;
}
