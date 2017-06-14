# -*- coding: utf-8 -*-
"""
Bittern build

filegrab_bit_2 changes from filegrab_bit_1

finds current working pathname and creates /data/ subdirectory if it doesn't already exist
all data files then written to /data/ subdirectory 

"""

import serial
import serial.tools.list_ports
import time
import msvcrt
import datetime
from sys import exit
import os,sys
from pathlib import Path
ser = serial.Serial()
#
# Find pathname for current directory
#
dirname=os.path.dirname(os.path.realpath(sys.argv[0]))
#
# Extend path to include /data/ sub-directory
#
dirname = dirname + '/data/'
#
# Convert string version of extended pathname to list (Python strings are immutable)
#
newname = list(dirname)
for i, c in enumerate(dirname):
#    print(i, c, newname[i])
    len = i 
#
# Change backslash characters into forward slash
#
for i in range (0, (len+1)):
#    print(newname[i]) 
    if newname[i] == '\x5C':
        newname[i] = '/'
#
# Convert list back to string
#
newpath = ''.join(newname)
#
# Create /data/ sub-directory if needed
#
path = Path(newpath  + 'testfile.txt')
path.parent.mkdir(parents=True, exist_ok=True)
#
# Fix pathname for sub-directory
#
subdir = newpath
#
#print(ser)
print()
print(">>> **********************************************************************************")
print(">>> anaero technology - www.anaero.co.uk - file upload utility")
print(">>> this utility performs an upload of all log files from the Arduino SD card")
print(">>> data logging will still remain active")
print()
print(">>> checking for /data/ sub-directory", subdir)
print(">>> sub-directory will be created if it doesn't already exist")
print()
print(">>> serial ports being used on this machine are:",'\n')
ports = list(serial.tools.list_ports.comports())
for p in ports:
    print ("    ",p,'\n')
print(">>> enter the name of the COM port the Arduino Mega 2560 is connected to")
print(">>> e.g. COM1 or COM2 or COM3 or COM4 or similar, and then press enter",'\n')
var = input(">>> ")
time.sleep(1)
print(">>> configuring serial port")
ser.port = var
ser.baudrate = 57600
ser.timeout = 180
ser.setDTR(False)
time.sleep(0.5)
print(">>> port configured")
print(">>> opening serial port")
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
print()
print(">>> requesting connection to Arduino - this may take a short while",'\n')
line = ser.readline()
line = line.rstrip()
while (line != b'ping'):      # wait for ping to be received then send request
    line = ser.readline()
    line = line.rstrip()
#    print(line)
time.sleep(0.01)
ser.write(b'9999x') 
print(">>> waiting - writeback request sent",'\n')
while (line != b'Connection request acknowledged....'):
    line = ser.readline()
    line = line.rstrip()
print(">>> connection request acknowledged....",'\n') 
while (line != b'starting eventlog.csv writeback'):
    line = ser.readline()
    line = line.rstrip()
    if(line == b'starting eventlog.csv writeback'):
        print(">>> ",line.decode(),'\n')
#
#   start upload of log files
#
#subdir = '\x5C' + 'data' + '\x5C'
fileName = 'eventlog_'
fileExtn = '.csv'
timeStamp = datetime.datetime.strftime(datetime.datetime.now(), '%Y_%m_%d_%H_%M')
fullName = subdir + fileName + timeStamp + fileExtn
f1 = open(fullName, 'wb')   # open file for binary write (replace existing)
f1.write(b'file uploaded: ')
line = datetime.datetime.strftime(datetime.datetime.now(), '%Y-%m-%d %H:%M:%S')
f1.write(line.encode())
f1.write(b',\n')    
while (line != b'writeback completed - eventlog.csv closed'):
    line = ser.readline()
    f1.write(line)
    line = line.rstrip()
    time.sleep(0.0001)
    print('\t', line.decode())
f1.close()
#
print()
while (line != b'starting snapshot.csv writeback'):
    line = ser.readline()
    line = line.rstrip()
    print('\t',line.decode())
fileName = 'snapshot_'
fileExtn = '.csv'
timeStamp = datetime.datetime.strftime(datetime.datetime.now(), '%Y_%m_%d_%H_%M')
fullName = subdir + fileName + timeStamp + fileExtn   
f2 = open(fullName, 'wb')   # open file for binary write (replace existing)
f2.write(b'file uploaded: ')
line = datetime.datetime.strftime(datetime.datetime.now(), '%Y-%m-%d %H:%M:%S')
f2.write(line.encode())
f2.write(b',\n')    
while (line != b'writeback completed - snapshot.csv closed'):
    line = ser.readline()
    f2.write(line)
    line = line.rstrip()
    print('\t', line.decode())    
f2.close()
print()
#
while (line != b'starting daily.csv writeback'):
    line = ser.readline()
    line = line.rstrip()
    print('\t', line.decode())
fileName = 'daily_'
fileExtn = '.csv'
timeStamp = datetime.datetime.strftime(datetime.datetime.now(), '%Y_%m_%d_%H_%M')
fullName = subdir + fileName + timeStamp + fileExtn    
f3 = open(fullName, 'wb')   # open file for binary write (replace existing)
f3.write(b'file uploaded: ')
line = datetime.datetime.strftime(datetime.datetime.now(), '%Y-%m-%d %H:%M:%S')
f3.write(line.encode())
f3.write(b',\n')
while (line != b'writeback completed - daily.csv closed'):
    line = ser.readline()
    f3.write(line)
    line = line.rstrip()
    print('\t', line.decode())    
f3.close()
print()
#
while (line != b'starting hourly.csv writeback'):
    line = ser.readline()
    line = line.rstrip()
    print('\t', line.decode())
fileName = 'hourly_'
fileExtn = '.csv'
timeStamp = datetime.datetime.strftime(datetime.datetime.now(), '%Y_%m_%d_%H_%M')
fullName = subdir + fileName + timeStamp + fileExtn    
f4 = open(fullName, 'wb')   # open file for binary write (replace existing)
f4.write(b'file uploaded: ')
line = datetime.datetime.strftime(datetime.datetime.now(), '%Y-%m-%d %H:%M:%S')
f4.write(line.encode())
f4.write(b',\n')
while (line != b'writeback completed - hourly.csv closed'):
    line = ser.readline()
    f4.write(line)
    line = line.rstrip()
    print('\t', line.decode())    
f4.close()                     
ser.close()
print()
print(">>> upload of logfiles complete :-)")   
print(">>> filegrab utility will close in 10 seconds")
time.sleep(10)
print(">>> bye...")
time.sleep(2)
exit # Successful exit