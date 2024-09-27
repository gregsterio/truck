#include "simulator_2d/simulation_map.h"
#include "simulator_2d/status_code.h"
#include "simulator_2d/truck_state.h"

#include "model/model.h"
#include "geom/angle.h"
#include "geom/angle_vector.h"
#include "geom/pose.h"
#include "geom/segment.h"
#include "geom/vector.h"
#include "geom/vector3.h"

#include <Eigen/Dense>

#include <optional>
#include <random>
#include <string>
#include <vector>

namespace truck::simulator {

struct NoiseGeneratorParams {
    struct Lidar {
        bool enable;
        double mean;
        double variance;
    } lidar;

    struct Gyro {
        bool enable;
        double mean;
        double variance;
    } gyro;

    struct Accel {
        bool enable;
        double mean;
        double variance;
    } accel;
};

class NoiseGenerator {
  public:
    NoiseGenerator(const NoiseGeneratorParams& params, size_t seed = 0);

    void applyToGyro(geom::Vec3& gyro);
    void applyToAccel(geom::Vec3& accel);
    void applyToLidar(std::vector<float>& ranges);

  private:
    std::default_random_engine generator_;

    std::normal_distribution<double> gyro_dist_;
    std::normal_distribution<double> accel_dist_;
    std::normal_distribution<double> lidar_dist_;

    NoiseGeneratorParams params_;
};

class SimulatorEngine {
  public:
    SimulatorEngine(
        std::unique_ptr<model::Model> model, std::unique_ptr<NoiseGenerator> noise_generator,
        double integration_step = 1e-3, double precision = 1e-8);

    void resetBase(
        const geom::Pose& pose, double middle_steering = 0.0, double linear_velocity = 0.0);
    void resetMap(const std::string& path);
    void eraseMap();

    TruckState getTruckState() const;

    void setBaseControl(double velocity, double acceleration, double curvature);
    void setBaseControl(double velocity, double curvature);
    void advance(double seconds = 1.0);

  private:
    enum StateIndex { kX = 0, kY = 1, kYaw = 2, kSteering = 3, kLinearVelocity = 4 };

    using State = Eigen::Matrix<double, 5, 1>;

    void initializeParameters(double integration_step, double precision);
    void initializeMathCache(double integration_step);
    void initializeLidarCache();
    void initializeImuCache();

    void resetRear(double x, double y, double yaw, double steering, double linear_velocity);
    void resetRear();

    void checkForCollisions();

    geom::Pose getOdomBasePose() const;
    model::Steering getTargetSteering() const;
    geom::Vec3 getImuAngularVelocity(double angular_velocity) const;
    geom::Vec3 getImuLinearAcceleration(const model::Twist& rear_twist) const;

    double getCurrentAcceleration() const;
    double getCurrentSteeringVelocity() const;
    State calculateStateDerivative(
        const State& state, double acceleration, double steering_velocity) const;
    State calculateRK4(double acceleration, double steering_velocity) const;

    struct Parameters {
        double integration_step;
        double precision;
    } params_;

    struct Cache {
        double integration_step_2;
        double integration_step_6;
        double inverse_integration_step;
        double inverse_wheelbase_length;
        geom::Vec2 rear_to_lidar;
        geom::Vec2 rear_to_imu_translation;
        tf2::Transform base_to_hyro_rotation;
    } cache_;

    // The value is set in setControl and should not change in other methods.
    struct Control {
        double velocity = 0.0;
        std::optional<double> acceleration = 0.0;
        double curvature = 0.0;
    } control_;

    rclcpp::Time time_;

    StatusCode status_;

    State rear_ax_state_;

    std::unique_ptr<model::Model> model_ = nullptr;

    std::unique_ptr<NoiseGenerator> noise_generator_ = nullptr;

    SimulationMap map_;
};

}  // namespace truck::simulator
