# DASE7503 组项目 — 自主搬运机器人

HKU 2026–27 Semester A · DASE7503 Robotics Systems Integration 组项目。
树莓派 5 上跑感知与任务层（相机 / 二维码 / 雷达 / 比赛状态机 / Nav2），ESP32 跑 micro-ROS 底盘固件，
比赛流程 **START → 依次经过货架（读二维码）→ 放置区 → END**。

截止：**2026-11-26 17:00**。仓库里现在放的是可运行的软件与配置，报告与 CAD 另存。

## 目录

| 路径 | 内容 |
| --- | --- |
| `pi_stack/` | 树莓派 5 上的 ROS 2 Jazzy 软件：`qr_scanner`、`robot_bringup`、`game_manager`、`robot_navigation`（SLAM / Nav2 配置与地图）、`tools/`；一条 `start_match.sh` 起全场 |
| `chassis_firmware/` | ESP-IDF + micro-ROS 底盘固件：`cmd_vel` → 电机 PID、编码器 `odom` 回传、舵机；`src/car_ws/keyboard_teleop` 是键盘遥控节点 |
| `ros2_ws/` | ROS 2 工作区：`car_nav`（导航节点）、`my_amcl_localization`、`oradar_ros`（MS200 雷达驱动，包名 `oradar_lidar`）、`qr_scanner`、`team_proj2_pkg`（URDF + 四轮控制器 + 键盘遥控） |
| `models/7503car/` | SolidWorks 导出的 URDF 包（`urdf/`、`meshes/`、`textures/`），`display.launch` / `gazebo.launch` 可直接看车 |
| `assets/qr_samples/` | 两轮比赛的二维码样张（first_round 5 张、second_round 6 张），用来现场调试识别 |
| `docs/` | 搭建与操作记录：`qr_scanning.md`、`build_and_run.md`、`setup/`（树莓派 ROS 安装、SSH 公钥） |

## 硬件

| 项目 | 型号 | 说明 |
| --- | --- | --- |
| 上位机 | Raspberry Pi 5, 8 GB | Ubuntu 24.04 (noble), aarch64, ROS 2 Jazzy |
| 相机 | Camera Module 3 (IMX708) | 640×480 @ 15 fps，XRGB8888 → `bgra8` |
| 雷达 | MS200 (OrADAR) | 360°, 10 Hz, `/dev/oradar`, 230400 baud |
| 底盘 | ESP32 + micro-ROS | 4 轮驱动 + 编码器，WiFi 接入 DDS（micro-ROS agent, UDP 8090） |

树莓派主机名 `rsi-bd-11`（192.168.3.18）。

## 三条入口

**1) 树莓派：一条命令跑比赛**（详见 [`pi_stack/README.md`](pi_stack/README.md)）

```bash
# 桌面排练：传感器 + fake_base + Nav2 desk_mode + game_manager
~/lidar_ros_ws/install/robot_bringup/lib/robot_bringup/start_match.sh --desk

# 现场：传感器 + 已建图场地上的 Nav2 + game_manager
~/lidar_ros_ws/install/robot_bringup/lib/robot_bringup/start_match.sh --map ~/field_map.yaml

# 现场带底盘：同时把 /odom 广播为 TF
~/lidar_ros_ws/install/robot_bringup/lib/robot_bringup/start_match.sh --map ~/field_map.yaml --odom-tf
```

**2) ROS 2 工作区：编译导航 / 雷达 / QR**

```bash
cd ros2_ws
colcon build --symlink-install
source install/setup.bash
ros2 launch oradar_lidar ms200_scan_view.launch.py     # 雷达点云
ros2 launch team_proj2_pkg rviz_launch.py              # 加载 URDF
ros2 run car_nav nav_node                              # 输入 (x, y, yaw) 导航
```

**3) 底盘固件：烧 ESP32**

```bash
source ~/esp/esp-idf/export.sh
cd chassis_firmware
idf.py menuconfig      # micro-ROS Agent IP/Port、WiFi SSID/密码、ROS domain id
idf.py build flash monitor
```

先在电脑端点起 agent，固件上电后才会连上：

```bash
docker run -it --rm -v /dev:/dev -v /dev/shm:/dev/shm --privileged --net=host \
  microros/micro-ros-agent:jazzy udp4 --port 8090 -v4
```

## 文档

- [`pi_stack/README.md`](pi_stack/README.md) — 树莓派栈的完整说明（话题、TF、看门狗、故障排查）
- [`pi_stack/docs/field_day_checklist.md`](pi_stack/docs/field_day_checklist.md) — 建图、相机/QR 几何、货架标定、验收测试
- [`pi_stack/docs/rehearsal_2026-10-18.md`](pi_stack/docs/rehearsal_2026-10-18.md) — 第一次端到端排练的时间线与限制
- [`pi_stack/docs/micro_ros_contract.md`](pi_stack/docs/micro_ros_contract.md) — ESP32 固件需要提供的话题与 TF 契约
- [`docs/qr_scanning.md`](docs/qr_scanning.md) — 树莓派上跑二维码识别的环境步骤
- [`docs/build_and_run.md`](docs/build_and_run.md) — 底盘 / 导航 / 雷达 / 遥操作的原版操作记录
- [`docs/setup/`](docs/setup/) — ROS 2 Jazzy 安装、SSH 公钥安装的命令清单

## 没放进仓库的东西

- 课程任务书 PDF（`1.~4.` 四份）与 SolidWorks 原始模型（约 30 MB 的 `modeling/`，含装配体、PCB、雷达 STEP）
  ——体积大且与课程材料版权相关，留在本地 `projects/`，需要时再单独放私有目录。
- `build/` `install/` `log/`、`__pycache__`、`.scratch/`：已在 `.gitignore` 里排除。

## 状态

- 2026-10-18：首次端到端排练跑通（传感器 + fake_base + Nav2 + game_manager），见排练记录。
- 树莓派侧待办：USB 重枚举后的雷达恢复、现场建图与货架标定。
