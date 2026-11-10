/*
 * DASE7503 - Stage 2: Single Motor Closed-loop PID Speed Control
 * Board: Yahboom Micro-ROS Controller Board (ESP32-S3)
 * Motor: M1 (Left Front)
 *
 * Topics:
 *   Subscribe: /set_rpm   (std_msgs/Int32)               target RPM
 *              /set_pid   (std_msgs/Float32MultiArray)   [Kp, Ki, Kd]
 *   Publish:   /rpm       (std_msgs/Int32)               actual RPM
 */

#include <micro_ros_arduino.h>
#include <stdio.h>
#include <rcl/rcl.h>
#include <rcl/error_handling.h>
#include <rclc/rclc.h>
#include <rclc/executor.h>
#include <std_msgs/msg/int32.h>
#include <std_msgs/msg/float32_multi_array.h>

// ========== Pins for Motor 1 ==========
#define BUZZER_PIN   46
#define PWMA         4
#define PWMB         5
#define ENCODER_A    6
#define ENCODER_B    7

#define PPR          13
#define GEAR_RATIO   20.0

// Encoder
volatile long encoderCount = 0;
double currentRPM = 0.0;

// PID
double Setpoint = 0.0;
double Kp = 2.0, Ki = 0.4, Kd = 0.05;
unsigned long pidInterval = 20; // ms
double error = 0, previous_error = 0;
double integral = 0, derivative = 0;
double Output = 0;

unsigned long tBuzzerStarted = 0;
bool bBuzzerStarted = false;

// micro-ROS
#define RCCHECK(fn) { rcl_ret_t temp_rc = fn; if((temp_rc != RCL_RET_OK)){error_loop();}}
#define RCSOFTCHECK(fn) { rcl_ret_t temp_rc = fn; if((temp_rc != RCL_RET_OK)){}}

rcl_publisher_t publisher_rpm;
std_msgs__msg__Int32 msg_rpm;

rcl_subscription_t subscriber_set_rpm;
std_msgs__msg__Int32 msg_set_rpm;

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

void IRAM_ATTR encoderISR() {
  if (digitalRead(ENCODER_A) == digitalRead(ENCODER_B)) {
    encoderCount++;
  } else {
    encoderCount--;
  }
}

void setMotorOutput(double pwm) {
  int p = (int)constrain(abs(pwm), 0, 255);
  if (pwm > 5) {
    analogWrite(PWMA, p);
    analogWrite(PWMB, 0);
  } else if (pwm < -5) {
    analogWrite(PWMA, 0);
    analogWrite(PWMB, p);
  } else {
    analogWrite(PWMA, 0);
    analogWrite(PWMB, 0);
  }
}

// FreeRTOS PID 控制任务
void ControlTask(void *pvParameters) {
  TickType_t xLastWakeTime = xTaskGetTickCount();
  const TickType_t xFrequency = pdMS_TO_TICKS(pidInterval);

  while (true) {
    vTaskDelayUntil(&xLastWakeTime, xFrequency);

    double dt = pidInterval / 1000.0;

    noInterrupts();
    long count = encoderCount;
    encoderCount = 0;
    interrupts();

    double revolutions = (double)count / PPR;
    double rpm_motor = revolutions * (60000.0 / pidInterval);
    currentRPM = rpm_motor / GEAR_RATIO;

    // PID
    error = Setpoint - currentRPM;
    integral += error * dt;
    integral = constrain(integral, -200, 200);   // anti-windup
    derivative = (error - previous_error) / dt;
    Output = Kp * error + Ki * integral + Kd * derivative;
    previous_error = error;

    if (Setpoint >= 0) {
      Output = constrain(Output, 0, 255);
    } else {
      Output = constrain(Output, -255, 0);
    }

    setMotorOutput(Output);
  }
}

void timer_callback(rcl_timer_t * timer, int64_t last_call_time) {
  RCLC_UNUSED(last_call_time);
  if (timer != NULL) {
    msg_rpm.data = (int32_t)currentRPM;
    RCSOFTCHECK(rcl_publish(&publisher_rpm, &msg_rpm, NULL));
  }
}

