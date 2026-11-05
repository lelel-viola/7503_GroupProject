import rclpy
from rclpy.node import Node
from geometry_msgs.msg import Twist
from std_msgs.msg import Int32  # 新增：用于发布舵机角度
import sys
import termios
import tty


KEY_BINDINGS = {
    # 移动控制（原有功能）
    'w': (0.20, 0.0, 0.0),    # 前进（x+）
    's': (-0.20, 0.0, 0.0),   # 后退（x-）
    'a': (0.0, 0.20, 0.0),    # 左移（y+）
    'd': (0.0, -0.20, 0.0),   # 右移（y-）
    'e': (0.0, 0.0, -1.0),   # 右转（角速度z-）
    'q': (0.0, 0.0, 1.0),    # 左转（角速度z+）
    'x': (0.0, 0.0, 0.0),    # 停止
    # 舵机控制（新增功能）
    'z': ('servo', 90),      # 设置舵机S1为90度
    'c': ('servo', 50)       # 设置舵机S1为50度
}

class KeyboardTeleopNode(Node):
    def __init__(self):
        super().__init__('keyboard_teleop_node')
        # 创建运动控制发布者（原有）
        self.cmd_vel_pub = self.create_publisher(Twist, 'cmd_vel', 10)
        # 新增：创建舵机控制发布者
        self.servo_pub = self.create_publisher(Int32, 'servo_s1', 10)
        
        self.get_logger().info("""
        麦轮键盘控制节点启动：
        移动控制：
        W：前进   S：后退   A：左移   D：右移
        Q：左转   E：右转   X：停止
        舵机控制：
        Z：舵机S1角度设为90度
        C：舵机S1角度设为60度
        Ctrl+C：退出
        """)

    def get_key(self):
        """读取键盘输入（不阻塞，无需回车）"""
        fd = sys.stdin.fileno()
        old_settings = termios.tcgetattr(fd)
        try:
            tty.setraw(sys.stdin.fileno())
            key = sys.stdin.read(1)
        finally:
            termios.tcsetattr(fd, termios.TCSADRAIN, old_settings)
        return key

def main(args=None):
    rclpy.init(args=args)
    node = KeyboardTeleopNode()
    twist = Twist()       # 运动控制消息
    servo_msg = Int32()   # 舵机控制消息（新增）

    try:
        while True:
            key = node.get_key()
            if key in KEY_BINDINGS:
                value = KEY_BINDINGS[key]
                # 区分运动控制和舵机控制
                if value[0] == 'servo':
                    # 处理舵机控制（发布到/servo_s1话题）
                    angle = value[1]
                    servo_msg.data = angle
                    node.servo_pub.publish(servo_msg)
                    node.get_logger().info(f"舵机S1角度设置为: {angle}度")
                else:
                    # 处理运动控制（原有逻辑）
                    linear_x, linear_y, angular_z = value
                    twist.linear.x = linear_x
                    twist.linear.y = linear_y
                    twist.angular.z = angular_z
                    node.cmd_vel_pub.publish(twist)
                    node.get_logger().info(
                        f"发布: 线速度x={linear_x}, 线速度y={linear_y}, 角速度z={angular_z}"
                    )
            elif key == '\x03':  # Ctrl+C退出
                break
    except Exception as e:
        node.get_logger().error(f"错误: {e}")
    finally:
        # 停止小车（运动归零）
        twist.linear.x = 0.0
        twist.linear.y = 0.0
        twist.angular.z = 0.0
        node.cmd_vel_pub.publish(twist)
        # 释放资源
        node.destroy_node()
        rclpy.shutdown()

if __name__ == '__main__':
    main()
