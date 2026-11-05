## Group4 Build and Start car node, navigation node and lidar node
### 1. Build car node and navigation node
```bash
cd ros2_ws
colcon build --packages-select team_proj2_pkg car_nav
source install/setup.bash
```
#### start car_node, loading URDF, publish topic "robot_description"
```bash
ros2 launch team_proj2_pkg rviz_launch.py
```

####  Start navigation node (test in turtlebot3 simulation env)
```bash
# start the gazebo world of turtlebot3
export TURTLEBOT3_MODEL=burger
ros2 launch turtlebot3_gazebo turtlebot3_world.launch.py 
# open a new terminal, start Nav2 service 
ros2 launch turtlebot3_navigation2 navigation2.launch.py
# init the position of robot in Rviz use 2D Pose
# open a new terminal
cd ros2_ws
source install/setup.bash
# Run navigation node, input the x,y,yaw of destination in terminal interface
ros2 run car_nav navigation_node
```

### 2. Build oradar_ros package to use lidar
```bash
cd ros2_ws
colcon build --symlink-install
source install/setup.bash
sudo cp oradar.rules /etc/udev/rules.d/
ros2 launch oradar_lidar ms200_scan_view.launch.py
# The cloud points shows in Rviz
```

## Group4 Chassis drive and remote control
### 1. Environment setup
```bash
# Complete the environment setup for Chapter1 "microROS control board development environment" according to the website https://www.yahboom.net/study/MicroROS-Board
```
### 2. Compile, download and flash firmware
```bash
# Use a Type-C data cable to connect the virtual machine/computer and the microROS control board. If the system pops up, choose to connect to the virtual machine.
# Activate the ESP-IDF development environment. Note that every time you open a new terminal, you need to activate the ESP-IDF development environment before compiling the firmware.
source ~/esp/esp-idf/export.sh
# Enter the project directory
cd chassis_firmware
# Open the ESP-IDF configuration tool.
idf.py menuconfig
# Open micro-ROS Settings, fill in the IP address of the agent host in micro-ROS Agent IP, and fill in the port number of the agent host in micro-ROS Agent Port.
# Open micro-ROS Settings->WiFi Configuration in sequence, and fill in your own WiFi name and password in the WiFi SSID and WiFi Password fields.
# Open the micro-ROS example-app settings. The Ros domain id of the micro-ROS defaults to 20. 
# After modification, press S to save, and then press Q to exit the configuration tool.
# Compile, flash, and open the serial port simulator.
idf.py build flash monitor
# If you need to exit the serial port simulator, press Ctrl+].
```
### 3 Turn on the agent
```bash
# After powering on, ESP32 tries to connect to the WiFi hotspot, and then tries to connect to the proxy IP and port.
# If the agent is not turned on in the virtual machine/computer terminal, please enter the following command to turn on the agent.
docker run -it --rm -v /dev:/dev -v /dev/shm:/dev/shm --privileged --net=host microros/micro-ros-agent:jazzy udp4 --port 8090 -v4
```
### 4 Enable keyboard remote control
```bash
# Run the following command to enable keyboard remote control
cd chassis_firmware/src/car_ws
colcon build
source install/setup.bash
ros2 run keyboard_teleop keyboard_node
```

