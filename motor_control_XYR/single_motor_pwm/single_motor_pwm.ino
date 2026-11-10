/*
 * DASE7503 - Stage 1: Single Motor Open-loop PWM Test
 * Board: Yahboom Micro-ROS Controller Board (ESP32-S3)
 * Motor: M1 (Left Front) - 310 Encoder Motor
 *
 * Topics:
 *   Subscribe: /set_pwm   (std_msgs/Int32)   positive=forward, negative=backward, 0=stop
 *   Publish:   /rpm       (std_msgs/Int32)   current RPM
 */

#include <micro_ros_arduino.h>
#include <stdio.h>
#include <rcl/rcl.h>
#include <rcl/error_handling.h>
#include <rclc/rclc.h>
#include <rclc/executor.h>
#include <std_msgs/msg/int32.h>

// ========== Yahboom Board Pins for Motor 1 (LF) ==========
#define BUZZER_PIN   46
#define PWMA         4     // M1-PWM-M1A
#define PWMB         5     // M1-PWM-M1B
#define ENCODER_A    6
#define ENCODER_B    7

// Encoder specs (310 motor)
#define PPR          13
#define GEAR_RATIO   20.0

// Encoder tracking
volatile long encoderCount = 0;
double currentRPM = 0;

unsigned long tBuzzerStarted = 0;
bool bBuzzerStarted = false;

// micro-ROS
#define RCCHECK(fn) { rcl_ret_t temp_rc = fn; if((temp_rc != RCL_RET_OK)){error_loop();}}
#define RCSOFTCHECK(fn) { rcl_ret_t temp_rc = fn; if((temp_rc != RCL_RET_OK)){}}

rcl_publisher_t publisher_rpm;
std_msgs__msg__Int32 msg_rpm;

rcl_subscription_t subscriber_set_pwm;
std_msgs__msg__Int32 msg_set_pwm;

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

void setMotorPWM(int pwm) {
  if (pwm > 0) {
    // Forward
    analogWrite(PWMA, constrain(pwm, 0, 255));
    analogWrite(PWMB, 0);
  } else if (pwm < 0) {
    // Backward
    analogWrite(PWMA, 0);
    analogWrite(PWMB, constrain(-pwm, 0, 255));
  } else {
    // Stop
    analogWrite(PWMA, 0);
    analogWrite(PWMB, 0);
  }
}

void timer_callback(rcl_timer_t * timer, int64_t last_call_time) {
  RCLC_UNUSED(last_call_time);
  if (timer != NULL) {
    noInterrupts();
    long count = encoderCount;
    encoderCount = 0;
    interrupts();

    // 100ms period -> *600 for RPM at motor shaft, then / gear
    double revolutions = (double)count / PPR;
    double rpm_motor = revolutions * 600.0;
    currentRPM = rpm_motor / GEAR_RATIO;

    msg_rpm.data = (int32_t)currentRPM;
    RCSOFTCHECK(rcl_publish(&publisher_rpm, &msg_rpm, NULL));
  }
}

void callback_set_pwm(const void *msgin) {
  const std_msgs__msg__Int32 *msg = (const std_msgs__msg__Int32 *)msgin;
  int32_t pwm = msg->data;
  setMotorPWM(pwm);

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

  set_microros_transports();
  delay(2000);

  allocator = rcl_get_default_allocator();
  RCCHECK(rclc_support_init(&support, 0, NULL, &allocator));
  RCCHECK(rclc_node_init_default(&node, "single_motor_pwm_node", "", &support));

  const unsigned int timer_timeout = 100; // ms
  RCCHECK(rclc_timer_init_default(&timer, &support, RCL_MS_TO_NS(timer_timeout), timer_callback));

  RCCHECK(rclc_publisher_init(
    &publisher_rpm, &node,
    ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Int32),
    "/rpm", &rmw_qos_profile_sensor_data));

  RCCHECK(rclc_subscription_init_default(
    &subscriber_set_pwm, &node,
    ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Int32),
    "set_pwm"));

  RCCHECK(rclc_executor_init(&executor, &support.context, 2, &allocator));
  RCCHECK(rclc_executor_add_timer(&executor, &timer));
  RCCHECK(rclc_executor_add_subscription(
    &executor, &subscriber_set_pwm, &msg_set_pwm, &callback_set_pwm, ON_NEW_DATA));
}

void loop() {
  RCCHECK(rclc_executor_spin_some(&executor, RCL_MS_TO_NS(10)));

  if (bBuzzerStarted && (millis() - tBuzzerStarted > 300)) {
    digitalWrite(BUZZER_PIN, LOW);
    bBuzzerStarted = false;
  }
}
