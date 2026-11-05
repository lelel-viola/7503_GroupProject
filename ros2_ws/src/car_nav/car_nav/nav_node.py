#!/usr/bin/env python3

import rclpy
from rclpy.node import Node
from geometry_msgs.msg import PoseStamped, PoseWithCovarianceStamped
from nav2_msgs.action import NavigateToPose
from action_msgs.msg import GoalStatus
from rclpy.action import ActionClient
import math
import threading

class NavigationNode(Node):
    def __init__(self):
        super().__init__('navigation_node')
        
        # 创建导航动作客户端
        self.nav_to_pose_client = ActionClient(self, NavigateToPose, 'navigate_to_pose')
        
        # 初始位置发布器
        self.initial_pose_pub = self.create_publisher(
            PoseWithCovarianceStamped, 
            '/initialpose', 
            10
        )
        
        # 等待动作服务器
        self.get_logger().info('等待NavigateToPose动作服务器...')
        self.nav_to_pose_client.wait_for_server()
        self.get_logger().info('动作服务器已连接')
        
        # 设置初始位置（可选）
        self.set_initial_pose()
        
        self.get_logger().info('导航节点已启动，等待目标点输入...')

    def set_initial_pose(self, x=0.0, y=0.0, yaw=0.0):
        """设置机器人的初始位置"""
        initial_pose = PoseWithCovarianceStamped()
        initial_pose.header.frame_id = 'map'
        initial_pose.header.stamp = self.get_clock().now().to_msg()
        
        # 设置位置
        initial_pose.pose.pose.position.x = x
        initial_pose.pose.pose.position.y = y
        
        # 将偏航角转换为四元数
        qx, qy, qz, qw = self.yaw_to_quaternion(yaw)
        initial_pose.pose.pose.orientation.x = qx
        initial_pose.pose.pose.orientation.y = qy
        initial_pose.pose.pose.orientation.z = qz
        initial_pose.pose.pose.orientation.w = qw
        
        self.initial_pose_pub.publish(initial_pose)
        self.get_logger().info(f'设置初始位置: x={x}, y={y}, yaw={yaw}')

    def navigate_to_pose(self, x, y, yaw):
        """导航到指定位置"""
        goal_msg = NavigateToPose.Goal()
        
        # 设置目标位姿
        goal_msg.pose.header.frame_id = 'map'
        goal_msg.pose.header.stamp = self.get_clock().now().to_msg()
        
        goal_msg.pose.pose.position.x = x
        goal_msg.pose.pose.position.y = y
        
        # 将偏航角转换为四元数
        qx, qy, qz, qw = self.yaw_to_quaternion(yaw)
        goal_msg.pose.pose.orientation.x = qx
        goal_msg.pose.pose.orientation.y = qy
        goal_msg.pose.pose.orientation.z = qz
        goal_msg.pose.pose.orientation.w = qw
        
        self.get_logger().info(f'导航到目标点: x={x}, y={y}, yaw={yaw}')
        
        # 发送目标
        send_goal_future = self.nav_to_pose_client.send_goal_async(
            goal_msg, 
            feedback_callback=self.feedback_callback
        )
        send_goal_future.add_done_callback(self.goal_response_callback)

    def goal_response_callback(self, future):
        """目标响应回调"""
        goal_handle = future.result()
        if not goal_handle.accepted:
            self.get_logger().info('目标被拒绝')
            return
            
        self.get_logger().info('目标已被接受，正在导航...')
        
        # 获取结果
        get_result_future = goal_handle.get_result_async()
        get_result_future.add_done_callback(self.get_result_callback)

    def get_result_callback(self, future):
        """获取结果回调"""
        result = future.result().result
        status = future.result().status
        
        if status == GoalStatus.STATUS_SUCCEEDED:
            self.get_logger().info('导航成功完成!')
        else:
            self.get_logger().info(f'导航失败，状态: {status}')

    def feedback_callback(self, feedback_msg):
        """反馈回调"""
        feedback = feedback_msg.feedback
        # 可以在这里添加反馈处理逻辑
        pass

    def yaw_to_quaternion(self, yaw):
        """将偏航角转换为四元数"""
        return 0.0, 0.0, math.sin(yaw / 2), math.cos(yaw / 2)

    def interactive_navigation(self):
        """交互式导航模式"""
        try:
            while rclpy.ok():
                print("\n=== 输入目标位置 ===")
                try:
                    x = float(input("请输入x坐标: "))
                    y = float(input("请输入y坐标: "))
                    yaw = float(input("请输入偏航角yaw(弧度): "))
                    
                    self.navigate_to_pose(x, y, yaw)
                    
                    continue_nav = input("是否继续导航? (y/n): ").lower()
                    if continue_nav != 'y':
                        break
                        
                except ValueError:
                    print("错误: 请输入有效的数字")
                except KeyboardInterrupt:
                    break
                    
        except KeyboardInterrupt:
            self.get_logger().info('导航被用户中断')

def main(args=None):
    rclpy.init(args=args)
    
    navigation_node = NavigationNode()
    
    # 在新线程中运行交互式导航
    nav_thread = threading.Thread(target=navigation_node.interactive_navigation)
    nav_thread.start()
    
    try:
        rclpy.spin(navigation_node)
    except KeyboardInterrupt:
        pass
    finally:
        navigation_node.destroy_node()
        rclpy.shutdown()
        nav_thread.join()

if __name__ == '__main__':
    main()