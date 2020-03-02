// Time-of-impact computation for rigid bodies with angular trajectories.
#include "rigid_body_time_of_impact.hpp"

#include <ccd/interval_root_finder.hpp>
#include <geometry/distance.hpp>
#include <geometry/intersection.hpp>
#include <geometry/normal.hpp>
#include <logger.hpp>
#include <utils/eigen_ext.hpp>
#include <utils/not_implemented_error.hpp>

namespace ccd {

/// Find time-of-impact between two rigid bodies
bool compute_edge_vertex_time_of_impact(
    const physics::RigidBody& bodyA,            // Body of the vertex
    const physics::Pose<double>& poseA,         // Pose of bodyA
    const physics::Pose<double>& displacementA, // Displacement of bodyA
    const size_t& vertex_id,                    // In bodyA
    const physics::RigidBody& bodyB,            // Body of the edge
    const physics::Pose<double>& poseB,         // Pose of bodyB
    const physics::Pose<double>& displacementB, // Displacement of bodyB
    const size_t& edge_id,                      // In bodyB
    double& toi)
{
    int dim = bodyA.dim();
    assert(bodyB.dim() == dim);
    assert(dim == 2); // TODO: 3D

    const physics::Pose<Interval> poseA_interval = poseA.cast<Interval>();
    const physics::Pose<Interval> poseB_interval = poseB.cast<Interval>();

    const physics::Pose<Interval> displacementA_interval =
        displacementA.cast<Interval>();
    const physics::Pose<Interval> displacementB_interval =
        displacementB.cast<Interval>();

    const auto vertex_positions = [&](const Interval& t,
                                      Eigen::VectorX3<Interval>& vertex,
                                      Eigen::VectorX3<Interval>& edge_vertex0,
                                      Eigen::VectorX3<Interval>& edge_vertex1) {
        // Compute the poses at time t
        physics::Pose<Interval> bodyA_pose_interval =
            poseA_interval + displacementA_interval * t;
        physics::Pose<Interval> bodyB_pose_interval =
            poseB_interval + displacementB_interval * t;

        // Get the world vertex of the edges at time t
        vertex = bodyA.world_vertex(bodyA_pose_interval, vertex_id);
        // Get the world vertex of the edge at time t
        edge_vertex0 =
            bodyB.world_vertex(bodyB_pose_interval, bodyB.edges(edge_id, 0));
        edge_vertex1 =
            bodyB.world_vertex(bodyB_pose_interval, bodyB.edges(edge_id, 1));
    };

    const auto distance = [&](const Interval& t) {
        // Get the world vertex of the vertex and edge at time t
        Eigen::VectorX3<Interval> vertex, edge_vertex0, edge_vertex1;
        vertex_positions(t, vertex, edge_vertex0, edge_vertex1);
        return geometry::point_line_signed_distance(
            vertex, edge_vertex0, edge_vertex1);
    };

    const auto is_point_along_edge = [&](const Interval& t) {
        // Get the world vertex of the vertex and edge at time t
        Eigen::VectorX3<Interval> vertex, edge_vertex0, edge_vertex1;
        vertex_positions(t, vertex, edge_vertex0, edge_vertex1);
        return geometry::is_point_along_segment(
            vertex, edge_vertex0, edge_vertex1);
    };

    Interval toi_interval;
    // TODO: Set tolerance dynamically
    bool is_impacting = interval_root_finder(
        distance, is_point_along_edge, Interval(0, 1), toi_interval,
        /*tol=*/1e-8);
    // Return a conservative time-of-impact
    toi = toi_interval.lower();
    return is_impacting;
}

// Find time-of-impact between two rigid bodies
bool compute_edge_edge_time_of_impact(
    const physics::RigidBody& bodyA,            // Body of the first edge
    const physics::Pose<double>& poseA,         // Pose of bodyA
    const physics::Pose<double>& displacementA, // Displacement of bodyA
    const size_t& edgeA_id,                     // In bodyA
    const physics::RigidBody& bodyB,            // Body of the second edge
    const physics::Pose<double>& poseB,         // Pose of bodyB
    const physics::Pose<double>& displacementB, // Displacement of bodyB
    const size_t& edgeB_id,                     // In bodyB
    double& toi)
{
    int dim = bodyA.dim();
    assert(bodyB.dim() == dim);
    assert(dim == 3);

    const physics::Pose<Interval> poseA_interval = poseA.cast<Interval>();
    const physics::Pose<Interval> poseB_interval = poseB.cast<Interval>();

    const physics::Pose<Interval> displacementA_interval =
        displacementA.cast<Interval>();
    const physics::Pose<Interval> displacementB_interval =
        displacementB.cast<Interval>();

    const auto vertex_positions =
        [&](const Interval& t, Eigen::VectorX3<Interval>& edgeA_vertex0,
            Eigen::VectorX3<Interval>& edgeA_vertex1,
            Eigen::VectorX3<Interval>& edgeB_vertex0,
            Eigen::VectorX3<Interval>& edgeB_vertex1) {
            // Compute the poses at time t
            physics::Pose<Interval> bodyA_pose_interval =
                poseA_interval + displacementA_interval * t;
            physics::Pose<Interval> bodyB_pose_interval =
                poseB_interval + displacementB_interval * t;

            // Get the world vertex of the edges at time t
            edgeA_vertex0 = bodyA.world_vertex(
                bodyA_pose_interval, bodyA.edges(edgeA_id, 0));
            edgeA_vertex1 = bodyA.world_vertex(
                bodyA_pose_interval, bodyA.edges(edgeA_id, 1));

            edgeB_vertex0 = bodyB.world_vertex(
                bodyB_pose_interval, bodyB.edges(edgeB_id, 0));
            edgeB_vertex1 = bodyB.world_vertex(
                bodyB_pose_interval, bodyB.edges(edgeB_id, 1));
        };

    const auto distance = [&](const Interval& t) {
        // Get the world vertex of the edges at time t
        Eigen::VectorX3<Interval> edgeA_vertex0, edgeA_vertex1;
        Eigen::VectorX3<Interval> edgeB_vertex0, edgeB_vertex1;
        vertex_positions(
            t, edgeA_vertex0, edgeA_vertex1, edgeB_vertex0, edgeB_vertex1);

        return geometry::line_line_signed_distance(
            edgeA_vertex0, edgeA_vertex1, edgeB_vertex0, edgeB_vertex1);
    };

    const auto is_intersection_inside_edges = [&](const Interval& t) -> bool {
        // Get the world vertex of the edges at time t
        Eigen::VectorX3<Interval> edgeA_vertex0, edgeA_vertex1;
        Eigen::VectorX3<Interval> edgeB_vertex0, edgeB_vertex1;
        vertex_positions(
            t, edgeA_vertex0, edgeA_vertex1, edgeB_vertex0, edgeB_vertex1);

        // http://stackoverflow.com/questions/563198/how-do-you-detect-where-two-line-segments-intersect
        // Search intersection between two segments
        // p + α*r :  α \in [0,1]
        // q + β*s :  β \in [0,1]
        Eigen::Vector3<Interval> p = edgeA_vertex0;
        Eigen::Vector3<Interval> r = edgeA_vertex1 - edgeA_vertex0;
        Eigen::Vector3<Interval> q = edgeB_vertex0;
        Eigen::Vector3<Interval> s = edgeB_vertex1 - edgeB_vertex0;

        // p + t * r = q + u * s  // x s
        // t(r x s) = (q - p) x s
        // t = (q - p) x s / (r x s)
        Eigen::Vector3<Interval> rxs = r.cross(s);
        if (boost::numeric::zero_in(rxs.squaredNorm())) {
            // If r × s = 0 and (q − p) × r = 0, then the two lines are
            // collinear.
            return false;
        }

        throw NotImplementedError("Edge-edge inside test not implemented!");
    };

    Interval toi_interval;
    // TODO: Set tolerance dynamically
    bool is_impacting = interval_root_finder(
        distance, is_intersection_inside_edges, Interval(0, 1), toi_interval);
    // Return a conservative time-of-impact
    toi = toi_interval.lower();
    return is_impacting;
}

// Find time-of-impact between two rigid bodies
bool compute_face_vertex_time_of_impact(
    const physics::RigidBody& bodyA,            // Body of the vertex
    const physics::Pose<double>& poseA,         // Pose of bodyA
    const physics::Pose<double>& displacementA, // Displacement of bodyA
    const size_t& vertex_id,                    // In bodyA
    const physics::RigidBody& bodyB,            // Body of the triangle
    const physics::Pose<double>& poseB,         // Pose of bodyB
    const physics::Pose<double>& displacementB, // Displacement of bodyB
    const size_t& face_id,                      // In bodyB
    double& toi)
{
    int dim = bodyA.dim();
    assert(bodyB.dim() == dim);
    assert(dim == 3);

    const physics::Pose<Interval> poseA_interval = poseA.cast<Interval>();
    const physics::Pose<Interval> poseB_interval = poseB.cast<Interval>();

    const physics::Pose<Interval> displacementA_interval =
        displacementA.cast<Interval>();
    const physics::Pose<Interval> displacementB_interval =
        displacementB.cast<Interval>();

    const auto vertex_positions = [&](const Interval& t,
                                      Eigen::VectorX3<Interval>& vertex,
                                      Eigen::VectorX3<Interval>& face_vertex0,
                                      Eigen::VectorX3<Interval>& face_vertex1,
                                      Eigen::VectorX3<Interval>& face_vertex2) {
        // Compute the poses at time t
        physics::Pose<Interval> bodyA_pose_interval =
            poseA_interval + displacementA_interval * t;
        physics::Pose<Interval> bodyB_pose_interval =
            poseB_interval + displacementB_interval * t;

        // Get the world vertex of the point at time t
        vertex = bodyA.world_vertex(bodyA_pose_interval, vertex_id);
        // Get the world vertex of the edge at time t
        face_vertex0 =
            bodyB.world_vertex(bodyB_pose_interval, bodyB.faces(face_id, 0));
        face_vertex1 =
            bodyB.world_vertex(bodyB_pose_interval, bodyB.faces(face_id, 1));
        face_vertex2 =
            bodyB.world_vertex(bodyB_pose_interval, bodyB.faces(face_id, 2));
    };

    const auto distance = [&](const Interval& t) {
        // Get the world vertex and face of the point at time t
        Eigen::VectorX3<Interval> vertex;
        Eigen::VectorX3<Interval> face_vertex0, face_vertex1, face_vertex2;
        vertex_positions(t, vertex, face_vertex0, face_vertex1, face_vertex2);

        Eigen::VectorX3<Interval> normal = geometry::triangle_normal(
            face_vertex0, face_vertex1, face_vertex2, /*normalized=*/false);

        return geometry::point_plane_signed_distance(
            vertex, face_vertex0, normal);
    };

    const auto is_point_inside_triangle = [&](const Interval& t) {
        // Get the world vertex and face of the point at time t
        Eigen::VectorX3<Interval> vertex;
        Eigen::VectorX3<Interval> face_vertex0, face_vertex1, face_vertex2;
        vertex_positions(t, vertex, face_vertex0, face_vertex1, face_vertex2);
        return geometry::is_point_inside_triangle(
            vertex, face_vertex0, face_vertex1, face_vertex2);
    };

    Interval toi_interval;
    // TODO: Set tolerance dynamically
    bool is_impacting = interval_root_finder(
        distance, is_point_inside_triangle, Interval(0, 1), toi_interval);
    // Return a conservative time-of-impact
    toi = toi_interval.lower();
    return is_impacting;
}

} // namespace ccd
