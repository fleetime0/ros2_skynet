FROM ros:jazzy-ros-base

RUN apt update && apt install -y \
    python3-colcon-common-extensions \
    python3-rosdep \
    wget \
    libusb-1.0-0-dev \
    libgflags-dev \
    nlohmann-json3-dev \
    ros-jazzy-image-transport \
    ros-jazzy-image-publisher \
    && rm -rf /var/lib/apt/lists/*

RUN wget -c https://github.com/google/glog/archive/refs/tags/v0.6.0.tar.gz -O glog-0.6.0.tar.gz && \
    tar -xzvf glog-0.6.0.tar.gz && \
    cd glog-0.6.0 && \
    mkdir build && cd build && \
    cmake .. && make -j$(nproc) && make install && \
    ldconfig && \
    cd / && rm -rf glog-0.6.0*

RUN wget -c https://github.com/Neargye/magic_enum/archive/refs/tags/v0.9.0.tar.gz -O magic_enum-0.9.0.tar.gz && \
    tar -xzvf magic_enum-0.9.0.tar.gz && \
    cd magic_enum-0.9.0 && \
    mkdir build && cd build && \
    cmake .. && make -j$(nproc) && make install && \
    ldconfig && \
    cd / && rm -rf magic_enum-0.9.0*

RUN git clone https://github.com/libuvc/libuvc.git && \
    cd libuvc && \
    mkdir build && cd build && \
    cmake .. && make -j$(nproc) && make install && \
    ldconfig && \
    cd / && rm -rf libuvc

WORKDIR /ros2_ws
COPY ./src ./src

RUN apt update
RUN . /opt/ros/jazzy/setup.sh && \
    rosdep update && rosdep install --from-paths src --ignore-src -r -y && \
    colcon build

RUN echo 'source /opt/ros/jazzy/setup.bash' >> /etc/bash.bashrc && \
    echo '[ -f /ros2_ws/install/setup.bash ] && source /ros2_ws/install/setup.bash' >> /etc/bash.bashrc

CMD [ "bash" ]
