#include "gpiolib_addr.h"
#include "gpiolib_reg.h"

#include <string.h>
#include <stdint.h>
#include <stdio.h> //for the printf() function
#include <fcntl.h>
//#include <linux/watchdog.h> //needed for the watchdog specific constants
#include <unistd.h>         //needed for sleep
#include <sys/ioctl.h>      //needed for the ioctl function
#include <stdlib.h>         //for atoi
#include <time.h>           //for time_t and the time() function
#include <sys/time.h>       //for gettimeofday()

enum State
{
    HUB,
    BLINK1,
    BLINK2,
    BLINK_DONE,
    DONE
};


//This function will initialize the GPIO pins and handle any error checking
//for the initialization
GPIO_Handle initializeGPIO()
{       
        //This is the same initialization that was done in Lab 2
        GPIO_Handle gpio;
        gpio = gpiolib_init_gpio();
        if(gpio == NULL)
        {       
                perror("Could not initialize GPIO");
        }
        return gpio;
}

//This function will change the appropriate pins value in the select register
//so that the pin can function as an output
void setToOutput(GPIO_Handle gpio, int pinNumber)
{       
        //Check that the gpio is functional
        if(gpio == NULL)
        {       
                printf("The GPIO has not been intitialized properly \n");
                return;
        }
        
        //Check that we are trying to set a valid pin number
        if(pinNumber < 2 || pinNumber > 27)
        {       
                printf("Not a valid pinNumer \n");
                return;
        }
        
        //This will create a variable that has the appropriate select
        //register number. For more information about the registers
        //look up BCM 2835.
        int registerNum = pinNumber / 10;
        
        //This will create a variable that is the appropriate amount that
        //the 1 will need to be shifted by to set the pin to be an output
        int bitShift = (pinNumber % 10) * 3;
        
        //This is the same code that was used in Lab 2, except that it uses
        //variables for the register number and the bit shift
        uint32_t sel_reg = gpiolib_read_reg(gpio, GPFSEL(registerNum));
        sel_reg |= 1  << bitShift;
        gpiolib_write_reg(gpio, GPFSEL(1), sel_reg);
}

int encode(int input, int CaesarShift)
{
    int in = input + CaesarShift;
    return in;
}

void Send(GPIO_Handle gpio, int ascii)
{
    int laser2 = 0; //how many times laser2 needs to blink

    int laser1 = 0; //how many times laser1 needs to blink

    if (ascii == 32)
    {
        laser2 = 1; //space?
    }
    else if (65 <= ascii && ascii <= 124)
    {
        laser1 = ascii - 64; //letter
    }
    else
    {
        laser2 = 2; //done (received something invalid, indicating end of transmission)
    }

    enum State s = HUB;

    while (s != DONE)
    {
        switch (s)
        {
        case HUB:
            if (laser1 == 0 && laser2 == 0)
            {
                s = BLINK_DONE; //blink both lights to indicate end of transmission
                break;
            }
            else if (laser2 != 0 && laser1 == 0)
            {
                s = BLINK2; //blink LED2
                break;
            }
            else
            {
                s = BLINK1; //blink LED1
                break;
            }
        case BLINK1:
            gpiolib_write_reg(gpio, GPSET(1), 1 << 18); //turn on laser1
            usleep(200);
            laser1--;                                   //decrement laser1 counter
            gpiolib_write_reg(gpio, GPCLR(0), 1 << 18); //turn off laser1
            s = HUB;
            break;
        case BLINK2:
            gpiolib_write_reg(gpio, GPSET(1), 1 << 17); //turn on laser2
            usleep(200);
            laser2--;                                   //decrement laser2 counter
            gpiolib_write_reg(gpio, GPCLR(0), 1 << 17); //turn off laser2
            s = HUB;
            break;
        case BLINK_DONE:
            gpiolib_write_reg(gpio, GPSET(1), 1 << 17); //turn on both lasers
            gpiolib_write_reg(gpio, GPSET(1), 1 << 18); //turn on both lasers
            usleep(200);
            gpiolib_write_reg(gpio, GPSET(1), 1 << 17); //turn on both lasers
            gpiolib_write_reg(gpio, GPSET(1), 1 << 18); //turn on both lasers
            s = DONE;
            break;
        case DONE:
            break;
        }
    }
}

int main(const int argc, const char* const argv[])
{
    /* 
    This section of the code initializes the GPIO
    stuff. Makes the lasers ready to lase and the
    photodiodes ready to photodiode.
    */

    GPIO_Handle gpio;
    gpio =  initializeGPIO();//turn on gpio
    setToOutput(gpio,17);
    setToOutput(gpio,18);
    /*****************************************************/

    /* 
    This section of the code reads the file
    converts it into an int array containing the ASCII
    values of the contents of the file. 
    */

    FILE *file; //input file pointer

    file = fopen("file.txt", "r"); //set value of pointer to point to input file; will be reading from the file

    char inString[200]; //create array to store chars from input file

    fgets(inString, 999, file); //using fgets to read from file, write contents into array

    int len = strlen(inString); //getting length of resulting string

    int input[200] = {0}; //new array to store ASCII values of string

    for (int i = 0; i < len; i++) //loop to typecast char to int and write to array
    {
        input[i] = (int)(inString[i]);
        printf("%c", inString[i]);
    }

    for (int k = 0; k < len; k++)
    {
        input[k] = encode(input[k], atof(argv[1])); //encoding via encode function
    }

    /***********************************************************************/

    /* 
    This section of the code waits
    for ten seconds to accomodate
    the delay implemented into the
    other system.
    The purpose of this is to allow
    the user to properly align the
    laser and photodiode to ensure 
    a consistent and reliable connection.
*/

    // ****** Need to start laser here to aid alignment *****

    //how long the delay will be in secs. Implemented as int to allow for quick changing.
    //should be set higher than that of the receiving system to accomodate for inaccuracy of usleep. Sending can start after receiving has begun, but the opposite is untrue.
    
    int delay = 11;

    int i = 1; //will set to '0' to break loop

    int timer = 0; // timer. Not using gettime methods because no need for precise timing here

    while (i)
    {
        if (timer >= 1000000 * delay)
        {
            i = 0; //break while loop after duration has elapsed by setting i to 0;
        }
        usleep(1000); //this will act as a (rough) timer. Make sure argument of usleep is equal to how much timer is incremented by
        timer += 1000;
    }
    printf("hi");
    for (int j = 0; j < len; j++) //loop to send laser pulses using "Send" function
    {
        Send(gpio, input[i]);
    }
    Send(gpio, 0);

    return 0;
}
