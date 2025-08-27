#include <cstdlib>
#include <vector>

#include "geometry_msgs/msg/twist.hpp"
#include "rclcpp/rclcpp.hpp"
#include "sensor_msgs/msg/joy.hpp"
#include "std_msgs/msg/bool.hpp"
#include "std_msgs/msg/int32.hpp"

class SkynetJoy : public rclcpp::Node {
public:
  SkynetJoy();

private:
  void cancel_nav();
  void cancel_buzzer();
  void cancel_light();
  double cancel_linear_gear();
  double cancel_angular_gear();
  void publish_twist();

  void callback(const sensor_msgs::msg::Joy::SharedPtr joy_data);

private:
  bool Joy_active_;
  rclcpp::Time cancel_nav_time_;
  rclcpp::Time cancel_light_time_;
  rclcpp::Time cancel_buzzer_time_;
  rclcpp::Time cancel_linear_gear_time_;
  rclcpp::Time cancel_angular_gear_time_;
  int rgb_light_index_;
  bool buzzer_active_;
  std::vector<double> linear_gears_;
  std::vector<double> angular_gears_;
  int cmd_vel_hz_;
  int linear_gear_index_, angular_gear_index_;
  double xspeed_limit_, yspeed_limit_, angular_speed_limit_;
  bool last_moved_;
  geometry_msgs::msg::Twist current_twist_;

  rclcpp::TimerBase::SharedPtr pub_cmd_vel_timer_;
  rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr pub_cmd_vel_;
  rclcpp::Publisher<std_msgs::msg::Bool>::SharedPtr pub_buzzer_;
  rclcpp::Publisher<std_msgs::msg::Bool>::SharedPtr pub_joy_state_;
  rclcpp::Publisher<std_msgs::msg::Int32>::SharedPtr pub_rgb_light_;

  rclcpp::Subscription<sensor_msgs::msg::Joy>::SharedPtr sub_joy_;
};

SkynetJoy::SkynetJoy()
    : rclcpp::Node("skynet_joy"), Joy_active_(false),
      cancel_nav_time_(this->now()), cancel_light_time_(this->now()),
      cancel_buzzer_time_(this->now()), cancel_linear_gear_time_(this->now()),
      cancel_angular_gear_time_(this->now()), rgb_light_index_(0),
      buzzer_active_(false), cmd_vel_hz_(0), linear_gear_index_(0), angular_gear_index_(0),
      xspeed_limit_(0), yspeed_limit_(0), angular_speed_limit_(0),
      last_moved_(false)
{
  linear_gears_ = {1.0, 2.0 / 3, 1.0 / 3};
  angular_gears_ = {1.0, 3.0 / 4, 1.0 / 2, 1.0 / 4};

  this->declare_parameter<double>("xspeed_limit", 1);
  this->declare_parameter<double>("yspeed_limit", 1);
  this->declare_parameter<double>("angular_speed_limit", 1.5);
  this->declare_parameter<int>("cmd_vel_hz", 20);
  this->get_parameter("xspeed_limit", xspeed_limit_);
  this->get_parameter("yspeed_limit", yspeed_limit_);
  this->get_parameter("angular_speed_limit", angular_speed_limit_);
  this->get_parameter("cmd_vel_hz", cmd_vel_hz_);

  pub_cmd_vel_ =
      this->create_publisher<geometry_msgs::msg::Twist>("cmd_vel", 10);
  pub_buzzer_ = this->create_publisher<std_msgs::msg::Bool>("buzzer", 1);
  pub_joy_state_ = this->create_publisher<std_msgs::msg::Bool>("joy_state", 10);
  pub_rgb_light_ =
      this->create_publisher<std_msgs::msg::Int32>("rgb_light", 10);

  pub_cmd_vel_timer_ = this->create_wall_timer(
    std::chrono::milliseconds(1000 / cmd_vel_hz_),
    std::bind(&SkynetJoy::publish_twist, this)
  );

  sub_joy_ = this->create_subscription<sensor_msgs::msg::Joy>(
      "joy", 10, std::bind(&SkynetJoy::callback, this, std::placeholders::_1));
}

