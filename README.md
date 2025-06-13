# ros2_skynet
基于树莓派5的ros2智能小车。

## 硬件和外设

本项目所使用的硬件以及外设如下：

- 树莓派5
- astra pro plus深度摄像头
- 思岚a1激光雷达
- 轮趣C100摄像头（仅用作测试USB摄像头）

## 安装

### docker部署

```bash
git clone https://github.com/fleetime0/ros2_skynet.git
cd ros2_skynet
docker compose up -d
```

### 创建外设别名

由于目前有深度摄像头以及RGB摄像头，为了方便管理，可以使用udev给摄像头创建别名。

```bash
sudo cp -r scripts/* /etc/udev/rules.d/
sudo udevadm control --reload-rules && sudo udevadm trigger
```

## 运行

以下操作均需要进入docker容器中。

```bash
docker exec -it ros2_skynet bash
```

### 浏览器预览摄像头画面

目前轮趣C100摄像头和astra pro plus深度摄像头存在冲突，只能通过在web_cam.launch.py屏蔽某一摄像头节点来运行。并且在接上轮趣C100摄像头后，会导致astra pro plus的rgb图变成轮趣C100摄像头的画面。

> 修改后，记得再次colcon build。

进入容器后，输入：

```bash
ros2 launch skynet_bringup web_cam.launch.py
```

在同一局域网下的其他设备上，就可以在浏览器预览摄像头画面了。

轮趣C100：

```bash
http://10.42.0.1:8080/stream?topic=/camera1/image_raw
```

astra pro plus：

```bash
http://10.42.0.1:8080/stream?topic=/camera/color/image_raw&qos_profile=sensor_data
```

## 版本计划

### v0.0.1：

1. 支持在ros2容器中运行项目。
2. 支持在浏览器浏览USB摄像头以及astra pro plus摄像头画面。
3. 支持在浏览器浏览astra pro plus的深度图以及IR图。
4. 支持在浏览器低延迟浏览摄像头画面。
