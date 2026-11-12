# 运行指南（修改后的完整版）

适用：树莓派 5 + ROS 2 Jazzy（你们现有环境），ESP32 底盘固件。
改了什么、为什么：见 [`pi_stack/docs/FIX_NOTES.md`](pi_stack/docs/FIX_NOTES.md)。

## 0. 先看这个：哪些验证过，哪些没有

| 内容 | 状态 |
| --- | --- |
| 状态机按 A→B→C→D 取货、钻进货架、抬起、送到槽位、放下、倒出 | ✅ 在假世界里测过 7 个场景（真代码，假 ROS/Nav2/相机） |
| 二维码解码 + 距离/偏移（`solvePnP`） | ✅ 合成图测过，0.45 m 内误差 < 1 cm（5 cm 的码，640x480） |
| 货架坐标、左右、二维码边长、升降方向、车宽 vs 货架腿间距 | ⚠ **估计值，必须现场核对**（第 3 节） |
| 固件改动（超时、时间同步） | ⚠ **没编译过**，请你们编译烧录（第 4 节） |
| 真实 Nav2 / AMCL / 雷达 / 相机 / 升降 | ⚠ 没在真机上跑过 |

## 1. 装到树莓派

```bash
cd ~
unzip ~/Downloads/7503_GroupProject-fixed.zip            # 得到 7503_GroupProject-main/
# 把四个包覆盖到工作区（如果 ~/lidar_ros_ws/src 里是指向仓库的软链接，直接覆盖仓库目录即可）
rsync -a ~/7503_GroupProject-main/pi_stack/src/ ~/lidar_ros_ws/src/

source /opt/ros/jazzy/setup.bash
cd ~/lidar_ros_ws
colcon build --symlink-install --packages-select qr_scanner robot_bringup game_manager robot_navigation
source install/setup.bash
```

没有新增系统依赖（`tf2_ros`、OpenCV、numpy 都已在 Jazzy 桌面版里）。

> 只想用补丁（`7503_fixes.patch`，`cd 仓库上一级 && patch -p1 < 7503_fixes.patch`）而不是整包的话：补丁带不了二进制的
> `field.pgm`，打完补丁后在 `pi_stack` 里运行一次 `python3 tools/make_field_map.py`（只画墙的新地图）和
> `python3 tools/make_game_params.py` 即可。

## 2. 先在桌面上跑一遍（不接底盘）

```bash
~/lidar_ros_ws/install/robot_bringup/lib/robot_bringup/start_match.sh --desk --supervisor
```

`--desk` 用 `fake_base` 代替底盘，`--supervisor` 自动注入 START / RACKx / END。你应该看到：

* 状态依次 `DRIVE_TO_RACK → WAIT_RACK_QR → DOCK_RACK → LIFT_UP → DRIVE_TO_DEST → DOCK_SLOT → LIFT_DOWN → EXIT_SLOT`，重复 4 次；
* 日志里 `no QR geometry ...; docking on the nominal rack centre`：桌面上没有真的二维码几何，所以对位走"名义中心"。这是正常的；
* 最后 `score 20`。

对照旧行为：`start_match.sh --desk --supervisor --no-docking`（在接近点直接抬升）。

## 3. 上机前必做的 3 件事

### 3.1 量货架，重新生成参数（最重要）
货架是 140 x 350 mm 的龙门架，二维码朝起点一侧。按 `pi_stack/docs/field_day_checklist.md` 第 5 节：
先确认"站在起点区面向目的区，A、D 在左手边"，量 4 个货架中心，然后

```bash
cd ~/7503_GroupProject-main/pi_stack
python3 tools/make_game_params.py --centres "A:1.130,0.525;B:1.830,1.385;C:1.320,1.695;D:1.920,0.385"
#                                  ↑ 换成你们的实测值；不带 --centres 就用脚本里的估计值
# 它会写 src/game_manager/config/game_params.yaml，并打印车体与墙/货架的间隙表（出现 COLLIDES 要先处理）
rsync -a src/game_manager/config/ ~/lidar_ros_ws/src/game_manager/config/    # 同步到工作区（用了软链接就不需要）
```

