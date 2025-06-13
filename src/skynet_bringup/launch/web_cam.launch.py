import os
from launch import LaunchDescription
from launch.actions import IncludeLaunchDescription, DeclareLaunchArgument
from launch.launch_description_sources import PythonLaunchDescriptionSource
from ament_index_python.packages import get_package_share_directory
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def generate_launch_description():
    ld = LaunchDescription()

    ld.add_action(
        DeclareLaunchArgument(
            "address", default_value="0.0.0.0", description="HTTP server address"
        )
    )
    ld.add_action(
        DeclareLaunchArgument(
            "port", default_value="8080", description="HTTP server port"
        )
    )

    # usb_camera_launch_dir = os.path.join(
    #     get_package_share_directory("skynet_bringup"), "launch", "camera.launch.py"
    # )
    # ld.add_action(
    #     IncludeLaunchDescription(PythonLaunchDescriptionSource(usb_camera_launch_dir))
    # )

    astra_pro_plus_launch_dir = os.path.join(
        get_package_share_directory("astra_camera"),
        "launch",
        "astra_pro_plus.launch.py",
    )
    ld.add_action(
        IncludeLaunchDescription(PythonLaunchDescriptionSource(astra_pro_plus_launch_dir))
    )

    address = LaunchConfiguration("address")
    port = LaunchConfiguration("port")

    web_video_server_node = Node(
        package="web_video_server",
        executable="web_video_server",
        name="web_video_server",
        parameters=[{"address": address, "port": port}],
        output="screen",
    )
    ld.add_action(web_video_server_node)

    return ld
