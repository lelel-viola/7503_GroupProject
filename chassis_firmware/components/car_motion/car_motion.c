#include "car_motion.h"


#include "stdio.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "motor.h"
#include <math.h>

car_motion_t micro_car;

// 线速度和角速度
// static float line_v = 0;
// static float angular_v = 0;

static float speed_L1_setup = 0;
static float speed_L2_setup = 0;
static float speed_R1_setup = 0;
static float speed_R2_setup = 0;


// 小车停止 Car stop
void Motion_Stop(uint8_t brake)
{
    Motor_Stop(brake);
}

// // 控制小车运动
// // Control car motion
// void Motion_Ctrl(float V_x, float V_y, float V_z)
// {
//     line_v = V_x;
//     angular_v = V_z;
//     speed_L1_setup = line_v - angular_v * ROBOT_APB;
//     speed_L2_setup = line_v - angular_v * ROBOT_APB;
//     speed_R1_setup = line_v + angular_v * ROBOT_APB;
//     speed_R2_setup = line_v + angular_v * ROBOT_APB;
//     Motor_Set_Speed(speed_L1_setup, speed_L2_setup, speed_R1_setup, speed_R2_setup);
// }

void Motion_Ctrl(float V_x, float V_y, float V_z)
{
    // 麦克纳姆轮运动学模型
    speed_L1_setup = V_x - V_y - V_z * ROBOT_APB;  // 前左轮
    speed_R1_setup = V_x + V_y + V_z * ROBOT_APB;  // 前右轮
    speed_L2_setup = V_x + V_y - V_z * ROBOT_APB;  // 后左轮
    speed_R2_setup = V_x - V_y + V_z * ROBOT_APB;  // 后右轮
    
    Motor_Set_Speed(speed_L1_setup, speed_R1_setup, speed_L2_setup, speed_R2_setup);
}

// // 获取小车运动的速度
// // Get the speed of the car's motion
// void Motion_Get_Speed(car_motion_t* car)
// {
//     float speed_m1 = 0, speed_m2 = 0, speed_m3 = 0, speed_m4 = 0;
//     Motor_Get_Speed(&speed_m1, &speed_m2, &speed_m3, &speed_m4);

//     car->Vx = (speed_m1 + speed_m2 + speed_m3 + speed_m4) / 4;
//     car->Vy = 0;
//     car->Wz = -(speed_m1 + speed_m2 - speed_m3 - speed_m4) / 4.0f / ROBOT_APB;
//     if(car->Wz == 0) car->Wz = 0;
// }


void Motion_Get_Speed(car_motion_t* car)
{
    float speed_m1 = 0, speed_m2 = 0, speed_m3 = 0, speed_m4 = 0;
    Motor_Get_Speed(&speed_m1, &speed_m2, &speed_m3, &speed_m4);

    // 麦克纳姆轮运动学逆解 - 从轮速计算机器人速度
    // 假设轮子布局：
    // m1: 前左轮    m2: 前右轮
    // m3: 后左轮    m4: 后右轮
    
    // 麦克纳姆轮运动学模型：
    // Vx = (m1 + m2 + m3 + m4) / 4
    // Vy = (-m1 + m2 + m3 - m4) / 4  
    // Wz = (-m1 + m2 - m3 + m4) / (4 * (Lx + Ly))
    
    car->Vx = (speed_m1 + speed_m2 + speed_m3 + speed_m4) / 4.0f;
    car->Vy = (-speed_m1 + speed_m2 + speed_m3 - speed_m4) / 4.0f;
    car->Wz = (-speed_m1 + speed_m2 - speed_m3 + speed_m4) / (4.0f * ROBOT_APB);
    
    // 防止浮点数精度问题导致的微小数值
    if(fabsf(car->Wz) < 0.001f) car->Wz = 0.0f;
    if(fabsf(car->Vx) < 0.001f) car->Vx = 0.0f;
    if(fabsf(car->Vy) < 0.001f) car->Vy = 0.0f;
}

// // 控制小车的运动状态
// // Control the motion state of the car
// void Motion_Ctrl_State(uint8_t state, float speed)
// {
//     if (speed < 0) speed = -speed;
//     if (speed > 1.0) speed = 1.0;
//     switch (state)
//     {
//     case MOTION_STOP:
//         Motion_Stop(STOP_COAST);
//         break;
//     case MOTION_RUN:
//         Motion_Ctrl(speed, 0, 0);
//         break;
//     case MOTION_BACK:
//         Motion_Ctrl(-speed, 0, 0);
//         break;
//     case MOTION_LEFT:
//         Motion_Ctrl(speed, 0, speed*ROBOT_SPIN_SCALE);
//         break;
//     case MOTION_RIGHT:
//         Motion_Ctrl(speed, 0, -speed*ROBOT_SPIN_SCALE);
//         break;
//     case MOTION_SPIN_LEFT:
//         Motion_Ctrl(0, 0, speed*ROBOT_SPIN_SCALE);
//         break;
//     case MOTION_SPIN_RIGHT:
//         Motion_Ctrl(0, 0, -speed*ROBOT_SPIN_SCALE);
//         break;
//     case MOTION_BRAKE:
//         Motion_Stop(STOP_BRAKE);
//         break;
//     default:
//         break;
//     }
// }


// Control the motion state of the car - Mecanum Wheel Version
void Motion_Ctrl_State(uint8_t state, float speed)
{
    if (speed < 0) speed = -speed;
    if (speed > 1.0) speed = 1.0;
    
    switch (state)
    {
    case MOTION_STOP:
        Motion_Stop(STOP_COAST);
        break;
        
    case MOTION_RUN:
        Motion_Ctrl(speed, 0, 0);  // 前进
        break;
        
    case MOTION_BACK:
        Motion_Ctrl(-speed, 0, 0); // 后退
        break;
        
    case MOTION_LEFT:
        // 麦轮：向左平移（不再是差速转向）
        Motion_Ctrl(0, speed, 0);
        break;
        
    case MOTION_RIGHT:
        // 麦轮：向右平移（不再是差速转向）
        Motion_Ctrl(0, -speed, 0);
        break;
        
    case MOTION_SPIN_LEFT:
        Motion_Ctrl(0, 0, 2 * speed * ROBOT_SPIN_SCALE); // 原地左转
        break;
        
    case MOTION_SPIN_RIGHT:
        Motion_Ctrl(0, 0, -2 * speed * ROBOT_SPIN_SCALE); // 原地右转
        break;
        
    case MOTION_BRAKE:
        Motion_Stop(STOP_BRAKE);
        break;
        
    default:
        break;
    }
}