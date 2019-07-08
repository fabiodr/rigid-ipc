/**
 * Functions for optimizing functions.
 * Includes Newton's method.
 */

#pragma once

#include <Eigen/Core>

#include <opt/optimization_problem.hpp>
#include <opt/optimization_results.hpp>
#include <solvers/optimization_solver.hpp>

namespace ccd {

/**
 * @namespace ccd::opt
 * @brief Functions for optimizing functions.
 */
namespace opt {

    class NewtonSolver : public OptimizationSolver {
    protected:
        int iteration_number; ///< @brief The current iteration number.

    public:
        double absolute_tolerance; ///< @brief Convergence tolerance.
        double min_step_length;    ///< @brief Minimum step length.

        NewtonSolver();
        virtual ~NewtonSolver() override;

        /**
         * @brief Perform Newton's Method to minimize the objective, \f$f(x)\f$,
         * of the problem unconstrained.
         *
         * @param[in] problem  The optimization problem to minimize
         *                     unconstrained.
         *
         * @return The results of the optimization including the minimizer,
         * minimum, and if the optimization was successful.
         */
        virtual OptimizationResults solve(
            OptimizationProblem& problem) override;

        /**
         * @brief Solve for the newton direction for the limited degrees of
         * freedom.
         *
         * The fixed dof of x will have a delta_x of zero.
         *
         * @param[in]  gradient  Gradient of the free dof of the objective
         *                       function.
         * @param[in]  hessian   Hessian of the free dof of the objective
         *                       function.
         * @param[in]  free_dof  Indices of the free degrees of freedom.
         * @param[out] delta_x   Output full dof newton direction.
         * @param[in]  make_psd  If delta_x is not a descent direction, then
         * make the hessian positive semi-definite.
         *
         * @return Returns true if the solve was successful.
         */
        bool compute_free_direction(const Eigen::VectorXd& gradient_free,
            const Eigen::SparseMatrix<double>& hessian_free,
            Eigen::VectorXd& delta_x, bool make_psd = false);

        /**
         * @brief Solve for the Newton direction
         *        (\f$\Delta x = -H^{-1} \nabla f \f$).
         *
         * @param[in]  gradient  Gradient of the objective function.
         * @param[in]  hessian   Hessian of the objective function.
         * @param[out] delta_x   Output newton direction.
         * @param[in]  make_psd  If delta_x is not a descent direction, then
         * make the hessian positive semi-definite.
         *
         * @return Returns true if the solve was successful.
         */
        bool compute_direction(const Eigen::VectorXd& gradient,
            const Eigen::SparseMatrix<double>& hessian,
            Eigen::VectorXd& delta_x, bool make_psd = false);
    };

    /**
     * @brief Make the matrix positive definite (\f$x^T A x > 0\$).
     *
     * @param A The matrix to make positive definite.
     *
     * @return The scale of the update to the diagonal.
     */
    double make_matrix_positive_definite(Eigen::SparseMatrix<double>& A);

} // namespace opt
} // namespace ccd