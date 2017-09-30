import serial
import time
import threading
ser = serial.Serial('/dev/ttyACM0', 115200, timeout = 0.050)
req = '{T: "CMD",F: "SENS",D: ""}'
time_read = 60
def setInterval(func,time):
    e = threading.Event()
    while not e.wait(time):
        func()

def foo():
    ser.write(req)
    print req
    time.sleep(10)
    a= ""
    while ser.inWaiting():
        a= a+ ser.readline()
    print a

# using
setInterval(foo,time_read)
