/*
 * Copyright (c) 2016 Open-RnD Sp. z o.o.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <device.h>
#include <gpio.h>
#include <pwm.h>
#include <misc/util.h>
#include <misc/printk.h>
#include <shell/shell.h>
#include <shell/shell_uart.h>
#include <version.h>
#include <pinmux.h>
#include "../boards/x86/galileo/board.h"
#include "../boards/x86/galileo/pinmux_galileo.h"
#include "../drivers/gpio/gpio_dw_registers.h"
#include "../drivers/gpio/gpio_dw.h"
#include <stdlib.h>
#include <version.h>
#include <logging/log.h>
#include <stdio.h>
#include <kernel.h>
#include <asm_inline_gcc.h>
#include <posix/time.h>
#include <posix/time.h>

#define DEBUG 
#if defined(DEBUG) 
	#define DPRINTK(fmt, args...) printk("DEBUG: %s():%d: " fmt, \
   		 __func__, __LINE__, ##args)
#else
 	#define DPRINTK(fmt, args...) /* do nothing if not defined*/
#endif

#define EDGE_FALLING    (GPIO_INT_EDGE | GPIO_INT_ACTIVE_LOW)
#define EDGE_RISING		(GPIO_INT_EDGE | GPIO_INT_ACTIVE_HIGH)


/* PWM Device 0 */
#define PWM CONFIG_PWM_PCA9685_0_DEV_NAME

/* Thread Priorities and Stack size */
#define MY_STACK_SIZE 500
#define T1_PRIORITY 3
#define T2_PRIORITY 4
#define T3_PRIORITY 5

/* gpio_dw driver struct */
struct device *gpiob, *pwm_dev;

/* Define stack and data struct for threads */
K_THREAD_STACK_DEFINE(t1_stack_area, MY_STACK_SIZE);
K_THREAD_STACK_DEFINE(t2_stack_area, MY_STACK_SIZE);
K_THREAD_STACK_DEFINE(t3_stack_area, MY_STACK_SIZE);
struct k_thread t1_thread_data, t2_thread_data, t3_thread_data;

/* RGB pulse values of LED in clock cycle */
static uint32_t R_pulse = 4095;
static uint32_t G_pulse = 4095;
static uint32_t B_pulse = 4095;

/* Global variables for Interrupt Latency measurements */
static int intno=0;
static int int_tsc, cb_tsc, delay_tsc, int_latency;

/* Global variables for Context Switch Latency measurements */
static int cs_tsc1, cs_tsc2, cs_latency, cs_temp,cs_total, cs_tsc_d1, cs_tsc_d2, cs_delay;
static int cs_num_samples, cs_count;
static bool thread2_flag=false;

/* Callback and Pinmux device struct */
static struct gpio_callback gpio_cb;
static struct device *pinmux;

/* RDTSC inline function for timesatmp */
static inline unsigned long long __attribute__((__always_inline__))
rdtsc(void)
{
    unsigned a, d;
    __asm __volatile("rdtsc" : "=a" (a), "=d" (d));
    return ((unsigned long long) a) | (((unsigned long long) d) << 32);
}

/* Defining Mutexes and Semaphores */
K_MUTEX_DEFINE(int_mutex);
K_MUTEX_DEFINE(cs_mutex);
K_MUTEX_DEFINE(cs_thread_mutex);
K_SEM_DEFINE(t1_sem, 0, 1); //Not available initially
K_SEM_DEFINE(t2_sem, 1, 1); //available initially

/* Thread Functions for 3 threads */
extern void t1_entry_point(void * a, void * b, void * c);
extern void t2_entry_point(void * a, void * b, void * c);
extern void t3_entry_point(void * a, void * b, void * c);

/* RGB intensity control using shell sub-command 'RGB-display' */
static int rgb_intensity_control(const struct shell *shell, size_t argc, char **argv)
{
	R_pulse = (atoi(argv[1])*4096)/100;
	G_pulse= (atoi(argv[2])*4096)/100;
	B_pulse = (atoi(argv[3])*4096)/100;

	pwm_pin_set_cycles(pwm_dev, 3, 4095, R_pulse);
	pwm_pin_set_cycles(pwm_dev, 5, 4095, G_pulse);
	pwm_pin_set_cycles(pwm_dev, 7, 4095, B_pulse);

	shell_print(shell, "Intensity Values Set; R = %d, G = %d, B = %d", atoi(argv[1]), atoi(argv[2]), atoi(argv[3]));

	return 0;
}

