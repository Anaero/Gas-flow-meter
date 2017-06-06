# -*- coding: utf-8 -*-
"""
Bittern build
"""
import serial
import serial.tools.list_ports
import time
import msvcrt
from sys import exit
import datetime
print()
print(">>> **********************************************************************************")
print(">>> anaero technology - www.anaero.co.uk - gas flow monitor startup utility")
print(">>>")
print(">>> serial ports being used on this machine are:",'\n')
ports = list(serial.tools.list_ports.comports())
for p in ports:
    print ("   ",p,'\n')
print(">>> enter the name of the COM port the Arduino Mega 2560 is connected to")
print(">>> e.g. COM1 or COM2 or COM3 or COM4 or similar, and then press enter",'\n')
var = input(">>> ")
time.sleep(1)
print(">>> opening serial port connection to arduino")
ser = serial.Serial()
try:
    ser = serial.Serial(var, 57600, timeout = 180)
except serial.serialutil.SerialException:
    print(">>> Arduino not connected - please check COM port and try again")
    print(">>> press any key to exit program")
    done = False
    while not done:
        if msvcrt.kbhit():
            line = msvcrt.getch()        
            print (">>> keyboard input detected - quitting monitor")
            done = True
            exit(0) # Successful exit  
#
# utility terminates if specified COM port cannot be opened
#
time.sleep(0.1)
line = datetime.datetime.strftime(datetime.datetime.now(), '%Y-%m-%d %H:%M:%S')
print('\n',line,'\n')

time.sleep(1)       # give Arduino time to start up
ser.write(b'1000')  # send data to move on from Arduino reset message
                    # 1000 designates normal startup
time.sleep(0.1)
line = ser.readline()            
while (line != b'*** Do you want to save existing log files to your computer before continuing? ***'):
    line = ser.readline()
    line = line.rstrip()
    print(line.decode())
#    
#
#
print()
print("To save the existing log files from SD card on your computer before new run starts press Enter now...")
print("")
print("OR")
print("")
print("To delete existing log files on SD card and start new run, press d followed by the Enter key... ")
print("")
var = input(">>>> ")
if (var == "D" or var == "d"):
    ser.write(b'N')  
else: 
    ser.write(b'Y')     
    while (line != b'starting eventlog.csv writeback'):
        line = ser.readline()
        line = line.rstrip()
        print(line.decode())  
    f1 = open('eventlog.csv', 'wb')     # open file for binary write (append)
    f1.write(b'file uploaded: ')
    line = datetime.datetime.strftime(datetime.datetime.now(), '%Y-%m-%d %H:%M:%S')
    f1.write(line.encode())
    f1.write(b',\n')    
    while (line != b'writeback completed - eventlog.csv closed'):
        line = ser.readline()
        f1.write(line)
        line = line.rstrip()
        print(line.decode())
    f1.close()
#
    while (line != b'starting snapshot.csv writeback'):
        line = ser.readline()
        line = line.rstrip()
        print(line.decode())
    f2 = open('snapshot.csv', 'wb')   # open file for binary write (append)
    f2.write(b'file uploaded: ')
    line = datetime.datetime.strftime(datetime.datetime.now(), '%Y-%m-%d %H:%M:%S')
    f2.write(line.encode())
    f2.write(b',\n')    
    while (line != b'writeback completed - snapshot.csv closed'):
        line = ser.readline()
        f2.write(line)
        line = line.rstrip()
        print(line.decode())    
    f2.close()
#
    while (line != b'starting daily.csv writeback'):
        line = ser.readline()
        line = line.rstrip()
        print(line.decode())
    f3 = open('daily.csv', 'wb')   # open file for binary write (append)
    f3.write(b'file uploaded: ')
    line = datetime.datetime.strftime(datetime.datetime.now(), '%Y-%m-%d %H:%M:%S')
    f3.write(line.encode())
    f3.write(b',\n')
    while (line != b'writeback completed - daily.csv closed'):
        line = ser.readline()
        f3.write(line)
        line = line.rstrip()
        print(line.decode())    
    f3.close()
#
    while (line != b'starting hourly.csv writeback'):
        line = ser.readline()
        line = line.rstrip()
        print(line.decode())
    f4 = open('hourly.csv', 'wb')   # open file for binary write (append)
    f4.write(b'file uploaded: ')
    line = datetime.datetime.strftime(datetime.datetime.now(), '%Y-%m-%d %H:%M:%S')
    f4.write(line.encode())
    f4.write(b',\n')
    while (line != b'writeback completed - hourly.csv closed'):
        line = ser.readline()
        f4.write(line)
        line = line.rstrip()
        print(line.decode())    
    f4.close()
#                        
while (line != b'Power on self test complete'):
    line = ser.readline()
    line = line.rstrip()
    print(line.decode())
print()
time.sleep(0.5)
while (line != b'Press the enter key to start transfer of setup.csv file to Arduino'):
    line = ser.readline()
    line = line.rstrip()
    print(line.decode())      
print()
var = input(">>>> ")
time.sleep(0.1)
print()
done = False
try:
    f1 = open('setup.csv', 'r')
except IOError as e:
    print(">>> file setup.csv not found in current directory")
    print(">>> please check that a valid setup.csv file exists in the same directory as this utility")
    print(">>> and/or is not currently open in another program such as Excel or Notepad")
    print(">>> press any key to exit this utility")
    while not done:
        if msvcrt.kbhit():
#        print ("you pressed",msvcrt.getch(),"so now i will quit")
            line = msvcrt.getch()        
            print (">>> keyboard input detected - quitting startrun")
            done = True
            ser.close()
            exit(0) # Successful exit  
line = f1.readline() # read in line 1 from csv to strip off col headings
for num in range(1,17):
    line = f1.readline()
    line = line.rstrip()
    print(line)
    time.sleep(0.1)
    line = line + "eol^"         # add end of line termintor - discarded by Arduino
    time.sleep(0.1)
    ser.write(line.encode())
    time.sleep(0.1)              # guard time for serial write to complete before printing to screen
f1.close()                       # close setup.csv when all 15 lines written

while (line != b'starting setup.csv writeback'):
    line = ser.readline()
    line = line.rstrip()
    print(line.decode())
f1 = open('setup.csv', 'ab')   # open file for binary write (append)
f1.write(b'file uploaded: ')
line = datetime.datetime.strftime(datetime.datetime.now(), '%Y-%m-%d %H:%M:%S')
f1.write(line.encode())
f1.write(b',\n')    
while (line != b'writeback completed - setup.csv closed'):
    line = ser.readline()
    f1.write(line)
    line = line.rstrip()
    print(line.decode())
f1.close()
#
while (line != b'Press the enter key to start data logging'):
    line = ser.readline()
    line = line.rstrip()
    print(line.decode())
time.sleep(0.1)
print()
var = input(">>>> ")
print()
time.sleep(0.1)
#
# all ready to go now
# 
ser.write(b'X')   # trigger Arduino by sending character over serial port 

done = False
while not done:
    line = ser.readline()
    line = line.rstrip()
    if (line != b'ping'):
        print(line.decode())
    if msvcrt.kbhit():
#        print ("you pressed",msvcrt.getch(),"so now i will quit")
        line = msvcrt.getch()        
        print ("keyboard input detected - quitting startup program")
        done = True
ser.close()
print("Bye")
time.sleep(2)
exit(0) # Successful exit