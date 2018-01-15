/*
 * -------------------------------------------
 *    MSP432 DriverLib - v3_21_00_05 
 * -------------------------------------------
 *
 * --COPYRIGHT--,BSD,BSD
 * Copyright (c) 2016, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * --/COPYRIGHT--*/
/******************************************************************************
 * MSP432 Empty Project
 *
 * Description: An empty project that uses DriverLib
 *
 *                MSP432P401
 *             ------------------
 *         /|\|                  |
 *          | |                  |
 *          --|RST               |
 *            |                  |
 *            |                  |
 *            |                  |
 *            |                  |
 *            |                  |
 * Author: 
 *******************************************************************************/
/* DriverLib Includes */
#include "driverlib.h"

/* Standard Includes */
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "ST7735.h"

#include "dark.txt"
#include "overcast.txt"
#include "partlysunny.txt"
#include "sunny.txt"
#include "twilight.txt"

/* Application Defines  */
#define TIMER_PERIOD    0x2DC6

typedef struct sensor_data sensor_data;
struct sensor_data{
    float temp;
    float Bp;
    float Rh;

};

typedef enum
{
    item1 = 0,
    item2 = 1,
    item3 = 2,
    item4 = 3,
    item5 = 4
}menu_items;

menu_items main_menu = item1;
int menu_item_changed = 0;
sensor_data inside_data={23.3,3.3,2.3}, outside_data={23.3,3.3,2.3}; //temp data

typedef enum
{
    DARK = 0,
    TWILIGHT = 1,
    OVERCAST = 2,
    PARTLY_SUNNY = 3,
    SUNNY = 4

}Light_Status;

Light_Status current_status = SUNNY;

/* Timer_A UpMode Configuration Parameter */
const Timer_A_UpModeConfig upConfig =
{
        TIMER_A_CLOCKSOURCE_SMCLK,              // SMCLK Clock Source
        TIMER_A_CLOCKSOURCE_DIVIDER_64,          // SMCLK/1 = 3MHz //one second
        TIMER_PERIOD,                           // 5000 tick period
        TIMER_A_TAIE_INTERRUPT_DISABLE,         // Disable Timer interrupt
        TIMER_A_CCIE_CCR0_INTERRUPT_ENABLE ,    // Enable CCR0 interrupt
        TIMER_A_DO_CLEAR                        // Clear value
};

int timer_count = 0;
int light_status_changed = 0;
int switches_disabled = 0;

volatile int MCLKfreq, SMCLKfreq;

void num_to_enum(int x);
void num_to_menu_item(int x);

void print_current_status_pic(void);

void PORT2_IRQHandler(void);

void clockInit48MHzXTL(void) {  // sets the clock module to use the external 48 MHz crystal

    /* Configuring pins for peripheral/crystal usage */
    MAP_GPIO_setAsPeripheralModuleFunctionOutputPin(GPIO_PORT_PJ,
                                                    GPIO_PIN3 | GPIO_PIN2, GPIO_PRIMARY_MODULE_FUNCTION);
    MAP_GPIO_setAsOutputPin(GPIO_PORT_P1, GPIO_PIN0);

    CS_setExternalClockSourceFrequency(32000,48000000); // enables getMCLK, getSMCLK to know externally set frequencies

    /* Starting HFXT in non-bypass mode without a timeout. Before we start
     * we have to change VCORE to 1 to support the 48MHz frequency */
    MAP_PCM_setCoreVoltageLevel(PCM_VCORE1);
    MAP_FlashCtl_setWaitState(FLASH_BANK0, 2);
    MAP_FlashCtl_setWaitState(FLASH_BANK1, 2);
    CS_startHFXT(false);  // false means that there are no timeouts set, will return when stable

    /* Initializing MCLK to HFXT (effectively 48MHz) */
    MAP_CS_initClockSignal(CS_MCLK, CS_HFXTCLK_SELECT, CS_CLOCK_DIVIDER_1);
}

// Subroutine to wait 1 msec (assumes 48 MHz clock)
// Inputs: n  number of 1 msec to wait
// Outputs: None
// Notes: implemented in ST7735.c as count of assembly instructions executed
void Delay1ms(uint32_t n);

