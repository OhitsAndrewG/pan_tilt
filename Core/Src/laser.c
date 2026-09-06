#include "laser.h"
#include "main.h"

void laser_on(void){
     HAL_GPIO_WritePin(laser_GPIO_Port,laser_Pin,GPIO_PIN_SET);
}

void laser_off(void){
    HAL_GPIO_WritePin(laser_GPIO_Port,laser_Pin,GPIO_PIN_RESET);
}