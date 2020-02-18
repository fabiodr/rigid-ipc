#pragma once

#include <vector>

#include <Eigen/Core>

#include <ccd/collision_detection.hpp>
#include <physics/rigid_body_assembler.hpp>

namespace ccd {

/// @brief Find all collisions in one time step.
void detect_collisions(
    const physics::RigidBodyAssembler& bodies,
    const std::vector<physics::Pose<double>>& poses,
    const std::vector<physics::Pose<double>>& displacements,
    const int collision_types,
    EdgeVertexImpacts& ev_impacts,
    EdgeEdgeImpacts& ee_impacts,
    FaceVertexImpacts& fv_impacts,
    DetectionMethod method = DetectionMethod::HASH_GRID);

///////////////////////////////////////////////////////////////////////////////
// Broad-Phase CCD
///////////////////////////////////////////////////////////////////////////////

/// @brief Use broad-phase method to create a set of candidate collisions.
void detect_collision_candidates(
    const physics::RigidBodyAssembler& bodies,
    const std::vector<physics::Pose<double>>& poses,
    const std::vector<physics::Pose<double>>& displacements,
    const int collision_types,
    EdgeVertexCandidates& ev_candidates,
    EdgeEdgeCandidates& ee_candidates,
    FaceVertexCandidates& fv_candidates,
    DetectionMethod method = DetectionMethod::HASH_GRID,
    const double inflation_radius = 0.0);

/// @brief Use a hash grid method to create a set of all candidate collisions.
void detect_collision_candidates_hash_grid(
    const physics::RigidBodyAssembler& bodies,
    const std::vector<physics::Pose<double>>& poses,
    const std::vector<physics::Pose<double>>& displacements,
    const int collision_types,
    EdgeVertexCandidates& ev_candidates,
    EdgeEdgeCandidates& ee_candidates,
    FaceVertexCandidates& fv_candidates,
    const double inflation_radius = 0.0);

///////////////////////////////////////////////////////////////////////////////
// Narrow-Phase CCD
///////////////////////////////////////////////////////////////////////////////

void detect_collisions_from_candidates(
    const physics::RigidBodyAssembler& bodies,
    const std::vector<physics::Pose<double>>& poses,
    const std::vector<physics::Pose<double>>& displacements,
    const EdgeVertexCandidates& ev_candidates,
    const EdgeEdgeCandidates& ee_candidates,
    const FaceVertexCandidates& fv_candidates,
    EdgeVertexImpacts& ev_impacts,
    EdgeEdgeImpacts& ee_impacts,
    FaceVertexImpacts& fv_impacts);

/// @brief Determine if a single edge-vertext pair intersects.
void detect_edge_vertex_collisions_narrow_phase(
    const physics::RigidBodyAssembler& bodies,
    const std::vector<physics::Pose<double>>& poses,
    const std::vector<physics::Pose<double>>& displacements,
    const EdgeVertexCandidate& ev_candidate,
    EdgeVertexImpacts& ev_impacts);

void detect_edge_edge_collisions_narrow_phase(
    const physics::RigidBodyAssembler& bodies,
    const std::vector<physics::Pose<double>>& poses,
    const std::vector<physics::Pose<double>>& displacements,
    const EdgeEdgeCandidate& ee_candidate,
    EdgeEdgeImpacts& ee_impacts);

void detect_face_vertex_collisions_narrow_phase(
    const physics::RigidBodyAssembler& bodies,
    const std::vector<physics::Pose<double>>& poses,
    const std::vector<physics::Pose<double>>& displacements,
    const FaceVertexCandidate& fv_candidate,
    FaceVertexImpacts& fv_impacts);

} // namespace ccd