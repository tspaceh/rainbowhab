#pressure sensor code
#ToivoHn
#12 May 2014

import os

#pressure sensor
#i2c address
pressure_sensor = 0x77
reset = 0x1E
pressure_read = 0x48
temperature_read = 0x58
adc_read = 0x00
####
pressure_sensitivity = 0xA2
pressure_offset = 0xA4
temperature_coefficient_of_pressure_sensitivity = 0xA6
temperature_coefficient_of_pressure_offset = 0xA8
reference_temperature = 0xAA
temperature_coefficient_of_temperature=0xAC

import time

#let python talk to I2C devices
from smbus import SMBus

bus = SMBus(1)


#reset sequence
print('resetting pressure sensor')
bus.write_byte(pressure_sensor,reset)
time.sleep(1)
print('pressure sensor reset')

print('reading factory calibration values')
#read 6 factory set coefficients in PROM (factory calibration)
C1 = bus.read_i2c_block_data(pressure_sensor,pressure_sensitivity)
time.sleep(0.05)
C2 = bus.read_i2c_block_data(pressure_sensor,pressure_offset)
time.sleep(0.05)
C3 = bus.read_i2c_block_data(pressure_sensor,temperature_coefficient_of_pressure_sensitivity)
time.sleep(0.05)
C4 = bus.read_i2c_block_data(pressure_sensor,temperature_coefficient_of_pressure_offset)
time.sleep(0.05)
C5 = bus.read_i2c_block_data(pressure_sensor,reference_temperature)
time.sleep(0.05)
C6 = bus.read_i2c_block_data(pressure_sensor,temperature_coefficient_of_temperature)
time.sleep(0.05)

#converting the 2 8bit packages (16 bit according to datasheet) into decimal
C1 = C1[0]*256.0+C1[1]
C2 = C2[0]*256.0+C2[1]
C3 = C3[0]*256.0+C3[1]
C4 = C4[0]*256.0+C4[1]
C5 = C5[0]*256.0+C5[1]
C6 = C6[0]*256.0+C6[1]

print('Calibration values are:')
print('C1='+str(C1))
print('C2='+str(C2))
print('C3='+str(C3))
print('C4='+str(C4))
print('C5='+str(C5))
print('C6='+str(C6))

while True:

    #the pressure sensor has i2c address 0x77 last time I checked
    #0x48 is a command from the datasheet (Convert D1 OSR=4096)
    #www.embeddedadventures.com/datasheets/ms5611.pdf 
    #print('Sending pressure read command')
    bus.write_byte(pressure_sensor,pressure_read)
    time.sleep(0.05)
    #this writes the command to the sensor to calculate the digital value
    #of pressure (this is a raw value and will be used in calculations later to
    #get the actual real pressure

    #print('Reading pressure value from analog pressure value converted to digital pressure')

    #this is the command that reads the data that we just calculated on the sensor
    D1 = bus.read_i2c_block_data(pressure_sensor,adc_read)
    time.sleep(0.05)

    #much like above, we are only getting raw temperature
    #print('Sending temperature read command')
    bus.write_byte(pressure_sensor,temperature_read)
    time.sleep(0.05)
    #print('Reading temperature value from analog temperature value converted to digital temperature')

    D2 = bus.read_i2c_block_data(pressure_sensor,adc_read)
    time.sleep(0.05)

    #the data are read in blocks are in an array of 8 bit pieces so we
    #convert first 24 bits to decimal
    #print('Converting D1 and D2 into decimal')
    D1 = D1[0]*65536+D1[1]*256.0+D1[2]
    D2 = D2[0]*65536+D2[1]*256.0+D2[2]
    #print('D1='+str(D1))
    #print('D2='+str(D2))


    #the MS5611 sensor stores 6 values in the EPROM memory that we need
    #in order to calculate the actual pressure and temperature

    #using calculations on the datasheet
    dT = D2-C5*2**8
    TEMP = 2000.0+dT*C6/2**23

    OFF = C2*2**16+(C4*dT)/2**7
    SENS = C1*2**15+(C3*dT)/2**8
    P = (D1*SENS/2**21-OFF)/2**15 
    H = (1-(P/100/1013.25)**0.190284)*44307.69396
    HR_T = TEMP/100
    RO_T = round(HR_T,2)
    HR_P = P/100
    RO_P = round(HR_P,1)
    HR_H = H
    RO_H = round(HR_H,1)
    #print('Temperature: '+str(HR_T)+' C')
    #print('Pressure: '+str(HR_P)+' mbar')
    #print('Altitude: '+str(HR_H)+' meters')

    f = open('/proc/uptime')
    contents = f.read().split()
    f.close()

    total_seconds = float(contents[0])
    MINUTE = 60
    HOUR = MINUTE * 60
    DAY = HOUR * 24
    
    days = int(total_seconds / DAY)
    hours = int((total_seconds % DAY)/HOUR)
    minutes = int((total_seconds % HOUR) / MINUTE)
    seconds = int(total_seconds % MINUTE)
 
    timestring = ""
    if days>0:
	timestring +=str(days)+" "+(days ==1 and "day" or "days") + ","
    if len(timestring)>0 or hours>0:
	timestring +=str(hours)+" "+(hours ==1 and "hour" or "hours") + ","
    if len(timestring)>0 or minutes>0:
	timestring+=str(minutes)+" "+(minutes ==1 and "minute" or "minutes")+","
    timestring += str(seconds)+" "+(seconds ==1 and "seconds" or "seconds")

    



    DATASTRING = 'D|'+str(RO_T)+'|'+str(RO_P)+'|'+str(RO_H)+'|'+timestring
    print('Submitting: '+DATASTRING)
    time.sleep(0.5)
