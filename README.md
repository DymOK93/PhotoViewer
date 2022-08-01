# PhotoViewer
* Works in conjunction with [ColorCompressor](https://github.com/DymOK93/ColorCompressor): acts as a master on a software-supported parallel port and a slave on a hardware serial port (USART).
* Upon receipt of the image change command through USART, a next picture in the BMP format is read from the root folder of the SD card and sent by pixel to the second device through a parallel port for transcoding color from BGR888 to RGB666 for further rendering on the display. 
* Pressing the joystick buttons initiates sending a command to turn on and off green and blue LEDs on the second board.

## Target 
STM32F412ZG-Discovery board