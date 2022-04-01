#pragma once

#include "planning_interfaces/msg/path.hpp"
#include "nav_msgs/msg/odometry.hpp"
#include "pure_pursuit_msgs/msg/command.hpp"

#include "visual_info.hpp"

#include "rclcpp/rclcpp.hpp"

#include "controller.hpp"

#include <optional>
#include <memory>

namespace pure_pursuit {

class PursuitNode : public rclcpp::Node {
public:
    PursuitNode()
        : Node("PursuitNode")
        , controller(Parameters(*this))
    {
        bool publish_debug_info = this->declare_parameter<bool>("publish_debug_info", true);

        cmd_publisher = Node::create_publisher<pure_pursuit_msgs::msg::Command>("pure_pursuit_command", 1);
        if (publish_debug_info) {
            arc_publisher = Node::create_publisher<visualization_msgs::msg::MarkerArray>("pure_pursuit_arc", 1);
        }
        slot_path = this->create_subscription<planning_interfaces::msg::Path>(
            "planned_path",
            1,
            [this](planning_interfaces::msg::Path::UniquePtr path) {
                trajectory = std::move(path->path.poses);
            }
        );
        slot_state = Node::create_subscription<nav_msgs::msg::Odometry>(
            "current_state",
            1,
            [this, publish_debug_info](nav_msgs::msg::Odometry::UniquePtr odometry) {
                if (trajectory) {
                    pure_pursuit_msgs::msg::Command cmd;
                    if (publish_debug_info) {
                        VisualInfo info;
                        cmd = controller.get_motion(*odometry, *trajectory, &info);
                        arc_publisher->publish(info.arc);
                    } else {
                        cmd = controller.get_motion(*odometry, *trajectory, nullptr);
                    }
                    cmd_publisher->publish(cmd);
                }
            }
        );
    }

private:
    rclcpp::Subscription<planning_interfaces::msg::Path>::SharedPtr slot_path;
    rclcpp::Subscription<nav_msgs::msg::Odometry>::SharedPtr slot_state;
    rclcpp::Publisher<pure_pursuit_msgs::msg::Command>::SharedPtr cmd_publisher;
    rclcpp::Publisher<visualization_msgs::msg::MarkerArray>::SharedPtr arc_publisher;
    std::optional<std::vector<geometry_msgs::msg::PoseStamped>> trajectory;

    Controller controller;
};

};