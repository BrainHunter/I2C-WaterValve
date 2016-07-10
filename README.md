# I2C-WaterValve

This firmware is for controling water valves with FHEM and Ethersex over a I2C Bus. The circuit is designed around a ATTiny2313A. 

##Usage:
The valves can be controlled by using the Buttons or by writing the valve registers through I2C.
A short press on the button will add 120sec opening time to this valve. A long press will close this valve. 
When using the I2C bus the following register layout is used:
```
//----- Register Layout ------//
// Register | RW?	| Usage			| Comment
// 0		| R		| Valve Status	| Bit 0 = Valve 1 Status ... Bit 5 = Valve 6 Status
// 1		| RW	| Valve 1 Time	| Watering time = Value * 10s 	--> Max 2540 Sec = 42,3mins	// 255 will not count down.
// 2		| RW	| Valve 2 Time
// 3		| RW	| Valve 3 Time
// 4		| RW	| Valve 4 Time
// 5		| RW	| Valve 5 Time
// 6		| RW	| Valve 6 Time
```
The basic time unit in the firmware is 10 sec. So a 6 in a register will open the valve for 60sec. Every 10sec the number will be decreased by 1 
until the value is 0. Then the output will be switched off. A value of 255 in a register is not counted down --> the output will stay on. 
If the number of outputs turned on is limited by the `NUMOPENVALVES` define, inactive outputs will wait until they is a "slot" ready. 

###Fhem Usage:
Required is a Ethersex with activated I2C Busmaster. 

##Circuit:
see circuit.png for a sampe circuit. This design is for some cheapy china valves. They need about 250mA at 12V each. 
If more current is needed the ULN2003A may not be suitble. Alternatively one can use Relais or discrete Transistors.

##Compiling:
The default configuration can be changed in the main.c file in the Configuration field.
- `I2C_ADDRESS` defines the address on the I2C bus default 0x20 (= 32).
- `NUMOPENVALVES` defines the maximum open valves at the same time. This is for protecting the output driver and/or the power supply from overload.
- `BUTTONSHORTTIME` defines the time in msec for a short button press.
- `BUTTONLONGTIME` defines the time in msec for a long button press.
- `BUTTONADDTIME` defines the time in 10sec steps added for a valve to be open. 

Use avr-gcc to compile the firmware. 
To compile just type: 
`make`
or under execute the make.bat 



