from launch import LaunchDescription
from launch_ros.actions import Node

def generate_launch_description():
    return LaunchDescription([
        Node(
            package='joy',
            executable='joy_node',
            name='joy_node',
            output='screen'
        ),
        Node(
            package='skynet_ctrl',
            executable='skynet_joy',
            name='skynet_joy',
            parameters=[{'xspeed_limit': 1.0, 
                         'yspeed_limit': 0.0, 
                         'angular_speed_limit': 5.0,
                         'cmd_vel_hz': 20}],
            output='screen'
        )
    ])