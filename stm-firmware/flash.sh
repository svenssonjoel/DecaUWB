#!/bin/sh
openocd -f stm32f407g.cfg -c "program ./build/TEST.elf verify reset exit"