void SkynetJoy::callback(const sensor_msgs::msg::Joy::SharedPtr joy_data) {
  if (joy_data->buttons[9] == 1) {
    cancel_nav();
  }

  if (joy_data->buttons[7] == 1) {
    cancel_light();
  }

  if (joy_data->buttons[11] == 1) {
    cancel_buzzer();
  }

  auto linear_gear = linear_gears_[linear_gear_index_];
  if (joy_data->buttons[13] == 1) {
    linear_gear = cancel_linear_gear();
  }
  auto angular_gear = angular_gears_[angular_gear_index_];
  if (joy_data->buttons[14] == 1) {
    angular_gear = cancel_angular_gear();
  }
  auto filter_data = [](float value) -> double {
    return std::abs(value) < 0.2 ? 0 : value;
  };
  auto xlinear_speed =
      filter_data(joy_data->axes[1]) * xspeed_limit_ * linear_gear;
  auto ylinear_speed =
      filter_data(joy_data->axes[0]) * yspeed_limit_ * linear_gear;
  auto angular_speed =
      filter_data(joy_data->axes[2]) * angular_speed_limit_ * angular_gear;
  if (xlinear_speed > xspeed_limit_) {
    xlinear_speed = xspeed_limit_;
  } else if (xlinear_speed < -xspeed_limit_) {
    xlinear_speed = -xspeed_limit_;
  }
  if (ylinear_speed > yspeed_limit_) {
    ylinear_speed = yspeed_limit_;
  } else if (ylinear_speed < -yspeed_limit_) {
    ylinear_speed = -yspeed_limit_;
  }
  if (angular_speed > angular_speed_limit_) {
    angular_speed = angular_speed_limit_;
  } else if (angular_speed < -angular_speed_limit_) {
    angular_speed = -angular_speed_limit_;
  }

  current_twist_.linear.x = xlinear_speed;
  current_twist_.linear.y = ylinear_speed;
  current_twist_.angular.z = angular_speed;
}

void SkynetJoy::cancel_nav() {
  auto now_time = this->now();
  auto delta = now_time - cancel_nav_time_;
  if (delta.seconds() > 1) {
    Joy_active_ = !Joy_active_;
    std_msgs::msg::Bool Joy_ctrl;
    Joy_ctrl.data = Joy_active_;
    pub_joy_state_->publish(Joy_ctrl);
    pub_cmd_vel_->publish(geometry_msgs::msg::Twist());
    cancel_nav_time_ = now_time;
  }
}

void SkynetJoy::cancel_buzzer() {
  auto now_time = this->now();
  auto delta = now_time - cancel_buzzer_time_;
  if (delta.seconds() > 0.25) {
    buzzer_active_ = !buzzer_active_;
    std_msgs::msg::Bool buzzer_ctrl;
    buzzer_ctrl.data = buzzer_active_;
    pub_buzzer_->publish(buzzer_ctrl);
    cancel_buzzer_time_ = now_time;
  }
}

void SkynetJoy::cancel_light() {
  auto now_time = this->now();
  auto delta = now_time - cancel_light_time_;
  if (delta.seconds() > 0.25) {
    rgb_light_index_ = (rgb_light_index_ + 1) % 7;
    std_msgs::msg::Int32 rgb_light_ctrl;
    rgb_light_ctrl.data = rgb_light_index_;
    pub_rgb_light_->publish(rgb_light_ctrl);
    cancel_light_time_ = now_time;
  }
}

double SkynetJoy::cancel_linear_gear() {
  double linear_gear = linear_gears_[linear_gear_index_];
  auto now_time = this->now();
  auto delta = now_time - cancel_linear_gear_time_;
  if (delta.seconds() > 0.25) {
    linear_gear_index_ = (linear_gear_index_ + 1) % linear_gears_.size();
    linear_gear = linear_gears_[linear_gear_index_];
    cancel_linear_gear_time_ = now_time;
  }
  return linear_gear;
}

double SkynetJoy::cancel_angular_gear() {
  double angular_gear = angular_gears_[angular_gear_index_];
  auto now_time = this->now();
  auto delta = now_time - cancel_angular_gear_time_;
  if (delta.seconds() > 0.25) {
    angular_gear_index_ = (angular_gear_index_ + 1) % angular_gears_.size();
    angular_gear = angular_gears_[angular_gear_index_];
    cancel_angular_gear_time_ = now_time;
  }
  return angular_gear;
}

void SkynetJoy::publish_twist() {
  auto is_moved =
      current_twist_.linear.x != 0.0 || current_twist_.linear.y != 0.0 || current_twist_.angular.z != 0.0;
  if (Joy_active_ && is_moved) {
    RCLCPP_DEBUG(this->get_logger(), "joy control now");
    pub_cmd_vel_->publish(current_twist_);
    last_moved_ = true;
  } else if (Joy_active_ && last_moved_) {
    pub_cmd_vel_->publish(geometry_msgs::msg::Twist());
    last_moved_ = false;
  }
}

int main(int argc, char **argv) {
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<SkynetJoy>());
  rclcpp::shutdown();
  return 0;
}