// Subroutine to wait 10 msec
// Inputs: n  number of 10 msec to wait
// Outputs: None
// Notes: calls Delay1ms repeatedly
void Delay10ms(uint32_t n){
    Delay1ms(n*10);
}

void create_data_display(void);

void TA1_0_IRQHandler(void);
void highlight_menu_option(void);
//void draw_menu_items(void);

uint16_t grid_color = ST7735_CYAN;
uint16_t menu_text_color = ST7735_YELLOW;
uint16_t highlight_text_color = ST7735_CYAN;

//create an array for the position of each option for use later
int menu_position[5][2]={
                     {15,4},
                     {15,5},
                     {15,6},
                     {15,7},
                     {15,8}
};
//create array for menu item names
char *menu_names[5] = {
                     {"item1"},
                     {"item2"},
                     {"item3"},
                     {"item4"},
                     {"item5"}
};

int main(void)
{
    /* Stop Watchdog  */
    MAP_WDT_A_holdTimer();

    clockInit48MHzXTL();  // set up the clock to use the crystal oscillator on the Launchpad
    MAP_CS_initClockSignal(CS_MCLK, CS_HFXTCLK_SELECT, CS_CLOCK_DIVIDER_1);
    MAP_CS_initClockSignal(CS_SMCLK, CS_HFXTCLK_SELECT, CS_CLOCK_DIVIDER_4);
    //    MAP_CS_setDCOCenteredFrequency(CS_DCO_FREQUENCY_48);  // use this if the crystal oscillator does not respond
    //    MAP_CS_initClockSignal(CS_MCLK, CS_DCOCLK_SELECT, CS_CLOCK_DIVIDER_1);
    //    MAP_CS_initClockSignal(CS_SMCLK, CS_DCOCLK_SELECT, CS_CLOCK_DIVIDER_4);  // set SMCLK to 12 MHz
    SMCLKfreq=MAP_CS_getSMCLK();  // get SMCLK value to verify it was set correctly
    MCLKfreq=MAP_CS_getMCLK();  // get MCLK value


    /* Configuring Timer_A1 for Up Mode */
    MAP_Timer_A_configureUpMode(TIMER_A1_BASE, &upConfig);

    ST7735_InitR(INITR_REDTAB); // initialize LCD controller IC

    /* Configuring P1.1 as an input and enabling interrupts */
    MAP_GPIO_setAsInputPinWithPullUpResistor(GPIO_PORT_P2, GPIO_PIN6);
    MAP_GPIO_clearInterruptFlag(GPIO_PORT_P2, GPIO_PIN6);
    MAP_GPIO_enableInterrupt(GPIO_PORT_P2, GPIO_PIN6);
    MAP_GPIO_setAsInputPinWithPullUpResistor(GPIO_PORT_P2, GPIO_PIN7);
    MAP_GPIO_clearInterruptFlag(GPIO_PORT_P2, GPIO_PIN7);
    MAP_GPIO_enableInterrupt(GPIO_PORT_P2, GPIO_PIN7);

    //MAP_Interrupt_enableInterrupt(INT_TA0_N);
    MAP_Interrupt_registerInterrupt(INT_PORT2, &PORT2_IRQHandler);
    MAP_Interrupt_enableInterrupt(INT_PORT2);
    Timer_A_registerInterrupt(TIMER_A1_BASE,TIMER_A_CCR0_INTERRUPT,TA1_0_IRQHandler);

    /* Enabling SRAM Bank Retention */
    MAP_SysCtl_enableSRAMBankRetention(SYSCTL_SRAM_BANK1);

    print_current_status_pic();

    create_data_display();

    //create temp data
    char data[5];
    switches_disabled=1;
    //print temp
    sprintf(data,"%2.2f",inside_data.temp);
    Delay10ms(1);
    ST7735_DrawString(5,13,data,menu_text_color);

    //print humidity
    sprintf(data,"%2.2f",inside_data.Rh);
    Delay10ms(1);
    ST7735_DrawString(5,14,data,menu_text_color);

    Delay10ms(1);

    //print temp
    sprintf(data,"%2.2f",outside_data.temp);
    Delay10ms(1);
    ST7735_DrawString(15,13,data,menu_text_color);

    //print humidity
    sprintf(data,"%2.2f",outside_data.Rh);
    Delay10ms(1);
    ST7735_DrawString(15,14,data,menu_text_color);

    //print bp
    sprintf(data,"%2.2f",outside_data.Bp);
    Delay10ms(1);
    ST7735_DrawString(15,15,data,menu_text_color);
    Delay10ms(1);
    switches_disabled=0;

    highlight_menu_option();

    /* Enabling MASTER interrupts */
    MAP_Interrupt_enableMaster();

    MAP_Timer_A_startCounter(TIMER_A1_BASE, TIMER_A_UP_MODE);

    while(1)
    {
        //changes should be made by interrupts
        if(light_status_changed){
            switches_disabled=1; //(Mutex of sorts)prevents status change while picture is printed to screen
            print_current_status_pic();
            switches_disabled=0;
            light_status_changed = 0;
        }

        if(menu_item_changed){
            //update current selected item
            highlight_menu_option();
        }
    }
}

