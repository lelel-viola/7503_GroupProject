from launch import LaunchDescription
from launch_ros.actions import Node
from ament_index_python.packages import get_package_share_directory
import os

def generate_launch_description():
    # 路径定义
    pkg_dir = get_package_share_directory('my_amcl_localization')
    turtlebot3_description_dir = get_package_share_directory('turtlebot3_description')
    urdf_file = os.path.join(turtlebot3_description_dir, 'urdf', 'turtlebot3_burger.urdf')
    amcl_params_path = os.path.join(pkg_dir, 'config', 'amcl_params.yaml')

    return LaunchDescription([
        # 1. AMCL 核心节点（订阅 Gazebo 的 /scan 和 /map 话题）
        Node(
            package='nav2_amcl',
            executable='amcl',
            name='amcl',
            parameters=[amcl_params_path],
            output='screen'
        ),

        # 2. 生命周期管理器（激活 AMCL，无需激活地图服务器）
        Node(
            package='nav2_lifecycle_manager',
            executable='lifecycle_manager',
            name='lifecycle_manager_amcl',
            output='screen',
            parameters=[{
                'node_names': ['amcl'],  # 仅激活 AMCL
                'autostart': True,
                'use_sim_time': True     # 仿真时间
            }]
        ),

        # 3. 机器人状态发布者（与 Gazebo 仿真模型一致）
        Node(
            package='robot_state_publisher',
            executable='robot_state_publisher',
            name='robot_state_publisher',
            arguments=[urdf_file],
            output='screen',
            parameters=[{'use_sim_time': True}]
        ),

        # 4. 静态 TF（map -> odom，仿真环境下可选，确保坐标系一致）
        Node(
            package='tf2_ros',
            executable='static_transform_publisher',
            name='static_tf_pub_odom_map',
            arguments=['0', '0', '0', '0', '0', '0', 'map', 'odom'],
        ),
    ])
