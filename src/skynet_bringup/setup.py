import os
from glob import glob
from setuptools import find_packages, setup

package_name = 'skynet_bringup'

setup(
    name=package_name,
    version='0.0.1',
    packages=find_packages(exclude=['test']),
    data_files=[
        ('share/ament_index/resource_index/packages',
            ['resource/' + package_name]),
        ('share/' + package_name, ['package.xml']),
        ('share/' + package_name + '/launch', glob(os.path.join('launch', '*.py'))),
        ('share/' + package_name + '/usb_cam_params', glob(os.path.join('usb_cam_params', '*.yaml')))
    ],
    install_requires=['setuptools'],
    zip_safe=True,
    maintainer='fleetime',
    maintainer_email='3176226599@qq.com',
    description='Provides launch files for full system bringup and component-level testing in the Skynet robot project.',
    license='MIT',
    entry_points={
        'console_scripts': [
        ],
    },
)
