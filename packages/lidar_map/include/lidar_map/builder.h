#pragma once

#include "lidar_map/common.h"

#include "geom/pose.h"

namespace truck::lidar_map {

struct BuilderParams {
    std::string icp_config;

    double icp_edge_max_dist = 0.6;
    double poses_min_dist = 0.5;

    double odom_edge_weight = 1.0;
    double icp_edge_weight = 3.0;

    size_t optimizer_steps = 10;

    bool verbose = true;
};

class Builder {
  public:
    Builder(const BuilderParams& params);

    std::pair<geom::Poses, Clouds> filterByPosesProximity(
        const geom::Poses& poses, const Clouds& clouds) const;

    geom::Poses optimizePoses(const geom::Poses& poses, const Clouds& clouds);

    Clouds transformClouds(const geom::Poses& poses, const Clouds& clouds) const;

    Cloud mergeClouds(const Clouds& clouds) const;

  private:
    ICP icp_;

    BuilderParams params_;
};

}  // namespace truck::lidar_map
