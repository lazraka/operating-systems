#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <getopt.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <poll.h>
#include <sys/types.h>

//Support debugging on Linux system
#ifdef DUMMY
#define MRAA_GPIO_IN 0
typedef int mraa_aio_context;
typedef int mraa_gpio_context;

mraa_aio_context mraa_aio_init(int p) {
	return 0; //return fake device handler
}
int mraa_aio_read(mraa_aio_context c) {
	return 650; //return fake temperature value
}
void mraa_aio_close(mraa_aio_context c) {
	//empty close function
}

mraa_gpio_context mraa_gpio_init(int p) {
	return 0; //return fake device handler
}
void mraa_gpio_dir(mraa_gpio_context c, int d) {
	//empty input/out register
}
int mraa_gpio_read(mraa_gpio_context c) {
	return 0; //return fake button input
}
void mraa_gpio_close(mraa_gpio_context c) {
	//empty close function
}

#else
#include <mraa.h>
#include <mraa/aio.h>
#endif

#define BUFFERSIZE 128
#define GO 1

int run_flag = 1;
int stop_flag = 0;
int status = GO;
char scale = 'C';

//code segment taken from Discussion 1B week 8 slides
#define B 4275 //thermistor value
#define R0 100000.0 //nominal base value


float convert_temp_reading(int reading) { //mraa_aio_read(dev_aio)
	float R = 1023.0/((float) reading) - 1.0;
	R = R0*R;
	float return_temp;
	//C is the temp in celsius
	return_temp = 1.0/(log(R/R0)/B + 1/298.15) - 273.15;
	if (scale == F) {
		//F is the temp in fahrenheit
		return_temp = (return_temp*9)/5 + 32;
	}
	return return_temp;
}

//localtime API
void print_current_time() {
	struct timespec ts;
	struct tm *tm;
	clock_gettime(CLOCK_REALTIME, &ts);
	tm = localtime(&(ts.tv_sec));
	sprintf(time_buf,"%02d:%02d:%02d\n", tm->tm_hour, tm->tm_min, tm->tm_sec);
	fputs(time_buf, 1);
}

void button_handler() {
	char handler_buf[BUFFERSIZE];
	local_time = localtime(&curr_time.tv_sec);
	sprintf(handler_buf, "%02d:%02d:%02d SHUTDOWN\n", tm->tm_hour, tm->tm_min, tm->tm_sec);
	fputs(time_buf, 1);
	run_flag = 0;
}

void handleCommand(char* command) {
	if (strncmp(command, 'SCALE=', sizeof(char) * 6) == 0){
		if (command[6] == 'C') {
			scale = 'C';
		}
		else if (command[6] == 'F') {
			scale = 'F';
		}
		printf("%s\n", command);
	}
	else if (strncmp(command, 'PERIOD=', sizeof(char)*7) == 0) {
		period = atoi(&command[7]);
		printf("%s\n", command);
	}
	else if (strncmp(command, 'STOP', sizeof(char)*4) == 0) {
		stop_flag = 1;
		printf("%s\n", command);
	}
	else if (strncmp(command, 'START', sizeof(char)*5) == 0) {
		stop_flag = 0;
		printf("%s\n", command);
	}
	else if (strncmp(command, 'OFF', sizeof(char)*3) == 0) {
		printf("%s\n", command);
		button_handler();
	}
	else if (strncmp(command, 'LOG ', sizeof(char)*4) == 0) {
		printf("%s\n", command);
	}
	else {
		printf("%s\n", command);
	}
}

