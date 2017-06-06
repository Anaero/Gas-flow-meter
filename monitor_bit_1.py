# -*- coding: utf-8 -*-
"""
Bittern build
"""

import serial
import serial.tools.list_ports
import time
import msvcrt
from sys import exit
print()
print(">>> **********************************************************************************")
print(">>> anaero technology - www.anaero.co.uk - simple monitor utility")
print(">>> this utility provides a simple monitor to allow observation of output from Arduino")
print(">>> serial ports being used on this machine are:",'\n')
ports = list(serial.tools.list_ports.comports())
for p in ports:
    print ("   ",p,'\n')
print(">>> enter the name of the COM port the Arduino Mega 2560 is connected to")
print(">>> e.g. COM1 or COM2 or COM3 or COM4 or similar, and then press enter",'\n')
var = input(">>> ")
time.sleep(1)
print(">>> configuring serial port")
ser = serial.Serial()
ser.baudrate = 57600
ser.port = var
ser.timeout = 180
ser.setDTR(False)
print(">>> port configured")
#ser.open()
try:
    ser.open()
except serial.serialutil.SerialException:
    print(">>> Arduino not connected - please check COM port and try again")
    print(">>> press any key to exit program")
    done = False
    while not done:
        if msvcrt.kbhit():
#        print ("you pressed",msvcrt.getch(),"so now i will quit")
            line = msvcrt.getch()        
            print (">>> keyboard input detected - quitting monitor")
            done = True
            exit(0) # Successful exit  
time.sleep(1.0)
print(">>> serial connection established")
line = ser.readline()
line = line.rstrip()
if(line == b'Arduino has reset'):
    print("!!!! Arduino has reset")
    print("!!!! to start a new run, use the startrun utility")
    print("!!!! to resume data logging of existing run, use the re-start utility",'\n')
    exit(0) # Successful exit  
print()    
print(">>> requesting connection to Arduino - this may take a short while",'\n')
line = ser.readline()
line = line.rstrip()
#print(line) 
while (line != b'ping'):      # wait for ping to be received then send request
    line = ser.readline()
    line = line.rstrip()
#    print(line)
time.sleep(0.01)              # wait before responding to   
ser.write(b'7777x')  # initiates snapshot then Arduino reverts to business as usual 
print(">>> waiting for response",'\n')
while (line != b'Connection request acknowledged....'):
    line = ser.readline()
    line = line.rstrip()
#    print(line)
#print(line)   diagnoistic
print(">>> request acknowledged....",'\n') 
print(">>> monitor started - preparing snapshot....", '\n')   
done = False
while not done:
    line = ser.readline()
    line = line.rstrip()
    if (line != b'ping'):
        print('\t',line.decode())
    if msvcrt.kbhit():
#        print ("you pressed",msvcrt.getch(),"so now i will quit")
        line = msvcrt.getch()        
        print (">>> keyboard input detected - quitting monitor")
        done = True
ser.close()
print(">>> bye")
time.sleep(2)
exit(0) # Successful exit