#include "navigation/mesh_builder.h"

#include "common/exception.h"
#include "common/math.h"

#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>
#include <CGAL/create_offset_polygons_from_polygon_with_holes_2.h>
#include <CGAL/create_straight_skeleton_from_polygon_with_holes_2.h>
#include <boost/shared_ptr.hpp>
#include <boost/geometry.hpp>

#include "geom/boost.h"

namespace truck::navigation::mesh {

using CGAL_K = CGAL::Exact_predicates_inexact_constructions_kernel;

namespace bg = boost::geometry;

using IndexPoint = std::pair<geom::Vec2, size_t>;
using IndexPoints = std::vector<IndexPoint>;
using RTree = bg::index::rtree<IndexPoint, bg::index::rstar<16>>;

namespace {

CGAL::Polygon_2<CGAL_K> toCGALPolygon(const geom::Polygon& polygon) {
    CGAL::Polygon_2<CGAL_K> cgal_poly;

    for (const geom::Vec2& point : polygon) {
        cgal_poly.push_back(CGAL_K::Point_2(point.x, point.y));
    }

    return cgal_poly;
}

CGAL::Polygon_with_holes_2<CGAL_K> toCGALPolygonWithHoles(const geom::ComplexPolygon& polygon) {
    CGAL::Polygon_with_holes_2<CGAL_K> cgal_poly_with_holes(toCGALPolygon(polygon.outer));

    for (const geom::Polygon& inner_poly : polygon.inners) {
        cgal_poly_with_holes.add_hole(toCGALPolygon(inner_poly));
    }

    return cgal_poly_with_holes;
}

geom::Polyline extractPolylineFromCGALPolygon(const CGAL::Polygon_2<CGAL_K>& cgal_poly) {
    geom::Polyline polyline;

    for (const auto& cgal_point : cgal_poly) {
        polyline.emplace_back(cgal_point.x(), cgal_point.y());
    }

    polyline.push_back(polyline.front());
    return polyline;
}

IndexPoints getPointNeighborsSearchRadius(
    const geom::Vec2& point, const RTree& rtree, double search_radius) {
    IndexPoints rtree_indexed_points;

    const geom::BoundingBox rtree_box(
        geom::Vec2(point.x - search_radius, point.y - search_radius),
        geom::Vec2(point.x + search_radius, point.y + search_radius));

    rtree.query(
        bg::index::intersects(rtree_box)
            && bg::index::satisfies([&](const IndexPoint& rtree_indexed_point) {
                   const geom::Vec2 neighbor_point = rtree_indexed_point.first;
                   return (point - neighbor_point).lenSq() < squared(search_radius);
               }),
        std::back_inserter(rtree_indexed_points));

    return rtree_indexed_points;
}

}  // namespace

MeshBuilder::MeshBuilder(const MeshParams& params) : params_(params) {}

MeshBuild MeshBuilder::build(const geom::ComplexPolygons& polygons) const {
    VERIFY(polygons.size() == 1);
    const auto& polygon = polygons[0];

    MeshBuild mesh_build;

    buildSkeleton(mesh_build, polygon);
    buildLevelLines(mesh_build, polygon, params_.offset);
    buildMesh(mesh_build, params_.dist);

    if (params_.filter.enabled) {
        applyMeshFilter(mesh_build, params_.filter.search_radius);
    }

    return mesh_build;
}

void MeshBuilder::buildSkeleton(MeshBuild& mesh_build, const geom::ComplexPolygon& polygon) const {
    const boost::shared_ptr<CGAL::Straight_skeleton_2<CGAL_K>> cgal_skeleton_ptr =
        CGAL::create_interior_straight_skeleton_2(toCGALPolygonWithHoles(polygon));

    for (const auto& cgal_edge_it : cgal_skeleton_ptr->halfedge_handles()) {
        mesh_build.skeleton.emplace_back(
            geom::Vec2(cgal_edge_it->vertex()->point().x(), cgal_edge_it->vertex()->point().y()),
            geom::Vec2(
                cgal_edge_it->opposite()->vertex()->point().x(),
                cgal_edge_it->opposite()->vertex()->point().y()));
    }
}

void MeshBuilder::buildLevelLines(
    MeshBuild& mesh_build, const geom::ComplexPolygon& polygon, double offset) const {
    double cur_offset = offset;

    auto cgal_polys_with_holes = CGAL::create_interior_skeleton_and_offset_polygons_with_holes_2(
        cur_offset, toCGALPolygonWithHoles(polygon));

    while (!cgal_polys_with_holes.empty()) {
        for (const auto& cgal_poly_with_holes_ptr : cgal_polys_with_holes) {
            mesh_build.level_lines.push_back(
                extractPolylineFromCGALPolygon(cgal_poly_with_holes_ptr->outer_boundary()));

            for (const auto& cgal_poly_inner : cgal_poly_with_holes_ptr->holes()) {
                mesh_build.level_lines.push_back(extractPolylineFromCGALPolygon(cgal_poly_inner));
            }
        }

        cur_offset += offset;

        cgal_polys_with_holes = CGAL::create_interior_skeleton_and_offset_polygons_with_holes_2(
            cur_offset, toCGALPolygonWithHoles(polygon));
    }
}

void MeshBuilder::buildMesh(MeshBuild& mesh_build, double dist) const {
    for (const auto& level_line : mesh_build.level_lines) {
        for (auto it = level_line.ubegin(params_.dist); it != level_line.uend(); ++it) {
            mesh_build.mesh.emplace_back((*it).pos);
        }
    }
}

void MeshBuilder::applyMeshFilter(MeshBuild& mesh_build, double search_radius) const {
    RTree rtree;
    const size_t points_count = mesh_build.mesh.size();

    for (size_t i = 0; i < points_count; ++i) {
        rtree.insert(IndexPoint(mesh_build.mesh[i], i));
    }

    std::vector<bool> points_to_remove_mask(points_count, false);

    for (size_t i = 0; i < points_count; ++i) {
        if (points_to_remove_mask[i]) {
            continue;
        }

        for (const IndexPoint& indexed_neighbor_point :
             getPointNeighborsSearchRadius(mesh_build.mesh[i], rtree, search_radius)) {
            if (indexed_neighbor_point.second == i) {
                continue;
            }

            points_to_remove_mask[indexed_neighbor_point.second] = true;
        }
    }

    std::vector<geom::Vec2> mesh_filtered;

    for (size_t i = 0; i < points_count; ++i) {
        if (points_to_remove_mask[i]) {
            continue;
        }

        mesh_filtered.push_back(mesh_build.mesh[i]);
    }

    mesh_build.mesh = std::move(mesh_filtered);
}

}  // namespace truck::navigation::mesh