void highlight_menu_option(void){
    //int i = (int)main_menu;
    int i;
    /*switch(i){
        case 0:
            //highlight menu item 1
            ST7735_DrawString(menu_position[i][0],menu_position[i][1],menu_names[i], highlight_text_color);
            break;
        case 1:
            ST7735_DrawString(menu_position[i][0],menu_position[i][1],menu_names[i], highlight_text_color);
            break;
        case 2:
            ST7735_DrawString(menu_position[i][0],menu_position[i][1],menu_names[i], highlight_text_color);
            break;
        case 3:
            ST7735_DrawString(menu_position[i][0],menu_position[i][1],menu_names[i], highlight_text_color);
            break;
        case 4:
            ST7735_DrawString(menu_position[i][0],menu_position[i][1],menu_names[i], highlight_text_color);
            break;
        }*/

    //if main_menu selection == current print '
    for(i=0; i<5; i++){
        if((int)main_menu == i){
            ST7735_DrawString(menu_position[i][0],menu_position[i][1],menu_names[i], highlight_text_color);
        }else{
            ST7735_DrawString(menu_position[i][0],menu_position[i][1],menu_names[i], menu_text_color);
        }
    }
}

/*
void draw_menu_items(void){
    int i;
    for(i=0; i<5; i++){
        ST7735_DrawString(menu_position[i][0],menu_position[i][1],menu_names[i], menu_text_color);
    }
}
*/
/* GPIO ISR */
void PORT2_IRQHandler(void)
{
    uint32_t status;
    int current_item;

    status = MAP_GPIO_getEnabledInterruptStatus(GPIO_PORT_P2);
    MAP_GPIO_clearInterruptFlag(GPIO_PORT_P2, status);

    //debounce
    Delay10ms(2);
    //confirm either pushbutton is still pressed
    if(MAP_GPIO_getInputPinValue(GPIO_PORT_P2,GPIO_PIN6) != 0 && MAP_GPIO_getInputPinValue(GPIO_PORT_P2, GPIO_PIN7) != 0){
        return;
    }

    //use GPIO to select menu items
    //highlight current menu selection
    if(status & GPIO_PIN6)
    {
        //Decrement status
        current_item = ((int)(main_menu) - 1);
        if(current_item == -1)
            current_item = 4;

        num_to_menu_item(current_item);
        menu_item_changed = 1;

    }else if(status & GPIO_PIN7){
        //Increment status
        current_item = ((int)(main_menu) + 1);
        if(current_item == 5)
            current_item = 0;
        num_to_menu_item(current_item);
        menu_item_changed = 1;
    }
/*
    if(!switches_disabled){
        switches_disabled = 1;
        if(status & GPIO_PIN6)
        {
            //Decrement status
            current = ((int)(current_status) - 1);
            if(current == -1)
                current = 4;
            num_to_enum(current);
            light_status_changed = 1;
        }else if(status & GPIO_PIN7){
            //increment status
            current = ((int)(current_status) + 1);
            if(current == 5)
                current = 0;
            num_to_enum(current);
            light_status_changed = 1;
        }
        switches_disabled = 0;
    }
*/
}
void num_to_menu_item(int x){
    switch(x){
    case 0:
        main_menu = item1;
        break;
    case 1:
        main_menu = item2;
        break;
    case 2:
        main_menu = item3;
        break;
    case 3:
        main_menu = item4;
        break;
    case 4:
        main_menu = item5;
        break;
    }
}

