import json
import time
import socket
import binascii
import sys
import math
from struct import *

# the INDATA list to be tested
IN_HV_Current = 0
IN_LV_Current = 1
IN_TV_Current = 2
IN_Top_temp = 3
IN_Btm_temp =4
IN_Shadow_temp = 5
IN_Sun_temp = 6
IN_H2ppm = 10
IN_H2Oppm = 11
IN_H2O_temp = 12
IN_OLTC_H2Oppm = 13
IN_OLTC_H2O_temp = 14
IN_OLTC1_temp_high = 15
IN_OLTC1_temp_low = 16
IN_OLTC_pos = 23
IN_Voltage = 24
IN_Extra1 = 25
IN_C2H2ppm = 70
IN_COppm = 74
IN_CO2ppm = 78
IN_C2H4ppm = 82
IN_C2H6ppm = 86
IN_CH4ppm = 90
IN_O2ppm = 94
IN_TDCGppm = 98
IN_N2ppm = 102
IN_TDGppm = 106
IN_Cooler1_Current_high = 115
IN_Cooler1_Current_low = 116

# the BLOCKNUM list to be tested
OUT_HV_Current = 0
OUT_LV_Current = 1
OUT_TV_Current = 2
OUT_Top_temp = 3
OUT_Btm_temp = 4
OUT_TTB_Top_temp = 5
OUT_TTB_Btm_temp = 6
OUT_Shadow_temp = 7
OUT_Sun_temp = 8
OUT_H2ppm = 12
OUT_H2ppm1 = 13
OUT_H2ppm2 = 14
OUT_H2ppm3 = 15
OUT_H2Oppm = 16
OUT_H2O_temp = 17
OUT_OLTC_H2Oppm = 18
OUT_OLTC_H2O_temp = 19
OUT_OLTC1_temp = 20
OUT_TTB_OLTC1_temp = 28
OUT_Voltage = 32
OUT_Extra1 = 34
OUT_HV_HotSpot = 60
OUT_LV_HotSpot = 61
OUT_TV_HotSpot = 62
OUT_OLTC_pos = 63
OUT_Bubbling_temp = 79
OUT_H2Oppm1 = 80
OUT_H2Oppm2 = 81
OUT_H2Oppm3 = 82
OUT_C2H2ppm = 83
OUT_C2H2ppm1 = 84
OUT_C2H2ppm2 = 85
OUT_C2H2ppm3 = 86
OUT_COppm = 87
OUT_COppm1 = 88
OUT_COppm2 = 89
OUT_COppm3 = 90
OUT_CO2ppm = 91
OUT_CO2ppm1 = 92
OUT_CO2ppm2 = 93
OUT_CO2ppm3 = 94
OUT_C2H4ppm = 95
OUT_C2H4ppm1 = 96
OUT_C2H4ppm2 = 97
OUT_C2H4ppm3 = 98
OUT_C2H6ppm = 99
OUT_C2H6ppm1 = 100
OUT_C2H6ppm2 = 101
OUT_C2H6ppm3 = 102
OUT_CH4ppm = 103
OUT_CH4ppm1 = 104
OUT_CH4ppm2 = 105
OUT_CH4ppm3 = 106
OUT_O2ppm = 107
OUT_O2ppm1 = 108
OUT_O2ppm2 = 109
OUT_O2ppm3 = 110
OUT_TDCGppm = 111
OUT_TDCGppm1 = 112
OUT_TDCGppm2 = 113
OUT_TDCGppm3 = 114
OUT_N2ppm = 115
OUT_N2ppm1 = 116
OUT_N2ppm2 = 117
OUT_N2ppm3 = 118
OUT_TDGppm = 119
OUT_TDGppm1 = 120
OUT_TDGppm2 = 121
OUT_TDGppm3 = 122
OUT_Cooler1_Current = 129
OUT_Ageing = 256
OUT_Ageing1 = 257
OUT_Ageing2 = 258
OUT_Load = 259
OUT_Load1 = 260
OUT_Overload = 285
OUT_Overload1 = 340
OUT_Overload2 = 341
OUT_Overload3 = 342
OUT_Overload4 = 343


