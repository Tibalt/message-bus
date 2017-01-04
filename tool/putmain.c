/*test put interface of msg_bus lib, run as server*/
#include <stdio.h>
#include <sys/time.h>
#include "apue.h"
#include "msg_bus.h"
#include "channel_define.h"
#include "PointPacket.h"

double timeval_diff(struct timeval *end_time,
            struct timeval *start_time)
{
	double start = 0.0;
	double end = 0.0;
	start = (start_time->tv_sec * 1000.0) + (start_time->tv_usec / 1000.0);
	end = (end_time->tv_sec * 1000.0) + (end_time->tv_usec / 1000.0);
	return (end - start);
}
int main()
{
	int rtn = msgbus_init(255, MSG_PUT);
	if(rtn < 0)
	{
		printf("msgbus_init failed\n");
	}

	struct timeval t1, t2;
	struct timeval t3, t4;

	gettimeofday(&t3, NULL);
	int i = 1;
	while(1)
	{
		gettimeofday(&t1, NULL);

		IPCData data;
		data.key_id = 0xffff;
		data.timestamp.second = t1.tv_sec;
		data.timestamp.millisecond = t1.tv_usec/1000;
		data.target = 0;
		data.msg_type = FloatValue;
		data.status = 0;
		data.value.f= i;

		rtn = put_msg_to_bus(255, &data, sizeof(IPCData), HostToNetFunc);
		if(rtn < 0)
		{
			log_quit(LOG_ERR, "put_msg_to_bus error");
		}

		gettimeofday(&t2, NULL);

		printf("Done in: %.3f milliseconds\n", timeval_diff(&t2, &t1));

		i++;
		if(i == 100)
			break;
	}
	gettimeofday(&t4, NULL);
	printf("Done %d times total in: %.3f milliseconds, average: %.3f milliseconds\n", i, timeval_diff(&t4, &t3), timeval_diff(&t4, &t3)/i);

	while(1)
	{
		usleep(1000);
	}
	
    return 0;
}


