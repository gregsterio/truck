#pragma once

#include "model/model.h"
#include "map/map.h"
#include "truck_msgs/msg/control.hpp"
#include "truck_msgs/msg/control_mode.hpp"
#include "truck_msgs/msg/trajectory.hpp"
#include "truck_msgs/msg/waypoints.hpp"
#include "truck_msgs/msg/hardware_telemetry.hpp"
#include "truck_msgs/msg/navigation_mesh.hpp"
#include "truck_msgs/msg/navigation_route.hpp"

#include <nav_msgs/msg/odometry.hpp>
#include <visualization_msgs/msg/marker.hpp>
#include <visualization_msgs/msg/marker_array.hpp>
#include <tf2_msgs/msg/tf_message.hpp>
#include <tf2_geometry_msgs/tf2_geometry_msgs.hpp>
#include <tf2/LinearMath/Transform.h>
#include <tf2/convert.h>

#include <rclcpp/rclcpp.hpp>
#include <rclcpp/time.hpp>

#include <string>
#include <optional>

namespace truck::visualization {

class VisualizationNode : public rclcpp::Node {
  public:
    VisualizationNode();

  private:
    void initializePtrFields();
    void initializeParams();
    void initializeTopicHandlers();
    void initializeCacheBodyBaseTf();
    void initializeCacheWheelBaseTfs();
    void initializeCache();

    void handleTrajectory(truck_msgs::msg::Trajectory::ConstSharedPtr msg);
    void handleControl(truck_msgs::msg::Control::ConstSharedPtr msg);
    void handleMode(truck_msgs::msg::ControlMode::ConstSharedPtr msg);
    void handleWaypoints(truck_msgs::msg::Waypoints::ConstSharedPtr msg);
    void handleTelemetry(truck_msgs::msg::HardwareTelemetry::ConstSharedPtr msg);
    void handleOdometry(nav_msgs::msg::Odometry::ConstSharedPtr msg);
    void handleNavigationMesh(truck_msgs::msg::NavigationMesh::ConstSharedPtr msg);
    void handleNavigationRoute(truck_msgs::msg::NavigationRoute::ConstSharedPtr msg);

    void updateWheelsSpin();
    void updateEgo();

    void publishTrajectory() const;
    void publishEgo() const;
    void publishEgoTrack() const;
    void publishArc() const;
    void publishWaypoints() const;
    void publishMap() const;
    void publishNavigationMesh() const;
    void publishNavigationRoute() const;

    std_msgs::msg::ColorRGBA velocityToColor(double velocity, double alpha = 1.0) const;

    struct Parameters {
        rclcpp::Duration ttl = rclcpp::Duration::from_seconds(1.0);

        double ego_z_lev = 0.0;
        double ego_height = 0.0;

        double ego_track_width = 0.06;
        double ego_track_height = 0.01;
        rclcpp::Duration ego_track_ttl = rclcpp::Duration::from_seconds(2.0);
        size_t ego_track_rate = 5;

        double arc_z_lev = 0.0;
        double arc_width = 0.0;
        double arc_length = 1.0;

        double waypoints_z_lev = 0.0;
        double waypoints_radius = 0.0;

        double trajectory_z_lev = 0.0;
        double trajectory_width = 0.0;

        std::string mesh_body = "";
        std::string mesh_wheel = "";

        double map_z_lev = 0.0;

        double navigation_mesh_z_lev = 0.0;
        double navigation_mesh_radius = 0.0;

        double navigation_route_z_lev = 0.0;
        double navigation_route_width = 0.0;
    } params_{};

    enum WheelIndex : int8_t { kFrontLeft = 0, kFrontRight = 1, kRearLeft = 2, kRearRight = 3 };

    static constexpr std::array<int, 4> kAllWheels{
        WheelIndex::kFrontLeft,
        WheelIndex::kFrontRight,
        WheelIndex::kRearLeft,
        WheelIndex::kRearRight};

    static constexpr std::array<const char*, 4> kWheelFrames{
        "front_left_wheel", "front_right_wheel", "rear_left_wheel", "rear_right_wheel"};

    struct Cache {
        tf2::Transform body_base_tf;
        std::array<tf2::Transform, 4> wheel_base_tfs;
    } cache_;

    std::unique_ptr<model::Model> model_ = nullptr;
    std::unique_ptr<map::Map> map_ = nullptr;

    struct State {
        size_t odom_seq_id = 0;
        truck_msgs::msg::ControlMode::ConstSharedPtr mode = nullptr;
        truck_msgs::msg::Control::ConstSharedPtr control = nullptr;
        nav_msgs::msg::Odometry::ConstSharedPtr odom = nullptr;
        truck_msgs::msg::Trajectory::ConstSharedPtr trajectory = nullptr;
        truck_msgs::msg::HardwareTelemetry::ConstSharedPtr telemetry = nullptr;
        truck_msgs::msg::Waypoints::ConstSharedPtr waypoints = nullptr;
        truck_msgs::msg::NavigationMesh::ConstSharedPtr navigation_mesh = nullptr;
        truck_msgs::msg::NavigationRoute::ConstSharedPtr navigation_route = nullptr;
        double wheel_spin_angles[4] = {0.0};
        std::optional<double> last_ego_update_seconds;
    } state_;

    struct Slots {
        rclcpp::Subscription<truck_msgs::msg::Trajectory>::SharedPtr trajectory = nullptr;
        rclcpp::Subscription<truck_msgs::msg::Control>::SharedPtr control = nullptr;
        rclcpp::Subscription<truck_msgs::msg::ControlMode>::SharedPtr mode = nullptr;
        rclcpp::Subscription<truck_msgs::msg::Waypoints>::SharedPtr waypoints = nullptr;
        rclcpp::Subscription<truck_msgs::msg::HardwareTelemetry>::SharedPtr telemetry = nullptr;
        // foxglove has twitching if publish ego pose in base frame, use odom for smoother result!
        rclcpp::Subscription<nav_msgs::msg::Odometry>::SharedPtr odom = nullptr;
        rclcpp::Subscription<truck_msgs::msg::NavigationMesh>::SharedPtr navigation_mesh = nullptr;
        rclcpp::Subscription<truck_msgs::msg::NavigationRoute>::SharedPtr navigation_route =
            nullptr;
    } slot_;

    struct Signals {
        rclcpp::Publisher<visualization_msgs::msg::Marker>::SharedPtr trajectory = nullptr;
        rclcpp::Publisher<visualization_msgs::msg::MarkerArray>::SharedPtr ego = nullptr;
        rclcpp::Publisher<visualization_msgs::msg::Marker>::SharedPtr ego_track = nullptr;
        rclcpp::Publisher<visualization_msgs::msg::Marker>::SharedPtr arc = nullptr;
        rclcpp::Publisher<visualization_msgs::msg::Marker>::SharedPtr waypoints = nullptr;
        rclcpp::Publisher<visualization_msgs::msg::Marker>::SharedPtr map = nullptr;
        rclcpp::Publisher<visualization_msgs::msg::Marker>::SharedPtr navigation_mesh = nullptr;
        rclcpp::Publisher<visualization_msgs::msg::Marker>::SharedPtr navigation_route = nullptr;
    } signal_;

    rclcpp::TimerBase::SharedPtr timer_ = nullptr;
};

}  // namespace truck::visualization