### 3.2 量二维码边长，校准距离
距离读数与 `--qr-side` 成正比。用尺子量打印出来的**黑白方块部分**（不含白边）。

```bash
~/lidar_ros_ws/install/robot_bringup/lib/robot_bringup/start_match.sh --qr-side 0.05   # 换成你量的值
# 另一个终端：把标签立在相机正前方 0.30 m（尺子量相机镜头到标签表面）
ros2 topic echo /qr_detection --field data
# 打印的是 JSON，看里面的 "fwd"（相机到标签的水平距离，m）和 "left"（偏左多少，m）
```
`fwd` 应约 0.30。偏小就把 `--qr-side` 乘以（真实距离 / 读数），偏大则相反。再检查
`ros2 topic echo /camera/camera_info --once`：`k[0]`（焦距，像素，640 宽时约 500–600）不应是 0。
`qr_node` 只在收不到 CameraInfo 时才用 `--hfov`（默认 1.0 rad）估算焦距，正常不用管。

这个码在 640x480 下 **0.45 m 内**能稳定读（5 cm 的码）；码更小就要把 `make_game_params.py` 里的 `APPROACH_RANGE` 调小。

### 3.3 核对升降方向
台架上（轮子悬空、货架不放）：

```bash
ros2 topic pub --once /lift_cmd std_msgs/msg/String "{data: up}"
ros2 topic pub --once /lift_cmd std_msgs/msg/String "{data: down}"
```
`servo_s1` 默认 up = 90°、down = 50°（来自键盘遥控的 z / c 键）。方向反了或角度不对，改
`pi_stack/src/robot_bringup/scripts/lift_bridge.py` 里 `s1_up` / `s1_down` 的默认值（如果升降用的是 S2，同样改 `s2_*`，`-1000` 表示不动）。
还要确认：`up` 之后货架腿离地至少 2 cm（规则：货架不能碰地面）。

## 4. 固件：编译、烧录、验证

```bash
source ~/esp/esp-idf/export.sh
cd ~/7503_GroupProject-main/chassis_firmware
idf.py build flash monitor
```
agent（先开，固件上电才会连）：

```bash
docker run -it --rm -v /dev:/dev -v /dev/shm:/dev/shm --privileged --net=host \
  microros/micro-ros-agent:jazzy udp4 --port 8090 -v4
```

验证两个改动：

```bash
ros2 topic echo /odom_raw --once        # header.stamp.sec 应是 17xxxxxxxx（当前时间），不是几十
ros2 topic pub -r 10 /cmd_vel geometry_msgs/msg/Twist "{linear: {x: 0.1}}"   # 轮子转
# Ctrl-C 停掉发布：轮子应在 0.5 s 内停（monitor 里会打印 "cmd_vel timeout"）
```
如果 `/odom_raw` 的时间戳仍然不对，不用担心：`odom_to_tf` 默认用树莓派时钟打戳（`use_pi_clock:=true`）。

## 5. 上场运行

1. 机器人放在**起点区中心** (x = 2.72, y = 1.00)，**车头朝目的区**（AMCL 初始位姿就是这个，写在 `nav2_params.yaml` 里）。
2. 启动：

```bash
~/lidar_ros_ws/install/robot_bringup/lib/robot_bringup/start_match.sh \
    --odom-tf --map ~/field_map.yaml --qr-side 0.05
#   --odom-tf   : /odom_raw → /odom + TF，同时启动 lift_bridge（/lift_cmd → servo_s1）
#   --map       : 实测的 Cartographer 地图；不带就用合成图（只有墙）
#   --qr-side   : 换成你量的值
```
3. 起跑前逐项检查（另开终端）：

