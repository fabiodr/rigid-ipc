// Time-of-impact computation for rigid bodies with angular trajectories.
#include "time_of_impact.hpp"

#include <geometry/distance.hpp>
#include <geometry/intersection.hpp>
#include <interval/interval_root_finder.hpp>
#include <utils/eigen_ext.hpp>

namespace ccd {

typedef physics::Pose<Interval> PoseI;

/// Find time-of-impact between two rigid bodies
bool compute_edge_vertex_time_of_impact_redon(
    const physics::RigidBody& bodyA,       // Body of the vertex
    const physics::Pose<double>& poseA_t0, // Pose of bodyA at t=0
    const physics::Pose<double>& poseA_t1, // Pose of bodyA at t=1
    size_t vertex_id,                      // In bodyA
    const physics::RigidBody& bodyB,       // Body of the edge
    const physics::Pose<double>& poseB_t0, // Pose of bodyB at t=0
    const physics::Pose<double>& poseB_t1, // Pose of bodyB at t=1
    size_t edge_id,                        // In bodyB
    double& toi,
    double earliest_toi, // Only search for collision in [0, earliest_toi]
    double toi_tolerance)
{
    int dim = bodyA.dim();
    assert(bodyB.dim() == dim);
    assert(dim == 2);

    const PoseI poseIA_t0 = poseA_t0.cast<Interval>();
    const PoseI poseIA_t1 = poseA_t1.cast<Interval>();

    const PoseI poseIB_t0 = poseB_t0.cast<Interval>();
    const PoseI poseIB_t1 = poseB_t1.cast<Interval>();

    const auto vertex_positions =
        [&](const Interval& t, Eigen::Vector2I& vertex,
            Eigen::Vector2I& edge_vertex0, Eigen::Vector2I& edge_vertex1) {
            // Compute the poses at time t
            PoseI poseIA = PoseI::interpolate(poseIA_t0, poseIA_t1, t);
            PoseI poseIB = PoseI::interpolate(poseIB_t0, poseIB_t1, t);

            // Get the world vertex of the edges at time t
            vertex = bodyA.world_vertex(poseIA, vertex_id);
            // Get the world vertex of the edge at time t
            edge_vertex0 = bodyB.world_vertex(poseIB, bodyB.edges(edge_id, 0));
            edge_vertex1 = bodyB.world_vertex(poseIB, bodyB.edges(edge_id, 1));
        };

    const auto distance = [&](const Interval& t) {
        // Get the world vertex of the vertex and edge at time t
        Eigen::Vector2I vertex, edge_vertex0, edge_vertex1;
        vertex_positions(t, vertex, edge_vertex0, edge_vertex1);
        return geometry::point_line_signed_distance(
            vertex, edge_vertex0, edge_vertex1);
    };

    const auto is_point_along_edge = [&](const Interval& t) {
        // Get the world vertex of the vertex and edge at time t
        Eigen::Vector2I vertex, edge_vertex0, edge_vertex1;
        vertex_positions(t, vertex, edge_vertex0, edge_vertex1);
        return geometry::is_point_along_edge(
            vertex, edge_vertex0, edge_vertex1);
    };

    Interval toi_interval;
    bool is_impacting = interval_root_finder(
        distance, is_point_along_edge, Interval(0, earliest_toi), toi_tolerance,
        toi_interval);
    // Return a conservative time-of-impact
    toi = toi_interval.lower();
    // This time of impact is very dangerous for convergence
    // assert(!is_impacting || toi > 0);
    return is_impacting;
}

// Find time-of-impact between two rigid bodies
bool compute_edge_edge_time_of_impact_redon(
    const physics::RigidBody& bodyA,       // Body of the first edge
    const physics::Pose<double>& poseA_t0, // Pose of bodyA at t=0
    const physics::Pose<double>& poseA_t1, // Pose of bodyA at t=1
    size_t edgeA_id,                       // In bodyA
    const physics::RigidBody& bodyB,       // Body of the second edge
    const physics::Pose<double>& poseB_t0, // Pose of bodyB at t=0
    const physics::Pose<double>& poseB_t1, // Pose of bodyB at t=1
    size_t edgeB_id,                       // In bodyB
    double& toi,
    double earliest_toi, // Only search for collision in [0, earliest_toi]
    double toi_tolerance)
{
    int dim = bodyA.dim();
    assert(bodyB.dim() == dim);
    assert(dim == 3);

    const PoseI poseIA_t0 = poseA_t0.cast<Interval>();
    const PoseI poseIA_t1 = poseA_t1.cast<Interval>();

    const PoseI poseIB_t0 = poseB_t0.cast<Interval>();
    const PoseI poseIB_t1 = poseB_t1.cast<Interval>();

    const auto vertex_positions = [&](const Interval& t,
                                      Eigen::Vector3I& edgeA_vertex0,
                                      Eigen::Vector3I& edgeA_vertex1,
                                      Eigen::Vector3I& edgeB_vertex0,
                                      Eigen::Vector3I& edgeB_vertex1) {
        // Compute the poses at time t
        PoseI poseIA = PoseI::interpolate(poseIA_t0, poseIA_t1, t);
        PoseI poseIB = PoseI::interpolate(poseIB_t0, poseIB_t1, t);

        // Get the world vertex of the edges at time t
        edgeA_vertex0 = bodyA.world_vertex(poseIA, bodyA.edges(edgeA_id, 0));
        edgeA_vertex1 = bodyA.world_vertex(poseIA, bodyA.edges(edgeA_id, 1));

        edgeB_vertex0 = bodyB.world_vertex(poseIB, bodyB.edges(edgeB_id, 0));
        edgeB_vertex1 = bodyB.world_vertex(poseIB, bodyB.edges(edgeB_id, 1));
    };

    const auto distance = [&](const Interval& t) {
        // Get the world vertex of the edges at time t
        Eigen::Vector3I edgeA_vertex0, edgeA_vertex1;
        Eigen::Vector3I edgeB_vertex0, edgeB_vertex1;
        vertex_positions(
            t, edgeA_vertex0, edgeA_vertex1, edgeB_vertex0, edgeB_vertex1);
        return geometry::line_line_signed_distance(
            edgeA_vertex0, edgeA_vertex1, edgeB_vertex0, edgeB_vertex1);
    };

    const auto is_intersection_inside_edges = [&](const Interval& t) -> bool {
        // Get the world vertex of the edges at time t
        Eigen::Vector3I edgeA_vertex0, edgeA_vertex1;
        Eigen::Vector3I edgeB_vertex0, edgeB_vertex1;
        vertex_positions(
            t, edgeA_vertex0, edgeA_vertex1, edgeB_vertex0, edgeB_vertex1);
        return geometry::are_edges_intersecting(
            edgeA_vertex0, edgeA_vertex1, edgeB_vertex0, edgeB_vertex1);
    };

    Interval toi_interval;
    bool is_impacting = interval_root_finder(
        distance, is_intersection_inside_edges, Interval(0, earliest_toi),
        toi_tolerance, toi_interval);

    // Return a conservative time-of-impact
    toi = toi_interval.lower();
    // This time of impact is very dangerous for convergence
    // assert(!is_impacting || toi > 0);
    return is_impacting;
}

// Find time-of-impact between two rigid bodies
bool compute_face_vertex_time_of_impact_redon(
    const physics::RigidBody& bodyA,       // Body of the vertex
    const physics::Pose<double>& poseA_t0, // Pose of bodyA at t=0
    const physics::Pose<double>& poseA_t1, // Pose of bodyA at t=1
    size_t vertex_id,                      // In bodyA
    const physics::RigidBody& bodyB,       // Body of the triangle
    const physics::Pose<double>& poseB_t0, // Pose of bodyB at t=0
    const physics::Pose<double>& poseB_t1, // Pose of bodyB at t=1
    size_t face_id,                        // In bodyB
    double& toi,
    double earliest_toi, // Only search for collision in [0, earliest_toi]
    double toi_tolerance)
{
    int dim = bodyA.dim();
    assert(bodyB.dim() == dim);
    assert(dim == 3);

    const PoseI poseIA_t0 = poseA_t0.cast<Interval>();
    const PoseI poseIA_t1 = poseA_t1.cast<Interval>();

    const PoseI poseIB_t0 = poseB_t0.cast<Interval>();
    const PoseI poseIB_t1 = poseB_t1.cast<Interval>();

    const auto vertex_positions =
        [&](const Interval& t, Eigen::Vector3I& vertex,
            Eigen::Vector3I& face_vertex0, Eigen::Vector3I& face_vertex1,
            Eigen::Vector3I& face_vertex2) {
            // Compute the poses at time t
            PoseI poseIA = PoseI::interpolate(poseIA_t0, poseIA_t1, t);
            PoseI poseIB = PoseI::interpolate(poseIB_t0, poseIB_t1, t);

            // Get the world vertex of the point at time t
            vertex = bodyA.world_vertex(poseIA, vertex_id);
            // Get the world vertex of the edge at time t
            face_vertex0 = bodyB.world_vertex(poseIB, bodyB.faces(face_id, 0));
            face_vertex1 = bodyB.world_vertex(poseIB, bodyB.faces(face_id, 1));
            face_vertex2 = bodyB.world_vertex(poseIB, bodyB.faces(face_id, 2));
        };

    const auto distance = [&](const Interval& t) {
        // Get the world vertex and face of the point at time t
        Eigen::Vector3I vertex, face_vertex0, face_vertex1, face_vertex2;
        vertex_positions(t, vertex, face_vertex0, face_vertex1, face_vertex2);
        return geometry::point_plane_signed_distance(
            vertex, face_vertex0, face_vertex1, face_vertex2);
    };

    const auto is_point_inside_triangle = [&](const Interval& t) {
        // Get the world vertex and face of the point at time t
        Eigen::Vector3I vertex, face_vertex0, face_vertex1, face_vertex2;
        vertex_positions(t, vertex, face_vertex0, face_vertex1, face_vertex2);
        return geometry::is_point_inside_triangle(
            vertex, face_vertex0, face_vertex1, face_vertex2);
    };

    Interval toi_interval;
    bool is_impacting = interval_root_finder(
        distance, is_point_inside_triangle, Interval(0, earliest_toi),
        toi_tolerance, toi_interval);

    // Return a conservative time-of-impact
    toi = toi_interval.lower();
    // This time of impact is very dangerous for convergence
    // assert(!is_impacting || toi > 0);
    return is_impacting;
}

} // namespace ccd