#pragma once
#include "cmsis_os.h"

extern const osThreadAttr_t pwdTask_attr;
void PwdTask(void *argument);