/* Interrupt Latency Calculation */
int calc_latency(const struct shell *shell){

	//Local timestamp variables
	int delay_tsc1, delay_tsc2, temp_latency;

	//Measure Delay for GPIO write on 6 with call back disabled
	gpio_pin_disable_callback(gpiob,7);
	gpio_pin_write(gpiob, 6, 0);
	delay_tsc1 = rdtsc();
	gpio_pin_write(gpiob, 6, 1);
	delay_tsc2 = rdtsc();
	delay_tsc = (delay_tsc2 - delay_tsc1);
	gpio_pin_write(gpiob, 6, 0);

	//Enable callback to start interrupt latency measurement
	gpio_pin_enable_callback(gpiob,7);

	int_tsc = rdtsc();	//Interrupt start TSC
	gpio_pin_write(gpiob, 6, 1);
	gpio_pin_write(gpiob, 6, 0);

	//Interrupt Latency for current sample measurement
	temp_latency = ((cb_tsc - int_tsc - (delay_tsc))*1000)/400;
	shell_print(shell,"Interrupt Latency measured(ns) :%d\n", temp_latency);

	return temp_latency;
}

/* Interrupt Callback function on GPIO 7 */
void interrupt_cb(struct device *gpiob, struct gpio_callback *cb, u32_t pins)
{
	cb_tsc = rdtsc();	//Interrupt stop TSC

	u32_t val;
	gpio_pin_read(gpiob, 7, &val);

	intno++;
	printk(" ---- interrupted %d \n", intno);
	//printk("Callback TSC:%d\n", cb_tsc);

	gpio_pin_disable_callback(gpiob,7);
}

/* Interrupt Latency Measurement using shell sub-command 'int-latency' */
static int interrupt_latency(const struct shell *shell, size_t argc, char **argv)
{
	//Lock Mutex to prevent preemption for ongoing measurement request
	k_mutex_lock(&int_mutex, K_FOREVER);

	ARG_UNUSED(argc);
	int n = atoi(argv[1]);
	int latency = 0;
	int meas_lat = 0;

	//Taking requested number of samples 
	for(int i =0; i < n; i++){
		meas_lat = calc_latency(shell);
		shell_print(shell,"Sample no = %d, latency = %d", i+1, meas_lat);
		latency += meas_lat;
	}

	int_latency = latency/n;
	intno = 0;

	shell_print(shell,"Number of samples to be taken = %d", atoi(argv[1]));
	shell_print(shell,"Delay TSC:%d", delay_tsc);
	//shell_print(shell,"Interrupt TSC:%d\n", int_tsc);
	shell_print(shell,"Interrupt Latency without delay(ns) :%d", int_latency);
	//shell_print(shell,"Interrupt Latency with delay(ns) :%d", ((cb_tsc - int_tsc)*1000)/400);

	//Unlock mutex to allow new requests
	k_mutex_unlock(&int_mutex);

	return 0;
}

/* Thread 3 - Lowest Priority */
extern void t3_entry_point(void * a, void * b, void * c){

	printk("Thread 3 started\n");

	//Check if All threads started then wait for mutex to be available
	while((thread2_flag)&&(cs_count < cs_num_samples)){
		k_mutex_lock(&cs_thread_mutex, K_FOREVER);
		printk("Context Swicthed to Thread 3\n");
		k_mutex_unlock(&cs_thread_mutex);
	}
}

/* Thread 2 - Mid Priority */
extern void t2_entry_point(void * a, void * b, void * c){

	printk("Thread 2 started\n");

	//Take sample measurement for requested no. of samples
	while(cs_count < (cs_num_samples)) {

		//Take semaphore for T2 and mutex if available
		k_sem_take(&t2_sem, K_FOREVER);
		k_mutex_lock(&cs_thread_mutex, K_FOREVER);
		thread2_flag = true;

		printk("Context locked in Thread 2\n");

		//Delay measurement with high priority thread blocked
		cs_tsc_d1 = rdtsc();
		k_mutex_unlock(&cs_thread_mutex);
		cs_tsc_d2 = rdtsc();

		k_mutex_lock(&cs_thread_mutex, K_FOREVER);
		cs_delay = cs_tsc_d2 - cs_tsc_d1;

		//Trigger context switch to Thread 1
		k_sem_give(&t1_sem);
		cs_tsc1 = rdtsc();
		k_mutex_unlock(&cs_thread_mutex);
	}
}

/* Thread 1 - Highest Priority */
extern void t1_entry_point(void * a, void * b, void * c){

	printk("Thread 1 started\n");

	//Take sample measurement for requested no. of samples
	while(cs_count < (cs_num_samples)) {

		//Wait for semaphore and mutex to be available
		k_sem_take(&t1_sem, K_FOREVER);
		k_mutex_lock(&cs_thread_mutex, K_FOREVER);

		//Context switched to T1
		cs_tsc2 = rdtsc();
		printk("Context Switched from T2 to T1\n");
		k_sem_give(&t2_sem);

		//Current sample measurement for context switch
		cs_temp = (((cs_tsc2 - cs_tsc1) - cs_delay)*1000)/400;
		cs_total += cs_temp;

		printk("Context Switch Latency no.%d , value = %d\n", cs_count++, cs_temp);
		k_mutex_unlock(&cs_thread_mutex);
	}
}