```bash
ros2 topic hz /odom                       # ≥ 15 Hz
ros2 run tf2_ros tf2_echo odom base_link  # 有值
ros2 run tf2_ros tf2_echo map base_link   # 有值，且约等于 (2.72, 1.00)
ros2 topic hz /scan                       # ≈ 10 Hz
ros2 topic echo /qr_detected                 # 起点前能读到 START
```
4. 扫到 `START`，计时开始，机器人自动按 **A → B → C → D** 取货、送货。
   `ros2 topic echo /game_state`、`/game_score` 可以看进度。

## 6. 常用参数（`game_params.yaml` 由脚本生成；想长期改请改 `tools/make_game_params.py`）

| 参数 | 默认 | 说明 |
| --- | --- | --- |
| `use_docking` | true | false = 旧行为：在接近点直接抬升 |
| `stop_range` | 0.30 | 伺服停在标签前多远（相机到标签，m） |
| `v_servo` / `v_line` / `v_exit` | 0.08 / 0.10 / 0.12 | 对准 / 进入货架 / 倒出的速度，m/s |
| `lift_time` | 1.5 | 等升降到位的秒数，按实际调 |
| `lift_offset` | 0.0 | 升降中心在 base_link 前方多远（m）；不为 0 要填 |
| `dock_timeout` | 25 | 单次对位超时，超时放弃该货架 |
| `servo_acquire_timeout` | 2.5 | 二维码几何一直没来：没有几何功能 → 名义中心；有几何功能但看不到该货架 → 放弃 |
| `rack_scan_sweep_w` | 0.35 | 接近点看不到标签时左右扫视的角速度（0 关闭） |
| `mission_timeout` | 180 | 比赛时长 |

节奏：假世界里 4 个货架在约 28 / 72 / 116 / 161 s 送达，余量约 20 s。真车慢的话先调高 `v_servo`、`v_line`，再考虑 DWB 的 `max_vel_x`（现在 0.25 m/s）。

## 7. 出问题时

| 现象 | 多半是 |
| --- | --- |
| Nav2 一直起不来 / 报 TF 外推 | `/odom`、`odom→base_link` 没有：`ros2 topic hz /odom_raw` 看固件是否在发；`--odom-tf` 是否加了 |
| 到了货架前读不到二维码，来回扫视后放弃 | 货架坐标不对（先做 3.1）；或 `--qr-side` 不对；或码太小，减小 `APPROACH_RANGE` |
| `rack X: its QR plate is not in view although the scanner works` | 机器人不在该货架前（坐标错、或缓存的是别的货架的码）；对位按设计放弃该货架，不盲钻 |
| 钻货架时蹭到腿 | 车宽 vs 腿内净宽太紧：降 `v_servo`；检查 `plate_to_center`（0.074）、`camera_forward`（0.12）是否与实物一致 |
| 抬起后 Nav2 不动 / 碰撞监视器冻结 | 货架腿被当成障碍：确认 `nav2_params.yaml` 里 `obstacle_min_range` / `laser_min_range` = 0.30 和碰撞多边形 ±0.13 都已生效（重新 build） |
| 抬起货架但没离地 | `s1_up` 角度不够；`lift_time` 太短 |
| 小车丢了 WiFi 还在跑 | 固件还是旧的（没有 `cmd_vel` 超时）：重新烧录 |

## 8. 测试（不需要 ROS）

```bash
cd ~/7503_GroupProject-main/pi_stack
python3 tests/test_game_manager_sim.py     # 真 game_manager 在假世界里跑 7 个场景（-v 看详细日志）
python3 tests/test_qr_geometry.py          # 二维码解码 + 位置精度
python3 tests/test_nodes_smoke.py          # qr_node / odom_to_tf / lift_bridge 的真实回调（假 ROS）
python3 tools/make_game_params.py --check  # 只检查布局的碰撞间隙
```