# the communication parameters
HOST = '192.168.200.158'
PORT = 10000
CHANNEL_61850 = 7
CHANNEL_TEC = 8
SUBSCRIBER = 1
PUBLISHER = 2
send_sockets = {}
receive_socket = None
channel_ids = []
channels = []
idxes = []
target_vals = []

#4 head(0xfefefefe) + 4 bytes length + 1 byte channel id + 1 byte type
def GetRegisterBytes(channel, mode):
    data = bytearray([0xfe, 0xfe, 0xfe, 0xfe])
    data.extend(pack('!I', 2)) 
    data.append( channel & 0xff ) #channel id
    data.append( mode & 0xff ) #subscriber
    return data

def init_socket(channel, mode):
    s = None
    recv_str = ''
    step = 0 
    length = 0 

    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.connect((HOST, PORT))
    except socket.error as msg:
        print "socket error happened:", msg 
        sys.exit(1)

    register_bytes = GetRegisterBytes(channel, mode)
    #print "register_bytes", binascii.hexlify(register_bytes)
    try:
        s.sendall(register_bytes)
    except socket.error as msg:
        print "socket error happened:", msg 
        sys.exit(1)

    try:
        while True:
            if step == 0: #read and verify head
                head = s.recv(4)
                head = unpack('BBBB', head)
                if head[0] == 0xfe and head[1] == 0xfe and head[2] == 0xfe and head[3] == 0xfe:
                    step = 1 
                    #print "head check passed"
            elif step == 1:#read length
                length = s.recv(4)
                length = unpack('!I', length)
                length = length[0]
                step = 3
                #print "length is:" , length
            elif step == 3:#read body
                temp_data = s.recv(length)
                recv_str += temp_data
                if len(temp_data) < length:
                    length = length - len(temp_data)
                else:#data finished
                    step = 4
                    #print "all data received"
            elif step == 4:
                recv_data = unpack('B', recv_str)
                if recv_data[0] == 0:
                    print "init msgbus %d successfully!" %(channel)
                    break
                else:
                    print "init msgbus %d failed, reaon code: %d" %(channel, recv_data[0])
                    sys.exit(1)

    except socket.error as msg:
        print "socket error happened:", msg
        sys.exit(1)
    
    return s

def msg_init():
    global send_sockets
    global receive_socket
    global channels

    for ch in channels:
        s = init_socket(ch, PUBLISHER)
        send_sockets[ch] = s

    receive_socket = init_socket(CHANNEL_TEC, SUBSCRIBER)

#4 bytes head(0xfefefefe) + 4 bytes length + 4 bytes channel_id + 4 bytes value
def GetSendBytes(channel_id, value):
    data = bytearray([0xfe, 0xfe, 0xfe, 0xfe])
    data.extend(pack('!I', 16))
    #print hex(channel_id)
    data.extend(pack('!I', channel_id))
    data.extend(pack('!f', value))
    channel_id = ((channel_id & 0xf0ffffff) | 0x03000000)
    #print hex(channel_id)
    data.extend(pack('!I', channel_id))
    data.extend(pack('!i', 0))
    return data

def send_value(idx, value):
    global send_sockets
    global channel_ids
    send_bytes = GetSendBytes(channel_ids[idx], value)
    try:
        channel = channel_ids[idx] >> 28
        send_sockets[channel].sendall(send_bytes)
    except socket.error as msg:
        print "socket error happened:", msg
        sys.exit(1)

def send_values(idxes, target_values):
    return

