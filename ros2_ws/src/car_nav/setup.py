import os
import glob
from setuptools import find_packages, setup

package_name = 'car_nav'  # 包名是car_nav

setup(
    name=package_name,
    version='0.0.0',
    packages=find_packages(exclude=['test']),
    data_files=[
        ('share/ament_index/resource_index/packages',
            ['resource/' + package_name]),
        ('share/' + package_name, ['package.xml']),
        (os.path.join('share', package_name, 'launch'), glob.glob(os.path.join('launch', '*.py'))),
    ],
    install_requires=['setuptools'],
    zip_safe=True,
    maintainer='wzx',
    maintainer_email='wzx@todo.todo',
    description='A client package to send goals to Nav2',
    license='Apache-2.0',  # 建议明确许可证
    extras_require={
        'test': [
            'pytest',
        ],
    },
    entry_points={
        'console_scripts': [
            # 修正模块路径：包名是car_nav，因此入口点应为 car_nav.xxx:main
            'navigation_node = car_nav.nav_node:main',
        ],
    },
)
