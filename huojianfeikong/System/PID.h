#ifndef __PID_H
#define __PID_H

typedef struct {
 float Kp; 
 float Ki; 
 float Kd; 
 float setpoint;
 float error_sum; 
 float last_error; 
}PIDController;

 
void PIDController_Init(PIDController* pid, float Kp, float Ki, float Kd, float setpoint);
float PIDController_compute(PIDController* pid, float input, float dt);
#endif