def receive_calculates(idxes):
    global receive_socket
    recv_str = ''
    step = 0
    length = 0
    passed = False

    last_vals = []
    for x in range(0, len(idxes)):
        last_vals.append(None)

    try:
        while not passed:
            if step == 0: #read and verify head
                head = receive_socket.recv(4)
                head = unpack('BBBB', head)
                if head[0] == 0xfe and head[1] == 0xfe and head[2] == 0xfe and head[3] == 0xfe:
                    step = 1 
            elif step == 1:#read length
                length = receive_socket.recv(4)
                length = unpack('!I', length)
                length = length[0]
                step = 3
            elif step == 3:#read body
                temp_data = receive_socket.recv(length)
                recv_str += temp_data
                if len(temp_data) < length:
                    length = length - len(temp_data)
                else:#data finished
                    step = 4
            elif step == 4:
                if len(recv_str) % 8 != 0:
                    pass
                else:
                    ele_num = len(recv_str) / 8
                    for x in range(0, ele_num):
                        blocknumber = unpack('!I', recv_str[:4])[0]
                        recv_str = recv_str[4:]

                        type = (blocknumber >> 24) & 0xf
                        blocknumber = blocknumber & 0xffff
                        for y in range(0, len(idxes)):
                            if blocknumber == idxes[y]:
                                if type == 1:
                                    val = unpack('!f', recv_str[:4])[0]
                                    if True:
                                        if math.isnan(val) or math.isinf(val):
                                            print "blocknumber %d got Nan/Inf/-Inf value" % (idxes[y])
                                            del idxes[y]
                                            del last_vals[y]
                                            break
                                        elif last_vals[y] == None:
                                            last_vals[y] = val
                                        else:
                                            if isclose(last_vals[y], val, 1e-3):
                                                print "blocknumber %d reach limited value %e" % (idxes[y], val)
                                                del idxes[y]
                                                del last_vals[y]
                                                break
                                            else:
                                                last_vals[y] = val

                                elif type == 8:
                                    status = unpack('!I', recv_str[:4])[0]
                                    if status != 0:
                                        print "blocknumber %d status changed to %d" %(idxes[y], status)
                                        del idxes[y]
                                        del last_vals[y]
                                        break

                        recv_str = recv_str[4:]
                        if len(idxes) == 0:
                            passed = True

                step = 0
                recv_str = ''
                length = 0

    except socket.error as msg:
        print "socket error happened:", msg
        sys.exit(1)

def receive_value(idxes, target_values):
    global receive_socket
    recv_str = ''
    step = 0
    length = 0
    passed = False

    diff_vals = []
    for x in range(0, len(idxes)):
        diff_vals.append(None)

    try:
        while not passed:
            #print idxes
            if step == 0: #read and verify head
                head = receive_socket.recv(4)
                head = unpack('BBBB', head)
                if head[0] == 0xfe and head[1] == 0xfe and head[2] == 0xfe and head[3] == 0xfe:
                    step = 1 
            elif step == 1:#read length
                length = receive_socket.recv(4)
                length = unpack('!I', length)
                length = length[0]
                step = 3
            elif step == 3:#read body
                temp_data = receive_socket.recv(length)
                recv_str += temp_data
                if len(temp_data) < length:
                    length = length - len(temp_data)
                else:#data finished
                    step = 4
                    #print "all data received"
            elif step == 4:
                #4 bytes blocknumber + 4 bytes value
                #print "length of str", len(recv_str)
                if len(recv_str) % 8 != 0:
                    #print "not a valid IPC package"
                    pass
                else:
                    ele_num = len(recv_str) / 8
                    for x in range(0, ele_num):
                        blocknumber = unpack('!I', recv_str[:4])[0]
                        recv_str = recv_str[4:]

                        type = (blocknumber >> 24) & 0xf
                        blocknumber = blocknumber & 0xffff
                        for y in range(0, len(idxes)):
                            if blocknumber == idxes[y]:
                                if type == 1:
                                    val = unpack('!f', recv_str[:4])[0]
                                    if isclose(val, target_values[y], 1e-03):
                                        print "blocknumber %d passes test" % (idxes[y])
                                        del idxes[y]
                                        del target_values[y]
                                        del diff_vals[y]
                                        break
                                    else:
                                            
                                        if diff_vals[y] == None:
                                            diff_vals[y] = abs(val) - abs(target_values[y])
                                        else:
                                            if isclose(diff_vals[y], abs(val) - abs(target_values[y]), 1e-06):
                                                print "blocknumber %d failes test with value %e" % (idxes[y], val)
                                                del idxes[y]
                                                del target_values[y]
                                                del diff_vals[y]
                                                break
                                            else:
                                                diff_vals[y] = abs(val) - abs(target_values[y])

                                elif type == 8:
                                    status = unpack('!I', recv_str[:4])[0]
                                    if status != 0:
                                        print "blocknumber %d test failed, status changed to %d" %(idxes[y], status)
                                        del idxes[y]
                                        del target_values[y]
                                        del diff_vals[y]
                                        break

                        recv_str = recv_str[4:]
                        if len(idxes) == 0:
                            passed = True

                step = 0
                recv_str = ''
                length = 0

    except socket.error as msg:
        print "socket error happened:", msg
        sys.exit(1)



