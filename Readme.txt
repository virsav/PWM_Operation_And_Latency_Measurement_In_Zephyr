
#########################################################################################################################
***********************************************Course Information********************************************************
                            	     CSE 522: Real Time Embedded Systems           
                                                Sem - Spring 2021                     
                                            Arizona State University                    
                                          Instructor: Dr. Yann-Hang Lee                 
*************************************************************************************************************************
#########################################################################################################################



#########################################################################################################################
-----------------------------------------------Assignment Information----------------------------------------------------

Author: Viraj Savaliya          ; ASU ID - 

Assignment 1 : PWM Operation and Latency Measurement in Zephyr RTOS

Date : 02/15/2021

-------------------------------------------------------------------------------------------------------------------------
----------------------------------------Instructions to Compile and Execute----------------------------------------------

How to Compile:
1. Unzip and Copy project1_87 folder in '~/zephyrproject/zephyr/samples/'
2. Change directory to zephyr. Make sure you are in '~/zephyrproject/zephyr/'
3. Set zephyr environment using 'source zephyr-env.sh'
4. Export environment variables 'export ZEPHYR_TOOLCHAIN_VARIANT=zephyr', 
   'export ZEPHYR_SDK_INSTALL_DIR=<sdk installation directory>'
5. Change directory to project1_87. Make sure you are in '~/zephyrproject/zephyr/samples/project1_87'
6. mkdir build
7. cd build
8. cmake -DBOARD=galileo ..
9. make

How to execute:
1. Copy the zephyr.strip file present in '~/zephyrproject/zephyr/samples/project1_87/build/zephyr' to
   your sd card under directory name kernel.
2. Insert the sd card on galileo board and connect board via FTDI cable with the machine.
3. Power on the board.
4. ssh terminal using '/dev/ttyUSB0' on speed '115200'
5. Press Enter on boot option and then press enter for 'Zephyr Kernel'

-------------------------------------------------------------------------------------------------------------------------
#########################################################################################################################



#########################################################################################################################
--------------------------------------------------Sample Output----------------------------------------------------------

uart:~$ DEBUG: main():340: Binding done
DEBUG: main():363: Pinmux set done
DEBUG: main():368: Configure done
DEBUG: main():376: Callback added
DEBUG: main():385: PWM intensity set to 100 percent
uart:~$ project1
project1 - CSE522:Project1 commands
Subcommands:
  RGB-display  :PWM RGB control command.
  int-latency  :Interrupt latency command.
  cs-latency   :Context Switch command.
uart:~$ project1 RGB-display 0 0 0
Intensity Values Set; R = 0, G = 0, B = 0
uart:~$ project1 int-latency 10
 ---- interrupted 1
Interrupt Latency measured(ns) :2387

nample no = 1, latency =  2---3-8 7i
 terrupted 2
Interrupt Latency measured(ns) :1712

iample no = 2, latency =  -1-7-1-2
 nterrupted 3
Interrupt Latency measured(ns) :1812

 ample no = 3, latency =  1-8-1-2-
 interrupted 4
Interrupt Latency measured(ns) :1642

inmple no = 4, latency =  1--6-4-2
  terrupted 5
Interrupt Latency measured(ns) :1642

rrmple no = 5, latency =  1--6--4 in2te
  upted 6
Interrupt Latency measured(ns) :1952

 ample no = 6, latency =  1-9-5-2-
 interrupted 7
Interrupt Latency measured(ns) :1802

 imple no = 7, latency =  1-8-0-2-
  nterrupted 8
Interrupt Latency measured(ns) :1692

 ample no = 8, latency =  1-6-9-2-
 interrupted 9
Interrupt Latency measured(ns) :1812

eample no = 9, latency =  --1--8 1i2nt
 rrupted 10
Interrupt Latency measured(ns) :1812

Sample no = 10, latency = 1812
Number of samples to be taken = 10
Delay TSC:609
Interrupt Latency without delay(ns) :1826

uart:~$ project1 cs-latency 10
Thread 1 started
Thread 3 started
Thread 2 started
Context locked in Thread 2
Context Switched from T2 to T1
Context Switch Latency no.0 , value = 4500
Context locked in Thread 2
Context Switched from T2 to T1
Context Switch Latency no.1 , value = 4372
Context locked in Thread 2
Context Switched from T2 to T1
Context Switch Latency no.2 , value = 4372
Context locked in Thread 2
Context Switched from T2 to T1
Context Switch Latency no.3 , value = 3700
Context locked in Thread 2
Context Switched from T2 to T1
Context Switch Latency no.4 , value = 4362
Context locked in Thread 2
Context Switched from T2 to T1
Context Switch Latency no.5 , value = 4980
Context locked in Thread 2
Context Switched from T2 to T1
Context Switch Latency no.6 , value = 4362
Context locked in Thread 2
Context Switched from T2 to T1
Context Switch Latency no.7 , value = 4392
Context locked in Thread 2
Context Switched from T2 to T1
Context Switch Latency no.8 , value = 5052
Context locked in Thread 2
Context Switched from T2 to T1
Context Switch Latency no.9 , value = 4460
Context Switch Latency final = 4455

-------------------------------------------------------------------------------------------------------------------------
#########################################################################################################################