//Program run on Beaglebone
int main (int argc, char **argv) {
	/*Process arguments from command line*/
	struct option optionargs[] = {
		{"period", 1, NULL, 'p'},
		{"scale", 1, NULL, 's'},
		{"log", 0, NULL, 'l'},
		{0,0,0,0}
	};

	//initialize variables
	int period = 1;
	char* log_file = NULL;
	char scale_temp;
	char cmd_option;

	while(run_flag) {
		cmd_option = getopt_long(argc, argv, "", optionargs, NULL);
		if (cmd_option < 0) {
			break;
		}
		else if (cmd_option == 'p') {
			period = atoi(optarg);
		}
		else if (cmd_option == 's') {
			scale_temp = optarg;
			if (scaletemp == 'F') {
				scale = 'F';
			}
			else {
				fprintf(stderr, "Incorrect arguments, options either 'F' or 'C'\n");
				exit(1);
			}
		}
		else if (cmd_option == 'l') {
			log_file = optarg;
		}
		else if (cmd_option == '?'){
			fprintf(stderr, "Incorrect arguments. Correct usage: ./lab4b --threads=# --iterations=# --yield='idl' --sync='m' or 's'\n");
			exit(1);
		}
	}

	//if log input specified, print to file, if not, print to stdout which is fd 1
	if (log_file) {
		int output_fd = creat(log_file, 0644);

		if (output_fd >= 0) {
			close(1);
			dup(output_fd);
			close(output_fd);
		}
		else if (output_fd < 0) {
			fprintf(stderr, "%s: %s\n", log_file, strerror(errno));
			exit(1);
		}
	}
	// now always write to stdout because changed file descriptor

	//initialize the sensors, could be written as seperate function
	//MRAA gpio code example
	int ret_gpio = 0;
	//assume device is connected with address 60
	mraa_gpio_context button_gpio = mraa_gpio_init(60); //initialize gpio_context, based on board number
	if (button_gpio == NULL) {
		fprintf(stderr, "Initializing GPIO for temperature sensor failed. Error: %s\n", strerror(errno));
		exit(1);
	}

	//configure the device interface to be an input pin
	mraa_gpio_dir(button_gpio, MRAA_GPIO_IN); //set the interface to be an input pin
	//mraa_gpio_dir(device, MRAA_GPIO_OUT); //set the interface to be an output pin

	//configure button handler if putton is pressed
	mraa_gpio_isr(button_gpio, MRAA_GPIO_EDGE_RISING, &button_handler, NULL);
	//read from device, results stored in ret
	ret_gpio = mraa_gpio_read(dev_gpio);
	//mraa_gpio_write(device, value); write value to device

	//MRAA aio code example
	int ret_aio = 0;
	//assume device is connected with address 1
	mraa_aio_context temp_aio = mraa_aio_init(1);
	if (temp_aio == NULL) {
		fprintf( stderr, "Initializing AIO for button failed. Error: %s\n", strerror(errno));
		exit(1);
	}

	//obtain current time
	time_t curr_time;
	time_t next_time = 0;

	time(&curr_time);

	//poll from standard input
	struct pollfd pollStdin[1]
	pollStdin[0].fd = 0
	pollStdin[0].events = POLLIN + POLLHUP +POLLERR;

	while (run_flag){
		time(&curr_time);
		if (curr_time.tv_sec >= next_time && !(stop_flag)) { //read from temperature sensor, convert and report
			int temp_reading = mraa_aio_read(temp_aio);
			if (temp_reading < 0) {
				fprintf(stderr, "Failed to obtain temperature reading. Error: %s\n", strerror(errno));
				exit(1);
			}
			float converted_temp = convert_temp_reading(temp_reading);
			print_current_time();
			next_time = curr_time.tv_sec + period;
		}
		
		//poll standard input
		//int ret = poll(&pollStdin, 1, 1000);
		if (poll(pollStdin, 1, 0) < 0) {
			fprintf(stderr, "Failed poll call. Error:%s\n", strerror(errno));
			exit(1);
		}
		else if (pollStdin[0].revents & POLLIN) {
			//read from stdin until encounter '\n' and process command
			char buf[BUFFERSIZE];
			int count;
			if ((count = read(0, buf, BUFFERSIZE)) < 0) {
				fprintf(stderr, "Reading from stdin failed. Error: %s\n", strerror(errno));
				exit(1);
			}
			char command[BUFFERSIZE];
			int i;
			for (i=0; i<count; i++) {
				if (buf[i] == '\n') {
					handleCommand(command);
					memset(command, 0, BUFFERSIZE);
				}
				else {
					char c = buf[i];
					strcat(command, &c);
				}
			}
		}

		//if (push button is pressed)
			//log and exit
	}
	if (mraa_gpio_close(button_gpio) < 0) {
		fprintf(stderr, "Failed to closee GPIO button. Error: %s\n", strerror(errno));
		exit(1);
	}

	if(mraa_aio_close(temp_aio) < 0) {
		fprintf(stderr, "Failed to closee AIO temperature sensor. Error: %s\n", strerror(errno));
		exit(1);
	}

}
