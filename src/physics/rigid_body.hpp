#pragma once

#include <vector>

#include <Eigen/Core>
#include <Eigen/Sparse>
#include <utils/eigen_ext.hpp>

#include <physics/mass.hpp>
#include <physics/pose.hpp>

#include <autodiff/autodiff.h>

#include <iostream>

namespace ccd {
namespace physics {

    class RigidBody {

    protected:
        /**
         * @brief Create rigid body with center of mass at \f$\vec{0}\f$.
         *
         * @param vertices  Vertices of the rigid body in body space
         * @param faces     Vertices pairs defining the topology of the rigid
         *                  body
         */
        RigidBody(
            const Eigen::MatrixXd& vertices,
            const Eigen::MatrixXi& edges,
            const Eigen::MatrixXi& faces,
            const Pose<double>& pose,
            const Pose<double>& velocity,
            const Pose<double>& force,
            const double density,
            const Eigen::VectorX6b& is_dof_fixed,
            const bool oriented,
            const int group_id);

    public:
        static RigidBody from_points(
            const Eigen::MatrixXd& vertices,
            const Eigen::MatrixXi& edges,
            const Eigen::MatrixXi& faces,
            const Pose<double>& pose,
            const Pose<double>& velocity,
            const Pose<double>& force,
            const double density,
            const Eigen::VectorX6b& is_dof_fixed,
            const bool oriented,
            const int group_id);

        // Faceless version for convienence (useful for 2D)
        static RigidBody from_points(
            const Eigen::MatrixXd& vertices,
            const Eigen::MatrixXi& edges,
            const Pose<double>& pose,
            const Pose<double>& velocity,
            const Pose<double>& force,
            const double density,
            const Eigen::VectorX6b& is_dof_fixed,
            const bool oriented,
            const int group_id)
        {
            return from_points(
                vertices, edges, Eigen::MatrixXi(), pose, velocity, force,
                density, is_dof_fixed, oriented, group_id);
        }

        enum Step { PREVIOUS_STEP = 0, CURRENT_STEP };

        // --------------------------------------------------------------------
        // State Functions
        // --------------------------------------------------------------------

        /// @brief: computes vertices position for current or previous state
        Eigen::MatrixXd world_vertices(const Step step = CURRENT_STEP) const
        {
            return world_vertices(step == PREVIOUS_STEP ? pose_prev : pose);
        }
        Eigen::MatrixXd world_vertices_t0() const
        {
            return world_vertices(PREVIOUS_STEP);
        }
        Eigen::MatrixXd world_vertices_t1() const
        {
            return world_vertices(CURRENT_STEP);
        }

        Eigen::MatrixXd world_velocities() const;

        // --------------------------------------------------------------------
        // CCD Functions
        // --------------------------------------------------------------------

        /// @brief Computes vertices position for given state.
        /// @return The positions of all vertices in 'world space',
        ///         taking into account the given body's position.
        template <typename T>
        Eigen::MatrixX<T> world_vertices(
            const Eigen::MatrixXX3<T>& R, const Eigen::VectorX3<T>& p) const;
        template <typename T>
        Eigen::MatrixX<T> world_vertices(const Pose<T>& _pose) const
        {
            return world_vertices<T>(
                _pose.construct_rotation_matrix(), _pose.position);
        }
        template <typename T>
        Eigen::MatrixX<T> world_vertices(const Eigen::VectorX6<T>& dof) const
        {
            return world_vertices(Pose<T>(dof));
        }

        template <typename T>
        Eigen::VectorX3<T> world_vertex(
            const Eigen::MatrixXX3<T>& R,
            const Eigen::VectorX3<T>& p,
            const int vertex_idx) const;
        template <typename T>
        Eigen::VectorX3<T>
        world_vertex(const Pose<T>& _pose, const int vertex_idx) const
        {
            return world_vertex<T>(
                _pose.construct_rotation_matrix(), _pose.position, vertex_idx);
        }
        template <typename T>
        Eigen::VectorX3<T>
        world_vertex(const Eigen::VectorX6<T>& dof, const int vertex_idx) const
        {
            return world_vertex<T>(Pose<T>(dof), vertex_idx);
        }

        Eigen::MatrixXd world_vertices_gradient(const Pose<double>& pose) const;

        Eigen::MatrixXd
        world_vertices_gradient_exact(const Pose<double>& pose) const;
        std::vector<Eigen::MatrixXd>
        world_vertices_hessian_exact(const Pose<double>& velocity) const;

        int dim() const { return vertices.cols(); }
        int ndof() const { return pose.ndof(); }
        int pos_ndof() const { return pose.pos_ndof(); }
        int rot_ndof() const { return pose.rot_ndof(); }

        /// @brief Group id of this body
        int group_id;

        // --------------------------------------------------------------------
        // Geometry
        // --------------------------------------------------------------------
        Eigen::MatrixXd vertices; ///< Vertices positions in body space
        Eigen::MatrixXi edges;    ///< Vertices connectivity
        Eigen::MatrixXi faces;    ///< Vertices connectivity

        double average_edge_length; ///< Average edge length

        /// @brief total mass (M) of the rigid body
        double mass;
        /// @breif moment of inertia measured with respect to the principal axes
        Eigen::VectorX3d moment_of_inertia;
        /// @brief rotation from the principal axes to the input orientation
        Eigen::MatrixXX3d R0;
        /// @brief maximum distance from CM to a vertex
        double r_max;

        /// @brief Flag to indicate if dof is fixed (doesnt' change)
        Eigen::VectorX6b is_dof_fixed;
        Eigen::MatrixXd mass_matrix;
        Eigen::MatrixXd inv_mass_matrix;

        bool is_oriented; ///< use edge orientation for normals

        // --------------------------------------------------------------------
        // State
        // --------------------------------------------------------------------
        /// @brief current timestep position and rotation of the center of mass
        Pose<double> pose;
        /// @brief previous timestep position and rotation of the center of mass
        Pose<double> pose_prev;

        /// @brief current timestep velocity of the center of mass
        Pose<double> velocity;
        /// @brief previous timestep velocity of the center of mass
        Pose<double> velocity_prev;

        /// @brief external force acting on the body
        Pose<double> force;
    };

} // namespace physics
} // namespace ccd

#include "rigid_body.tpp"