void num_to_enum(int x){
    switch(x){
    case 0:
        current_status = DARK;
        break;
    case 1:
        current_status = TWILIGHT;
        break;
    case 2:
        current_status = OVERCAST;
        break;
    case 3:
        current_status = PARTLY_SUNNY;
        break;
    case 4:
        current_status = SUNNY;
        break;
    }
}

void print_current_status_pic(void){
    // Must be less than or equal to 128 pixels wide by 160 pixels high
    uint16_t picture_width = 64, picture_hight = 80;
    //was 32 and v = 100
    uint16_t horizontal_start = 10, vertical_start = 100;

    //Clear status text

    switch(current_status){
    case DARK:
        ST7735_DrawBitmap(horizontal_start, vertical_start, dark, picture_width, picture_hight);
        Delay10ms(10);
        /*ST7735_SetCursor(30,0);
        ST7735_OutString("Status: Dark");*/
        ST7735_DrawString(0,0,"Status: Dark        ", menu_text_color);
        break;

    case OVERCAST:
        ST7735_DrawBitmap(horizontal_start, vertical_start, overcast, picture_width, picture_hight);
        Delay10ms(10);
        ST7735_DrawString(0,0,"Status: Overcast    ", menu_text_color);
        break;

    case PARTLY_SUNNY:
        ST7735_DrawBitmap(horizontal_start, vertical_start, partlysunny, picture_width, picture_hight);
        Delay10ms(10);
        ST7735_DrawString(0,0,"Status: Partly-Sunny", menu_text_color);
        break;

    case SUNNY:
        ST7735_DrawBitmap(horizontal_start, vertical_start, sunny, picture_width, picture_hight);
        Delay10ms(10);
        ST7735_DrawString(0,0,"Status: Sunny       ", menu_text_color);
        break;

    case TWILIGHT:
        ST7735_DrawBitmap(horizontal_start, vertical_start, twilight, picture_width, picture_hight);
        Delay10ms(10);
        ST7735_DrawString(0,0,"Status: Twilight    ", menu_text_color);
        break;
    }
}

void create_data_display(void){
    ST7735_DrawString(15,3,"Menu:",menu_text_color);
    ST7735_DrawFastHLine(80,38,50,grid_color);
    ST7735_DrawFastHLine(0,10,128,grid_color);
    Delay10ms(1);
    // Input: x         columns from the left edge (0 to 20)
    //        y         rows from the top edge (0 to 15)
    ST7735_DrawString(0,11,"  Inside     Outside",menu_text_color);
    Delay10ms(1);
    ST7735_DrawFastHLine(0,120,128,grid_color);
    Delay10ms(1);
    ST7735_DrawFastVLine(64,120,50,grid_color);

    //Home stats
    ST7735_DrawString(0,13,"TP:",menu_text_color);
    ST7735_DrawString(0,14,"RH:",menu_text_color);

    //Outside stats
    ST7735_DrawString(11,13,"TP:",menu_text_color);
    ST7735_DrawString(11,14,"RH:",menu_text_color);
    ST7735_DrawString(11,15,"BP:",menu_text_color);
}

//******************************************************************************
//
//This is the TIMERA interrupt vector service routine. //one second
//
//******************************************************************************
void TA1_0_IRQHandler(void)
{
    int current = 0;
    if(!switches_disabled && timer_count>=40){
        timer_count=0;
        switches_disabled = 1;
        //increment status
        current = ((int)(current_status) + 1);
        if(current == 5)
            current = 0;
        num_to_enum(current);
        light_status_changed = 1;

        switches_disabled = 0;
    }
    MAP_Timer_A_clearCaptureCompareInterrupt(TIMER_A1_BASE,
            TIMER_A_CAPTURECOMPARE_REGISTER_0);
    timer_count++;
}
