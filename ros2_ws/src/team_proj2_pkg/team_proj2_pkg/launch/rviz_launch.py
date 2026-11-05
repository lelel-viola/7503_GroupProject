from launch import LaunchDescription,LaunchContext
from launch.actions import DeclareLaunchArgument
from launch.conditions import IfCondition
from launch.substitutions import LaunchConfiguration, PathJoinSubstitution
from launch_ros.actions import Node
from launch_ros.substitutions import FindPackageShare



def generate_launch_description():
    # 定义启动参数（可选：是否自动加载RViz配置）
    use_rviz_config = LaunchConfiguration('use_rviz_config')
    
    # 找到你的URDF包的路径
    package_name = 'team_proj2_pkg'  # 替换为你的ROS包名称
    urdf_package_path = FindPackageShare(package=package_name)
    
    # URDF文件路径
    urdf_file_path = PathJoinSubstitution(
        [urdf_package_path, 'urdf', '7503car.urdf']  # 假设URDF文件在包的urdf目录下
    )
    
    # RViz配置文件路径（可选，若有自定义配置）
    rviz_config_path = PathJoinSubstitution(
        [urdf_package_path, 'config', 'rviz_config.rviz']  # 配置文件存放路径
    )

    return LaunchDescription([
        # 声明启动参数：是否使用自定义RViz配置
        DeclareLaunchArgument(
            'use_rviz_config',
            default_value='true',
            description='Whether to load the custom RViz config file'
        ),
        
        # 启动robot_state_publisher节点（用于发布URDF模型和TF变换）
        Node(
            package='robot_state_publisher',
            executable='robot_state_publisher',
            name='robot_state_publisher',
            output='screen',
            parameters=[{'robot_description': open(urdf_file_path.perform(LaunchContext()), 'r').read()}]
        ),
        
        # 启动joint_state_publisher_gui节点（可选，用于交互式控制车轮关节）
        # Node(
        #     package='joint_state_publisher_gui',
        #     executable='joint_state_publisher_gui',
        #     name='joint_state_publisher_gui',
        #     output='screen'
        # ),
        
        # 启动RViz2
        Node(
            package='rviz2',
            executable='rviz2',
            name='rviz2',
            output='screen',
            # 若使用自定义配置，则加载；否则不加载
            arguments=[['-d', rviz_config_path]] if IfCondition(use_rviz_config) else []
        ),
        
        Node(
            package='team_proj2_pkg',
            executable='mecanum_pynput_node_exe',
            name='mecanum_pynput_node_exe',
            output='screen',
        ),
        
        Node(
            package='team_proj2_pkg',
            executable='four_wheel_controller_exe',
            name='four_wheel_controller_exe',
            output='screen',
        ),
    ])