def isclose(a, b, rel_tol=1e-09, abs_tol=0.0):
    return abs(a-b) <= max(rel_tol * max(abs(a), abs(b)), abs_tol)

def add_idx(idx, value):
    global idxes
    global target_vals
    idxes.append(idx)
    target_vals.append(value)

def add_idx_only(idx):
    global idxes
    idxes.append(idx)

def test():
    global idxes
    global target_vals
    FLOAT_MAX = 3.402823e+38
    FLOAT_MIN = -3.402823e38

    
    print "1st, test the maximum values"
    idxes = []
    target_vals = []
    target_val = FLOAT_MAX

    send_value(IN_Top_temp, target_val)
    time.sleep(0.5)
    send_value(IN_Btm_temp, target_val)
    time.sleep(0.5)
    send_value(IN_LV_Current, target_val)
    time.sleep(0.5)
    send_value(IN_Shadow_temp, target_val)
    time.sleep(0.5)
    send_value(IN_Sun_temp, target_val)
    time.sleep(0.5)
    send_value(IN_H2ppm, target_val)
    time.sleep(0.5)
    send_value(IN_H2Oppm, target_val)
    time.sleep(0.5)
    send_value(IN_OLTC_H2Oppm, target_val)
    time.sleep(0.5)
    send_value(IN_OLTC1_temp_high, target_val)
    time.sleep(0.5)
    send_value(IN_OLTC_pos, target_val)
    time.sleep(0.5)
    send_value(IN_Voltage, target_val)
    time.sleep(0.5)
    send_value(IN_Extra1, target_val)
    time.sleep(0.5)
    send_value(IN_C2H2ppm, target_val)
    time.sleep(0.5)
    send_value(IN_COppm, target_val)
    time.sleep(0.5)
    send_value(IN_CO2ppm, target_val)
    time.sleep(0.5)
    send_value(IN_C2H4ppm, target_val)
    time.sleep(0.5)
    send_value(IN_C2H6ppm, target_val)
    time.sleep(0.5)
    send_value(IN_CH4ppm, target_val)
    time.sleep(0.5)
    send_value(IN_O2ppm, target_val)
    time.sleep(0.5)
    send_value(IN_TDCGppm, target_val)
    time.sleep(0.5)
    send_value(IN_N2ppm, target_val)
    time.sleep(0.5)
    send_value(IN_TDGppm, target_val)
    time.sleep(0.5)
    send_value(IN_Cooler1_Current_high, target_val)

    add_idx(OUT_Top_temp, target_val)
    add_idx(OUT_Btm_temp, target_val)
    add_idx(OUT_LV_Current, target_val)
    add_idx(OUT_Shadow_temp, target_val)
    add_idx(OUT_Sun_temp, target_val)
    add_idx(OUT_H2ppm, target_val)
    add_idx(OUT_H2Oppm, target_val)
    add_idx(OUT_OLTC_H2Oppm, target_val)
    add_idx(OUT_OLTC1_temp, target_val)
    add_idx(OUT_OLTC_pos, target_val)
    add_idx(OUT_Voltage, target_val)
    add_idx(OUT_Extra1, target_val)
    add_idx(OUT_C2H2ppm, target_val)
    add_idx(OUT_COppm, target_val)
    add_idx(OUT_CO2ppm, target_val)
    add_idx(OUT_C2H4ppm, target_val)
    add_idx(OUT_C2H6ppm, target_val)
    add_idx(OUT_CH4ppm, target_val)
    add_idx(OUT_O2ppm, target_val)
    add_idx(OUT_TDCGppm, target_val)
    add_idx(OUT_N2ppm, target_val)
    add_idx(OUT_TDGppm, target_val)
    add_idx(OUT_Cooler1_Current, target_val)

    receive_value(idxes, target_vals)

    idxes = []

    add_idx_only(OUT_HV_Current)
    add_idx_only(OUT_TTB_Top_temp)
    add_idx_only(OUT_TTB_Btm_temp)
    #add_idx_only(OUT_H2ppm1)
    add_idx_only(OUT_H2ppm2)
    add_idx_only(OUT_H2ppm3)
    add_idx_only(OUT_TTB_OLTC1_temp)
    add_idx_only(OUT_HV_HotSpot)
    add_idx_only(OUT_LV_HotSpot)
    #add_idx_only(OUT_Bubbling_temp)
    '''
    add_idx_only(OUT_H2Oppm1)
    add_idx_only(OUT_H2Oppm2)
    add_idx_only(OUT_H2Oppm3)
    add_idx_only(OUT_C2H2ppm1)
    add_idx_only(OUT_C2H2ppm2)
    add_idx_only(OUT_C2H2ppm3)
    add_idx_only(OUT_COppm1)
    add_idx_only(OUT_COppm2)
    add_idx_only(OUT_COppm3)
    add_idx_only(OUT_CO2ppm1)
    add_idx_only(OUT_CO2ppm2)
    add_idx_only(OUT_CO2ppm3)
    add_idx_only(OUT_C2H4ppm1)
    add_idx_only(OUT_C2H4ppm2)
    add_idx_only(OUT_C2H4ppm3)
    add_idx_only(OUT_C2H6ppm1)
    add_idx_only(OUT_C2H6ppm2)
    add_idx_only(OUT_C2H6ppm3)
    add_idx_only(OUT_CH4ppm1)
    add_idx_only(OUT_CH4ppm2)
    add_idx_only(OUT_CH4ppm3)
    add_idx_only(OUT_O2ppm1)
    add_idx_only(OUT_O2ppm2)
    add_idx_only(OUT_O2ppm3)
    add_idx_only(OUT_TDCGppm1)
    add_idx_only(OUT_TDCGppm2)
    add_idx_only(OUT_TDCGppm3)
    add_idx_only(OUT_N2ppm1)
    add_idx_only(OUT_N2ppm2)
    add_idx_only(OUT_N2ppm3)
    add_idx_only(OUT_TDGppm1)
    add_idx_only(OUT_TDGppm2)
    add_idx_only(OUT_TDGppm3)
    '''
    add_idx_only(OUT_Ageing)
    add_idx_only(OUT_Ageing1)
    add_idx_only(OUT_Ageing2)
    #add_idx_only(OUT_Load)
    add_idx_only(OUT_Load1)
    #add_idx_only(OUT_Overload)
    add_idx_only(OUT_Overload1)
    add_idx_only(OUT_Overload2)
    add_idx_only(OUT_Overload3)
    add_idx_only(OUT_Overload4)
    
    receive_calculates(idxes)
    
    print "2nd, test the minimum values"
    idxes = []
    target_vals = []
    target_val = FLOAT_MIN

    send_value(IN_Top_temp, target_val)
    time.sleep(0.5)
    send_value(IN_Btm_temp, target_val)
    time.sleep(0.5)
    send_value(IN_LV_Current, target_val)
    time.sleep(0.5)
    send_value(IN_Shadow_temp, target_val)
    time.sleep(0.5)
    send_value(IN_Sun_temp, target_val)
    time.sleep(0.5)
    send_value(IN_H2ppm, target_val)
    time.sleep(0.5)
    send_value(IN_H2Oppm, target_val)
    time.sleep(0.5)
    send_value(IN_OLTC_H2Oppm, target_val)
    time.sleep(0.5)
    send_value(IN_OLTC1_temp_high, target_val)
    time.sleep(0.5)
    send_value(IN_OLTC_pos, target_val)
    time.sleep(0.5)
    send_value(IN_Voltage, target_val)
    time.sleep(0.5)
    send_value(IN_Extra1, target_val)
    time.sleep(0.5)
    send_value(IN_C2H2ppm, target_val)
    time.sleep(0.5)
    send_value(IN_COppm, target_val)
    time.sleep(0.5)
    send_value(IN_CO2ppm, target_val)
    time.sleep(0.5)
    send_value(IN_C2H4ppm, target_val)
    time.sleep(0.5)
    send_value(IN_C2H6ppm, target_val)
    time.sleep(0.5)
    send_value(IN_CH4ppm, target_val)
    time.sleep(0.5)
    send_value(IN_O2ppm, target_val)
    time.sleep(0.5)
    send_value(IN_TDCGppm, target_val)
    time.sleep(0.5)
    send_value(IN_N2ppm, target_val)
    time.sleep(0.5)
    send_value(IN_TDGppm, target_val)
    time.sleep(0.5)
    send_value(IN_Cooler1_Current_high, target_val)

    add_idx(OUT_Top_temp, target_val)
    add_idx(OUT_Btm_temp, target_val)
    add_idx(OUT_LV_Current, target_val)
    add_idx(OUT_Shadow_temp, target_val)
    add_idx(OUT_Sun_temp, target_val)
    add_idx(OUT_H2ppm, target_val)
    add_idx(OUT_H2Oppm, target_val)
    add_idx(OUT_OLTC_H2Oppm, target_val)
    add_idx(OUT_OLTC1_temp, target_val)
    add_idx(OUT_OLTC_pos, target_val)
    add_idx(OUT_Voltage, target_val)
    add_idx(OUT_Extra1, target_val)
    add_idx(OUT_C2H2ppm, target_val)
    add_idx(OUT_COppm, target_val)
    add_idx(OUT_CO2ppm, target_val)
    add_idx(OUT_C2H4ppm, target_val)
    add_idx(OUT_C2H6ppm, target_val)
    add_idx(OUT_CH4ppm, target_val)
    add_idx(OUT_O2ppm, target_val)
    add_idx(OUT_TDCGppm, target_val)
    add_idx(OUT_N2ppm, target_val)
    add_idx(OUT_TDGppm, target_val)
    add_idx(OUT_Cooler1_Current, target_val)

    receive_value(idxes, target_vals)

    idxes = []

    add_idx_only(OUT_HV_Current)
    add_idx_only(OUT_TTB_Top_temp)
    add_idx_only(OUT_TTB_Btm_temp)
    #add_idx_only(OUT_H2ppm1)
    add_idx_only(OUT_H2ppm2)
    add_idx_only(OUT_H2ppm3)
    add_idx_only(OUT_TTB_OLTC1_temp)
    add_idx_only(OUT_HV_HotSpot)
    add_idx_only(OUT_LV_HotSpot)

    add_idx_only(OUT_Ageing)
    add_idx_only(OUT_Ageing1)
    add_idx_only(OUT_Ageing2)
    #add_idx_only(OUT_Load)
    add_idx_only(OUT_Load1)
    #add_idx_only(OUT_Overload)
    add_idx_only(OUT_Overload1)
    add_idx_only(OUT_Overload2)
    add_idx_only(OUT_Overload3)
    add_idx_only(OUT_Overload4)
    
    receive_calculates(idxes)
   

def loadKeysFromPara():
    global channel_ids
    global channels
    ANA_FILE="./analog_para"
    
    with open(ANA_FILE,'r') as f:
        for line in f:
            channel_ids.append(int(line.split()[0], 16))
            if int(line.split()[0], 16) >> 28 != 0:
                channels.append(int(line.split()[0], 16) >> 28)
    channels = list(set(channels))
    #print channels

def main():
    loadKeysFromPara()
    msg_init()
    test()


if __name__ == "__main__":
    main()
