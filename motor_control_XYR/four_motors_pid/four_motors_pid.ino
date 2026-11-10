/*
 * DASE7503 - Stage 3: Four Motors Independent PID Speed Control
 * Board: Yahboom Micro-ROS Controller Board (ESP32-S3)
 * Motors: M1=LF, M2=LR, M3=RF, M4=RR
 *
 * Topics:
 *   Subscribe: /set_rpms  (std_msgs/Float32MultiArray)  data[4] = target RPMs for M1~M4
 *              /set_pid   (std_msgs/Float32MultiArray)  [Kp, Ki, Kd]  (shared)
 *   Publish:   /rpms      (std_msgs/Float32MultiArray)  actual RPMs of 4 motors
 */

#include <micro_ros_arduino.h>
#include <micro_ros_utilities/type_utilities.h>
#include <stdio.h>
#include <rcl/rcl.h>
#include <rcl/error_handling.h>
#include <rclc/rclc.h>
#include <rclc/executor.h>
#include <std_msgs/msg/float32_multi_array.h>

#define BUZZER_PIN 46
#define NUM_MOTORS 4
#define PPR 13
#define GEAR_RATIO 20.0
#define PID_INTERVAL_MS 20

// Pin definitions: {pwmA, pwmB, encA, encB}
const int motorPins[NUM_MOTORS][4] = {
  {4,  5,  6,  7},   // M1 LF
  {15, 16, 47, 48},  // M2 LR
  {9,  10, 11, 12},  // M3 RF
  {13, 14, 1,  2}    // M4 RR
};

struct MotorCtrl {
  volatile long count;
  double setpoint;
  double currentRPM;
  double error, prev_error, integral, derivative, output;
};

MotorCtrl motors[NUM_MOTORS];

// Shared PID gains
double Kp = 2.0, Ki = 0.4, Kd = 0.05;

unsigned long tBuzzerStarted = 0;
bool bBuzzerStarted = false;

#define RCCHECK(fn) { rcl_ret_t temp_rc = fn; if((temp_rc != RCL_RET_OK)){error_loop();}}
#define RCSOFTCHECK(fn) { rcl_ret_t temp_rc = fn; if((temp_rc != RCL_RET_OK)){}}

rcl_publisher_t publisher_rpms;
std_msgs__msg__Float32MultiArray msg_rpms;

rcl_subscription_t subscriber_set_rpms;
std_msgs__msg__Float32MultiArray msg_set_rpms;

rcl_subscription_t subscriber_set_pid;
std_msgs__msg__Float32MultiArray msg_set_pid;

rclc_executor_t executor;
rclc_support_t support;
rcl_allocator_t allocator;
rcl_node_t node;
rcl_timer_t timer;

void error_loop() {
  while (1) {
    digitalWrite(BUZZER_PIN, !digitalRead(BUZZER_PIN));
    delay(100);
  }
}

// Individual ISRs
void IRAM_ATTR encoderISR0() {
  if (digitalRead(motorPins[0][2]) == digitalRead(motorPins[0][3])) motors[0].count++;
  else motors[0].count--;
}
void IRAM_ATTR encoderISR1() {
  if (digitalRead(motorPins[1][2]) == digitalRead(motorPins[1][3])) motors[1].count++;
  else motors[1].count--;
}
void IRAM_ATTR encoderISR2() {
  if (digitalRead(motorPins[2][2]) == digitalRead(motorPins[2][3])) motors[2].count++;
  else motors[2].count--;
}
void IRAM_ATTR encoderISR3() {
  if (digitalRead(motorPins[3][2]) == digitalRead(motorPins[3][3])) motors[3].count++;
  else motors[3].count--;
}

void setMotorPWM(int idx, double pwm) {
  int p = (int)constrain(abs(pwm), 0, 255);
  if (pwm > 5) {
    analogWrite(motorPins[idx][0], p);
    analogWrite(motorPins[idx][1], 0);
  } else if (pwm < -5) {
    analogWrite(motorPins[idx][0], 0);
    analogWrite(motorPins[idx][1], p);
  } else {
    analogWrite(motorPins[idx][0], 0);
    analogWrite(motorPins[idx][1], 0);
  }
}

void ControlTask(void *pvParameters) {
  TickType_t xLastWakeTime = xTaskGetTickCount();
  const TickType_t xFrequency = pdMS_TO_TICKS(PID_INTERVAL_MS);

  while (true) {
    vTaskDelayUntil(&xLastWakeTime, xFrequency);
    double dt = PID_INTERVAL_MS / 1000.0;

    for (int i = 0; i < NUM_MOTORS; i++) {
      noInterrupts();
      long count = motors[i].count;
      motors[i].count = 0;
      interrupts();

      double revolutions = (double)count / PPR;
      double rpm_motor = revolutions * (60000.0 / PID_INTERVAL_MS);
      motors[i].currentRPM = rpm_motor / GEAR_RATIO;

      // PID
      motors[i].error = motors[i].setpoint - motors[i].currentRPM;
      motors[i].integral += motors[i].error * dt;
      motors[i].integral = constrain(motors[i].integral, -200, 200);
      motors[i].derivative = (motors[i].error - motors[i].prev_error) / dt;
      motors[i].output = Kp * motors[i].error + Ki * motors[i].integral + Kd * motors[i].derivative;
      motors[i].prev_error = motors[i].error;

      if (motors[i].setpoint >= 0)
        motors[i].output = constrain(motors[i].output, 0, 255);
      else
        motors[i].output = constrain(motors[i].output, -255, 0);

      setMotorPWM(i, motors[i].output);
    }
  }
}

