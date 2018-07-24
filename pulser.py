#!/usr/bin/python
#######
# GPM - FREQ for default model 
#GPM     Freq
#5.216   16
#7.148   22
#10.368  32
#15.198  47
#20.35	63
#30.334	94

#######
import RPi.GPIO as GPIO
from time import sleep
import sys
import signal
import time

pulses_per_sec = 8
rate = 1.0/pulses_per_sec
print(rate)

def signal_handler(signal, frame):
  print
  GPIO.cleanup()
  sys.exit(0)

def lopper():
  while 1:
    for index in range(pulses_per_sec):
      GPIO.output(14,0)
      GPIO.output(14,1)
      sleep(rate)
    localtime = time.asctime( time.localtime(time.time()) )
    print "done sendind at :", localtime
      

GPIO.setmode(GPIO.BCM)
GPIO.setup(14,GPIO.OUT)
signal.signal(signal.SIGINT,signal_handler)
print("Press Ctrl+c to Stop Pulse train")
lopper()
GPIO.cleanup()

