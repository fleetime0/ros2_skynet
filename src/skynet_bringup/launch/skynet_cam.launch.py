import os
import yaml
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, OpaqueFunction
from launch.substitutions import LaunchConfiguration
from ament_index_python.packages import get_package_share_directory
from launch_ros.actions import ComposableNodeContainer
from launch_ros.descriptions import ComposableNode

def launch_setup(context, *args, **kwargs):
    image_topic = LaunchConfiguration("image_topic").perform(context)
    qos_reliability = LaunchConfiguration("qos_reliability").perform(context)

    nodes = []

    nodes.append(
        ComposableNode(
            package='webrtc_whep',
            plugin='webrtc_whep::WebrtcWhep',
            name='webrtc_whep_node',
            parameters=[{
                "image_topic": image_topic,
                "qos_reliability": qos_reliability
            }],
            extra_arguments=[{"use_intra_process_comms": True}]
        )
    )

    params_path = os.path.join(
        get_package_share_directory("astra_camera"),
        "params", "astra_pro_plus_params.yaml"
    )
    with open(params_path, 'r') as f:
        astra_params = yaml.safe_load(f)

    nodes += [
        ComposableNode(
            package='astra_camera',
            plugin='astra_camera::OBCameraNodeFactory',
            name='camera',
            namespace='camera',
            parameters=[astra_params],
            extra_arguments=[{"use_intra_process_comms": True}]
        ),
        ComposableNode(
            package='astra_camera',
            plugin='astra_camera::PointCloudXyzNode',
            name='point_cloud_xyz',
            namespace='camera',
            extra_arguments=[{"use_intra_process_comms": True}]
        ),
        ComposableNode(
            package='astra_camera',
            plugin='astra_camera::PointCloudXyzrgbNode',
            name='point_cloud_xyzrgb',
            namespace='camera',
            extra_arguments=[{"use_intra_process_comms": True}]
        ),
    ]

    container = ComposableNodeContainer(
        name='skynet_cam_container',
        namespace='',
        package='rclcpp_components',
        executable='component_container',
        composable_node_descriptions=nodes,
        output='screen'
    )

    return [container]


def generate_launch_description():
    return LaunchDescription([
        DeclareLaunchArgument("image_topic", default_value="/camera/color/image_raw", description="Image topic for webrtc_whep"),
        DeclareLaunchArgument("qos_reliability", default_value="best_effort", description="QoS reliability: best_effort or reliable"),
        OpaqueFunction(function=launch_setup)
    ])
