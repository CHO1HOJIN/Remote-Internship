#!/usr/bin/env python

from pymavlink import mavutil
from pymavlink import mavwp
import time
import threading
import string
import sys

# Global variables
target_roll = 1500
target_pitch = 1500
target_yaw = 1500
target_throttle = 1500
target_rc5 = 1500

current_heading = 0
start_heading = 0
target_heading = 0

right_start_time = 0
right_turn_phase = 0
left_start_time = 0
left_turn_phase = 0

forward_start_time = 0
forward_flag = 0


# python /usr/local/bin/mavproxy.py --mav=/dev/tty.usbserial-DN01WM7R --baudrate 57600 --out udp:127.0.0.1:14540 --out udp:127.0.0.1:14550
master = mavutil.mavlink_connection('udp:127.0.0.1:14551')
#------------------------------------------------------------------------------------
def set_rc_channel_pwm(id, pwm=1500):
    """ Set RC channel pwm value
    Args:
        id (TYPE): Channel ID
        pwm (int, optional): Channel pwm value 1100-1900
    """
    if id < 1:
        print("Channel does not exist.")
        return

    # We only have 8 channels
    # https://mavlink.io/en/messages/common.html#RC_CHANNELS_OVERRIDE
    if id < 9:
        rc_channel_values = [65535 for _ in range(8)]
        rc_channel_values[id - 1] = pwm

        # global master

        master.mav.rc_channels_override_send(
            master.target_system,  # target_system
            master.target_component,  # target_component
            *rc_channel_values)  # RC channel list, in microseconds.

#------------------------------------------------------------------------------------
def throttle_th():
    global target_roll
    global target_pitch
    global target_yaw
    global target_throttle

    while True:
        set_rc_channel_pwm(1, target_roll)
        set_rc_channel_pwm(2, target_pitch)
        set_rc_channel_pwm(3, target_throttle)
        set_rc_channel_pwm(4, target_yaw)
        time.sleep(0.1)

# ------------------------------------------------------------------------------------
def handle_hud(msg):

    hud_data = (msg.airspeed, msg.groundspeed, msg.heading,
                msg.throttle, msg.alt, msg.climb)
    # print "Aspd\tGspd\tHead\tThro\tAlt\tClimb"
    # print "%0.2f\t%0.2f\t%0.2f\t%0.2f\t%0.2f\t%0.2f" % hud_data

    # print("Alt: %f" %msg.alt)

    global current_heading
    global start_heading
    global target_heading
    global target_roll
    global right_turn_phase
    global left_turn_phase
    global target_throttle
    global target
    global forward_throttle
    global right_start_time
    global left_start_time
    global forward_start_time
    global forward_flag

    current_time = time.time()

    if  forward_start_time !=0 and current_time - forward_start_time > 5 and forward_flag == 1:
        while(target_throttle > 1500):
            target_throttle -= 20
            time.sleep(0.1)
        target_throttle = 1500
        forward_flag = 0
    
    elif  right_start_time !=0 and right_turn_phase == 1 and current_time - right_start_time > 3:
        while(target_throttle > 1550):
            target_throttle -= 20
            time.sleep(0.1)
        target_throttle = 1550
        right_turn_phase = 2
        current_heading = msg.heading

    elif right_turn_phase == 2:
        start_heading = current_heading
        target_heading = (start_heading + 90) % 360
        target_roll = 1600
        if(abs(msg.heading - target_heading) < 5):
            target_roll = 1500
            right_turn_phase = 3
            right_start_time = time.time()
            target_throttle = forward_throttle

    elif right_turn_phase == 3 and current_time - right_start_time > 3:
        while(target_throttle > 1500):
            target_throttle -= 20
            time.sleep(0.1)
        target_throttle = 1500
        right_turn_phase = 0

    elif left_start_time !=0 and left_turn_phase == 1 and current_time - left_start_time > 3:
        while(target_throttle > 1550):
            target_throttle -= 20
            time.sleep(0.1)
        target_throttle = 1550
        left_turn_phase = 2
        current_heading = msg.heading

    elif left_turn_phase == 2:
        start_heading = current_heading
        target_heading = (start_heading - 90) % 360
        target_roll = 1400
        if(abs(msg.heading - target_heading) < 5):
            target_roll = 1500
            left_turn_phase = 3
            left_start_time = time.time()
            ttarget_throttle = forward_throttle

    if left_turn_phase == 3 and current_time - left_start_time > 3:
        while(target_throttle > 1500):
            target_throttle -= 20
            time.sleep(0.1)
        target_throttle = 1500
        left_turn_phase = 0


    # [Implement your code at here]

# ------------------------------------------------------------------------------------
def read_loop():
    while True:
        # grab a mavlink message
        msg = master.recv_match(blocking=True)

        # handle the message based on its type
        msg_type = msg.get_type()

        if msg_type == "VFR_HUD":
            handle_hud(msg)

#------------------------------------------------------------------------------------
def main(argv):
    global target_roll
    global target_pitch
    global target_yaw
    global target_throttle
    global target_rc5

    global current_heading
    global start_heading
    global target_heading

    global right_turn_phase
    global left_turn_phase
    global target

    global forward_throttle 
    global forward_start_time
    global right_start_time
    global left_start_time
    global forward_flag

    print(sys.version)

    master.wait_heartbeat()
    print("HEARTBEAT OK\n")

    goal_throttle = 1500
    
    forward_throttle = 1600

    t = threading.Thread(target=throttle_th, args=())
    t.daemon = True
    t.start()

    t2 = threading.Thread(target=read_loop, args=())
    t2.daemon = True
    t2.start()
    
    target = ""

    while True:
        
        # request data to be sent at the given rate
        for i in range(0, 3):
            master.mav.request_data_stream_send(master.target_system, master.target_component,
                                                mavutil.mavlink.MAV_DATA_STREAM_ALL, 30, 1)
            
        if target == "":
            target = raw_input("[forward/stop/right/left] ")
            print('%s' %target)

        if target == "forward":
            target_throttle = forward_throttle
            forward_start_time = time.time()
            forward_flag = 1
            target = ""


        elif target == "stop":
            for i in range(target_throttle, 1500, -20):
                target_throttle = i
                time.sleep(0.1)
            target_throttle = 1500
            target_roll = 1500
            left_turn_phase = 0
            right_turn_phase = 0
            forward_flag = 0
            target = ""
        
        elif target == "right" and right_turn_phase == 0:
            right_start_time = time.time()
            target_throttle = forward_throttle
            right_turn_phase = 1
            target = ""

        elif target == "left" and left_turn_phase == 0:
            left_start_time = time.time()
            target_throttle = forward_throttle
            left_turn_phase = 1
            target = ""

        #     # [Implement your code at here]

        else:
            target = ""

        time.sleep(0.1)

if __name__ == "__main__":
   main(sys.argv[1:])