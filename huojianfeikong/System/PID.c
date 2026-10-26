#include "stm32f10x.h"                  // Device header
#include "PID.h"
 
void PIDController_Init(PIDController* pid, float Kp, float Ki, float Kd, float setpoint) {
 pid-> Kp = Kp;
 pid-> Ki = Ki;
 pid-> Kd = Kd;
 pid-> setpoint = setpoint;
 pid-> error_sum = 0;
 pid-> last_error = 0;
}


float PIDController_compute(PIDController* pid, float input, float dt) {

	float error = pid-> setpoint - input;

	pid-> error_sum += error * dt;

	float error_delta = ( error- pid-> last_error) / dt;

	float output = pid-> Kp * error + pid-> Ki * pid-> error_sum + pid-> Kd * error_delta;
	pid-> last_error = error;
	return output;
}