#!/usr/bin/env python3
import rclpy
from rclpy.node import Node
from std_msgs.msg import Float64MultiArray
from pynput.keyboard import Listener, KeyCode
import math

class MecanumPynputNode(Node):
    def __init__(self):
        super().__init__('mecanum_pynput_node')
        
        # 发布麦轮速度指令（对接 FourWheelController）
        self.wheel_pub = self.create_publisher(
            Float64MultiArray,
            'wheel_commands',
            10
        )
        
        # 速度参数（可调整快慢）
        self.max_linear_speed = 1.0  # 平移最大速度（rad/s）
        self.max_angular_speed = 0.8  # 旋转最大速度（rad/s）
        
        # 按键映射（自定义功能，U=逆时针，O=顺时针）
        self.key_mapping = {
            # 平移控制
            KeyCode.from_char('i'): ('vx', 1.0),    # I=前进
            KeyCode.from_char('k'): ('vx', -1.0),   # K=后退
            KeyCode.from_char('j'): ('vy', 1.0),    # J=左移
            KeyCode.from_char('l'): ('vy', -1.0),   # L=右移
            # 旋转控制（核心修改：U/O 对应旋转）
            KeyCode.from_char('o'): ('wz', -1.0),   # O=逆时针旋转
            KeyCode.from_char('u'): ('wz', 1.0),    # U=顺时针旋转
            # 停止
            KeyCode.from_char(' '): ('stop', 0.0),  # 空格=紧急停止
        }
        
        # 当前速度分量（初始为0）
        self.vx = 0.0  # 前进/后退
        self.vy = 0.0  # 左移/右移
        self.wz = 0.0  # 旋转
        
        # 记录已按下的按键
        self.pressed_keys = set()
        
        # 启动键盘监听（非阻塞）
        self.listener = Listener(
            on_press=self.on_key_press,
            on_release=self.on_key_release
        )
        self.listener.start()
        
        # 定时发布速度指令（10Hz，与控制器同步）
        self.timer = self.create_timer(0.1, self.publish_wheel_speed)
        
        # 启动日志提示
        self.get_logger().info('麦轮 Pynput 控制节点启动！')
        self.get_logger().info('控制按键：')
        self.get_logger().info('I(前进) K(后退) J(左移) L(右移)')
        self.get_logger().info('U(逆时针旋转) O(顺时针旋转)')
        self.get_logger().info('空格(紧急停止) ESC(退出节点)')

    def on_key_press(self, key):
        """按键按下时更新速度分量"""
        if key in self.key_mapping:
            self.pressed_keys.add(key)
            axis, value = self.key_mapping[key]
            if axis == 'vx':
                self.vx = value * self.max_linear_speed
            elif axis == 'vy':
                self.vy = value * self.max_linear_speed
            elif axis == 'wz':
                self.wz = value * self.max_angular_speed
            elif axis == 'stop':
                self.vx = 0.0
                self.vy = 0.0
                self.wz = 0.0
        
        # 按 ESC 退出节点
        elif key == KeyCode.from_char('\x1b'):
            self.get_logger().info('退出控制节点')
            self.destroy_node()
            rclpy.shutdown()

    def on_key_release(self, key):
        """按键释放时复位对应速度分量"""
        if key in self.pressed_keys:
            self.pressed_keys.remove(key)
            # 若该按键是当前唯一控制键，复位对应轴速度
            if key in self.key_mapping:
                axis, _ = self.key_mapping[key]
                # 检查是否有其他按键控制同一轴（避免组合键释放时误停）
                same_axis_keys = [k for k in self.pressed_keys if self.key_mapping[k][0] == axis]
                if not same_axis_keys:
                    if axis == 'vx':
                        self.vx = 0.0
                    elif axis == 'vy':
                        self.vy = 0.0
                    elif axis == 'wz':
                        self.wz = 0.0

    def publish_wheel_speed(self):
        """根据当前速度分量计算麦轮速度并发布"""
        # 麦轮运动学模型（核心逻辑不变）
        fl_speed = self.vx - self.vy - self.wz  # 前左轮
        fr_speed = self.vx + self.vy + self.wz  # 前右轮
        rl_speed = self.vx + self.vy - self.wz  # 后左轮
        rr_speed = self.vx - self.vy + self.wz  # 后右轮
        
        # 发布速度指令（顺序：前左、前右、后左、后右）
        wheel_msg = Float64MultiArray()
        wheel_msg.data = [fl_speed, fr_speed, rl_speed, rr_speed]
        self.wheel_pub.publish(wheel_msg)
        
        # 可选：打印当前控制状态（便于调试）
        if self.vx != 0 or self.vy != 0 or self.wz != 0:
            self.get_logger().debug(
                f'当前状态：vx={self.vx:.2f}, vy={self.vy:.2f}, wz={self.wz:.2f} | '
                f'轮子速度：FL={fl_speed:.2f}, FR={fr_speed:.2f}, RL={rl_speed:.2f}, RR={rr_speed:.2f}'
            )

def main(args=None):
    rclpy.init(args=args)
    node = MecanumPynputNode()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()

if __name__ == '__main__':
    main()