void timer_callback(rcl_timer_t * timer, int64_t last_call_time) {
  RCLC_UNUSED(last_call_time);
  if (timer != NULL) {
    for (int i = 0; i < NUM_MOTORS; i++) {
      msg_rpms.data.data[i] = (float)motors[i].currentRPM;
    }
    msg_rpms.data.size = NUM_MOTORS;
    RCSOFTCHECK(rcl_publish(&publisher_rpms, &msg_rpms, NULL));
  }
}

void callback_set_rpms(const void *msgin) {
  const std_msgs__msg__Float32MultiArray *msg =
    (const std_msgs__msg__Float32MultiArray *)msgin;
  if (msg->data.size >= NUM_MOTORS) {
    for (int i = 0; i < NUM_MOTORS; i++) {
      motors[i].setpoint = msg->data.data[i];
    }
  }
  digitalWrite(BUZZER_PIN, HIGH);
  tBuzzerStarted = millis();
  bBuzzerStarted = true;
}

void callback_set_pid(const void *msgin) {
  const std_msgs__msg__Float32MultiArray *msg =
    (const std_msgs__msg__Float32MultiArray *)msgin;
  if (msg->data.size >= 3) {
    Kp = msg->data.data[0];
    Ki = msg->data.data[1];
    Kd = msg->data.data[2];
  }
  digitalWrite(BUZZER_PIN, HIGH);
  tBuzzerStarted = millis();
  bBuzzerStarted = true;
}

void setup() {
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  for (int i = 0; i < NUM_MOTORS; i++) {
    pinMode(motorPins[i][0], OUTPUT);
    pinMode(motorPins[i][1], OUTPUT);
    analogWrite(motorPins[i][0], 0);
    analogWrite(motorPins[i][1], 0);
    pinMode(motorPins[i][2], INPUT);
    pinMode(motorPins[i][3], INPUT);
    motors[i].count = 0;
    motors[i].setpoint = 0;
    motors[i].currentRPM = 0;
    motors[i].integral = 0;
    motors[i].prev_error = 0;
  }

  attachInterrupt(digitalPinToInterrupt(motorPins[0][2]), encoderISR0, CHANGE);
  attachInterrupt(digitalPinToInterrupt(motorPins[1][2]), encoderISR1, CHANGE);
  attachInterrupt(digitalPinToInterrupt(motorPins[2][2]), encoderISR2, CHANGE);
  attachInterrupt(digitalPinToInterrupt(motorPins[3][2]), encoderISR3, CHANGE);

  xTaskCreatePinnedToCore(ControlTask, "ControlTask", 8192, NULL, 1, NULL, 1);

  set_microros_transports();
  delay(2000);

  allocator = rcl_get_default_allocator();
  RCCHECK(rclc_support_init(&support, 0, NULL, &allocator));
  RCCHECK(rclc_node_init_default(&node, "four_motors_pid_node", "", &support));

  static micro_ros_utilities_memory_conf_t conf = {0};
  // allocate for messages
  micro_ros_utilities_create_message_memory(
    ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Float32MultiArray), &msg_rpms, conf);
  micro_ros_utilities_create_message_memory(
    ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Float32MultiArray), &msg_set_rpms, conf);
  micro_ros_utilities_create_message_memory(
    ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Float32MultiArray), &msg_set_pid, conf);

  // pre-size data arrays
  static float rpm_data[4] = {0};
  static float set_rpm_data[4] = {0};
  static float pid_data[3] = {0};
  msg_rpms.data.data = rpm_data;
  msg_rpms.data.capacity = 4;
  msg_rpms.data.size = 4;
  msg_set_rpms.data.data = set_rpm_data;
  msg_set_rpms.data.capacity = 4;
  msg_set_pid.data.data = pid_data;
  msg_set_pid.data.capacity = 3;

  const unsigned int timer_timeout = 50;
  RCCHECK(rclc_timer_init_default(&timer, &support, RCL_MS_TO_NS(timer_timeout), timer_callback));

  RCCHECK(rclc_publisher_init(
    &publisher_rpms, &node,
    ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Float32MultiArray),
    "/rpms", &rmw_qos_profile_sensor_data));

  RCCHECK(rclc_subscription_init_default(
    &subscriber_set_rpms, &node,
    ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Float32MultiArray),
    "set_rpms"));

  RCCHECK(rclc_subscription_init_default(
    &subscriber_set_pid, &node,
    ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Float32MultiArray),
    "set_pid"));

  RCCHECK(rclc_executor_init(&executor, &support.context, 3, &allocator));
  RCCHECK(rclc_executor_add_timer(&executor, &timer));
  RCCHECK(rclc_executor_add_subscription(
    &executor, &subscriber_set_rpms, &msg_set_rpms, &callback_set_rpms, ON_NEW_DATA));
  RCCHECK(rclc_executor_add_subscription(
    &executor, &subscriber_set_pid, &msg_set_pid, &callback_set_pid, ON_NEW_DATA));
}

void loop() {
  RCCHECK(rclc_executor_spin_some(&executor, RCL_MS_TO_NS(10)));
  if (bBuzzerStarted && (millis() - tBuzzerStarted > 300)) {
    digitalWrite(BUZZER_PIN, LOW);
    bBuzzerStarted = false;
  }
}
