from setuptools import find_packages, setup

package_name = 'team_proj2_pkg'

setup(
    name=package_name,
    version='0.0.0',
    packages=find_packages(exclude=['test']),
data_files=[
        ('share/ament_index/resource_index/packages',
            ['resource/' + package_name]),
        ('share/' + package_name, ['package.xml']),
        #安装 launch 目录下所有 .py 启动文件
        ('share/' + package_name + '/launch', [
            'team_proj2_pkg/launch/rviz_launch.py',  # RViz 启动文件
        ]),
        #同时安装 config 目录（存放 rviz_config.rviz）
        ('share/' + package_name + '/config', [
            'team_proj2_pkg/config/rviz_config.rviz'  # RViz 配置文件
        ]),
        #安装 urdf 目录（存放 URDF 文件）
        ('share/' + package_name + '/urdf', [
            'team_proj2_pkg/urdf/7503car.urdf'  # URDF 模型文件
        ]),
        #安装 meshes 目录（存放 STL 模型文件）
        ('share/' + package_name + '/meshes', [
            'team_proj2_pkg/meshes/base_link.STL',
            'team_proj2_pkg/meshes/Link_wheel_FL.STL',
            'team_proj2_pkg/meshes/Link_wheel_FR.STL',
            'team_proj2_pkg/meshes/Link_wheel_RL.STL',
            'team_proj2_pkg/meshes/Link_wheel_RR.STL'
        ])
    ],
    install_requires=['setuptools'],
    zip_safe=True,
    maintainer='hopper',
    maintainer_email='hopper@todo.todo',
    description='TODO: Package description',
    license='Apache-2.0',
    extras_require={
        'test': [
            'pytest',
        ],
    },
    entry_points={
        'console_scripts': [
            'mecanum_pynput_node_exe = team_proj2_pkg.mecanum_pynput_node:main',
            'four_wheel_controller_exe = team_proj2_pkg.four_wheel_controller:main',
        ],
    },
)