void callback_set_rpm(const void *msgin) {
  const std_msgs__msg__Int32 *msg = (const std_msgs__msg__Int32 *)msgin;
  Setpoint = (double)msg->data;

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

  pinMode(PWMA, OUTPUT);
  pinMode(PWMB, OUTPUT);
  analogWrite(PWMA, 0);
  analogWrite(PWMB, 0);

  pinMode(ENCODER_A, INPUT);
  pinMode(ENCODER_B, INPUT);
  attachInterrupt(digitalPinToInterrupt(ENCODER_A), encoderISR, CHANGE);

  // 启动 PID 控制任务
  xTaskCreate(ControlTask, "ControlTask", 4096, NULL, 1, NULL);

  Serial.begin(115200);
  delay(2000);
  Serial.println("Starting micro-ROS...");

  set_microros_transports();
  delay(2000);
  Serial.println("Transport set");

  allocator = rcl_get_default_allocator();
  rcl_ret_t rc;

  rc = rclc_support_init(&support, 0, NULL, &allocator);
  Serial.print("support_init: "); Serial.println((int)rc);
  if (rc != RCL_RET_OK) error_loop();

  rc = rclc_node_init_default(&node, "single_motor_pid_node", "", &support);
  Serial.print("node_init: "); Serial.println((int)rc);
  if (rc != RCL_RET_OK) error_loop();

  // Timer
  rc = rclc_timer_init_default(&timer, &support, RCL_MS_TO_NS(100), timer_callback);
  Serial.print("timer_init: "); Serial.println((int)rc);
  if (rc != RCL_RET_OK) error_loop();

  // Publisher /rpm
  rc = rclc_publisher_init_default(
    &publisher_rpm, &node,
    ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Int32),
    "/rpm");
  Serial.print("publisher_init: "); Serial.println((int)rc);
  if (rc != RCL_RET_OK) error_loop();

  // Subscriber /set_rpm
  rc = rclc_subscription_init_default(
    &subscriber_set_rpm, &node,
    ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Int32),
    "/set_rpm");
  Serial.print("sub_set_rpm: "); Serial.println((int)rc);
  if (rc != RCL_RET_OK) error_loop();

  // Subscriber /set_pid
  rc = rclc_subscription_init_default(
    &subscriber_set_pid, &node,
    ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Float32MultiArray),
    "/set_pid");
  Serial.print("sub_set_pid: "); Serial.println((int)rc);
  if (rc != RCL_RET_OK) error_loop();

  // Executor (1 timer + 2 subscriptions)
  rc = rclc_executor_init(&executor, &support.context, 3, &allocator);
  Serial.print("executor_init: "); Serial.println((int)rc);
  if (rc != RCL_RET_OK) error_loop();

  rc = rclc_executor_add_timer(&executor, &timer);
  Serial.print("add_timer: "); Serial.println((int)rc);
  if (rc != RCL_RET_OK) error_loop();

  rc = rclc_executor_add_subscription(
    &executor, &subscriber_set_rpm, &msg_set_rpm,
    &callback_set_rpm, ON_NEW_DATA);
  Serial.print("add_sub_rpm: "); Serial.println((int)rc);
  if (rc != RCL_RET_OK) error_loop();

  rc = rclc_executor_add_subscription(
    &executor, &subscriber_set_pid, &msg_set_pid,
    &callback_set_pid, ON_NEW_DATA);
  Serial.print("add_sub_pid: "); Serial.println((int)rc);
  if (rc != RCL_RET_OK) error_loop();

  Serial.println("===== All init OK! =====");
}

void loop() {
  RCSOFTCHECK(rclc_executor_spin_some(&executor, RCL_MS_TO_NS(10)));

  if (bBuzzerStarted && (millis() - tBuzzerStarted > 300)) {
    digitalWrite(BUZZER_PIN, LOW);
    bBuzzerStarted = false;
  }
}