/* Context Swicth Latency Measurement using shell sub-command 'cs-latency' */
static int context_switch_latency(const struct shell *shell, size_t argc, char **argv){

	//Lock Mutex to prevent preemption for ongoing measurement request
	k_mutex_lock(&cs_mutex, K_FOREVER);
	
	ARG_UNUSED(argc);
	cs_num_samples = atoi(argv[1]);

	cs_total = 0;
	cs_count = 0;
	thread2_flag=false;

	//Create Thread 1
	k_tid_t t1_tid = k_thread_create(&t1_thread_data, t1_stack_area,
	                             K_THREAD_STACK_SIZEOF(t1_stack_area),
	                             t1_entry_point,
	                             NULL, NULL, NULL,
	                             T1_PRIORITY, 0, K_NO_WAIT);
	k_thread_name_set(t1_tid, "thread1");

	//Create Thread 3
	k_tid_t t3_tid = k_thread_create(&t3_thread_data, t3_stack_area,
                             	K_THREAD_STACK_SIZEOF(t3_stack_area),
                             	t3_entry_point,
                             	NULL, NULL, NULL,
                             	T3_PRIORITY, 0, K_NO_WAIT);
	k_thread_name_set(t3_tid, "thread3");

	//Create Thread 2
	k_tid_t t2_tid = k_thread_create(&t2_thread_data, t2_stack_area,
                             	K_THREAD_STACK_SIZEOF(t2_stack_area),
                             	t2_entry_point,
                             	NULL, NULL, NULL,
                             	T2_PRIORITY, 0, K_NO_WAIT);
	k_thread_name_set(t2_tid, "thread2");

	//Wait for ongoing sample measurements to complete
	while(cs_count < cs_num_samples) {
		shell_print(shell,"");
	}

	cs_latency = cs_total/cs_num_samples;

	shell_print(shell,"Context Switch Latency final = %d\n", cs_latency);

	//Unlock mutex to allow new requests
	k_mutex_unlock(&cs_mutex);

	return 0;
}

/* Register Shell Command 'project1' and its sub-commands */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_proj1,
	SHELL_CMD_ARG(RGB-display, NULL, "PWM RGB control command.", rgb_intensity_control,4,0),
	SHELL_CMD_ARG(int-latency, NULL, "Interrupt latency command.", interrupt_latency,2,0),
	SHELL_CMD_ARG(cs-latency, NULL, "Context Switch command.", context_switch_latency,2,0),
	SHELL_SUBCMD_SET_END /* Array terminated. */
);
SHELL_CMD_REGISTER(project1, &sub_proj1, "CSE522:Project1 commands", NULL);

/* Main Function */
void main(void)
{
	int ret;

	//Binding for pinmux and pwm
	pinmux=device_get_binding(CONFIG_PINMUX_NAME);
	pwm_dev = device_get_binding(PWM);

	struct galileo_data *dev = pinmux->driver_data;
	
    gpiob=dev->gpio_dw; //retrieving gpio_dw driver struct from pinmux driver
						// Alternatively, gpiob = device_get_binding(PORT);
	if (!gpiob) {
		DPRINTK("error\n");
		return;
	}

	DPRINTK("Binding done\n");

	//Set all pins to be used for PWM display and interrupt
	ret=pinmux_pin_set(pinmux,5,PINMUX_FUNC_C); //IO 5-- PWM3
	if(ret<0)
		DPRINTK("error setting pin for IO5\n");
	
	ret=pinmux_pin_set(pinmux,6,PINMUX_FUNC_C); //IO 6 -- PWM5
	if(ret<0)
		DPRINTK("error setting pin for IO6\n");
	
	ret=pinmux_pin_set(pinmux,9,PINMUX_FUNC_C); //IO 9 -- PWM7
	if(ret<0)
		DPRINTK("error setting pin for IO9\n");
	
	ret=pinmux_pin_set(pinmux,12,PINMUX_FUNC_B); //IO12 for input interrupt  -GPIO7
	if(ret<0)
		DPRINTK("error setting pin for IO12\n");

	ret=pinmux_pin_set(pinmux,3,PINMUX_FUNC_A); //IO3 for input interrupt  -GPIO6
	if(ret<0)
		DPRINTK("error setting pin for IO3\n");

	DPRINTK("Pinmux set done\n");

	//Configure GPIO pins for interrupt
	gpio_pin_configure(gpiob,6,GPIO_DIR_OUT);
	ret=gpio_pin_configure(gpiob, 7, GPIO_DIR_IN | GPIO_INT | EDGE_RISING);
	DPRINTK("Configure done\n");
	
	//Set Callback for interrupt
	gpio_init_callback(&gpio_cb, interrupt_cb, BIT(7));
	ret=gpio_add_callback(gpiob, &gpio_cb);
	if(ret<0)
		DPRINTK("error adding callback\n");

	DPRINTK("Callback added\n");

	//Set GPIO 6 to 0 initially
	gpio_pin_write(gpiob, 6, 0);

	//Set PWM values to 100% on boot
	pwm_pin_set_cycles(pwm_dev, 3, 4095, R_pulse);
	pwm_pin_set_cycles(pwm_dev, 5, 4095, G_pulse);
	pwm_pin_set_cycles(pwm_dev, 7, 4095, B_pulse);
	DPRINTK("PWM intensity set to 100 percent");
}