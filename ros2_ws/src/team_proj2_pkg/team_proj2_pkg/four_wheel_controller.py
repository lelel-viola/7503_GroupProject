#!/usr/bin/env python3
import rclpy
from rclpy.node import Node
from sensor_msgs.msg import JointState
from std_msgs.msg import Float64MultiArray
import math

class FourWheelController(Node):
    def __init__(self):
        super().__init__('four_wheel_controller')
        
        # 订阅轮子速度命令
        self.wheel_commands_sub = self.create_subscription(
            Float64MultiArray,
            'wheel_commands',
            self.wheel_commands_callback,
            10
        )
        
        # 发布关节状态
        self.joint_pub = self.create_publisher(
            JointState,
            'joint_states',
            10
        )
        
        # 四个轮子的当前速度和位置
        self.wheel_velocities = [0.0, 0.0, 0.0, 0.0]  # [前左, 前右, 后左, 后右]
        self.wheel_positions = [0.0, 0.0, 0.0, 0.0]
        
        # 定时器更新关节状态
        self.timer = self.create_timer(0.1, self.publish_joint_states)
        
        self.get_logger().info('四轮独立控制器已启动')
        self.get_logger().info('等待轮子速度命令...')
    
    # def wheel_commands_callback(self, msg):
    #     """接收四个轮子的速度命令"""
    #     if len(msg.data) == 4:
    #         self.wheel_velocities = msg.data
    #         self.get_logger().info(
    #             f'轮子速度: 前左={self.wheel_velocities[0]:.2f}, '
    #             f'前右={self.wheel_velocities[1]:.2f}, '
    #             f'后左={self.wheel_velocities[2]:.2f}, '
    #             f'后右={self.wheel_velocities[3]:.2f} rad/s'
    #         )
    #     else:
    #         self.get_logger().warn(f'无效的速度命令长度: {len(msg.data)}，需要4个值')   
    
    
    def wheel_commands_callback(self, msg):
        """接收四个轮子的速度命令，仅在速度变化时打印日志"""
        if len(msg.data) == 4:
            # 将 array.array 转换为列表进行处理
            new_velocities = list(msg.data)
            # 检查新速度是否与当前速度不同（考虑浮点数精度）
            speed_changed = False
            for i in range(4):
                if abs(new_velocities[i] - self.wheel_velocities[i]) > 1e-6:
                    speed_changed = True
                    break
            
            if speed_changed:
                self.wheel_velocities = new_velocities  # 直接使用转换后的列表
                self.get_logger().info(
                    f'轮子速度更新: 前左={self.wheel_velocities[0]:.2f}, '
                    f'前右={self.wheel_velocities[1]:.2f}, '
                    f'后左={self.wheel_velocities[2]:.2f}, '
                    f'后右={self.wheel_velocities[3]:.2f} rad/s'
                )
        else:
            self.get_logger().warn(f'无效的速度命令长度: {len(msg.data)}，需要4个值')


    

    def publish_joint_states(self):
        """发布关节状态"""
        current_time = self.get_clock().now()
        dt = 0.1  # 时间间隔
        
        # 更新轮子位置
        for i in range(4):
            self.wheel_positions[i] += self.wheel_velocities[i] * dt
        
        joint_msg = JointState()
        joint_msg.header.stamp = current_time.to_msg()
        joint_msg.name = [
            'wheel_FL',    
            'wheel_FR',   
            'wheel_RL',     
            'wheel_RR'     
        ]
        
        joint_msg.position = [
            self.wheel_positions[0],
            self.wheel_positions[1], 
            self.wheel_positions[2],
            self.wheel_positions[3]
        ]
        
        joint_msg.velocity = self.wheel_velocities
        
        self.joint_pub.publish(joint_msg)

def main(args=None):
    rclpy.init(args=args)
    controller = FourWheelController()
    try:
        rclpy.spin(controller)
    except KeyboardInterrupt:
        pass
    finally:
        controller.destroy_node()
        rclpy.shutdown()

if __name__ == '__main__':
    main()