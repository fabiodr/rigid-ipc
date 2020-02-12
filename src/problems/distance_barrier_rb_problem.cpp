#include "distance_barrier_rb_problem.hpp"

#include <finitediff.hpp>

#include <constants.hpp>
#include <geometry/distance.hpp>
#include <multiprecision.hpp>
#include <utils/tensor.hpp>

#include <logger.hpp>
#include <profiler.hpp>

namespace ccd {

namespace opt {

    DistanceBarrierRBProblem::DistanceBarrierRBProblem(const std::string& name)
        : RigidBodyProblem(name)
    {
    }

    void DistanceBarrierRBProblem::settings(const nlohmann::json& params)
    {
        constraint_.settings(params["distance_barrier_constraint"]);
        opt_solver_.settings(params["barrier_solver"]);
        opt_solver_.set_problem(*this);
        nlohmann::json inner_solver_settings =
            params[params["barrier_solver"]["inner_solver"].get<std::string>()];
        opt_solver_.inner_solver_settings(inner_solver_settings);
        RigidBodyProblem::settings(params["rigid_body_problem"]);
    }

    nlohmann::json DistanceBarrierRBProblem::state() const
    {
        nlohmann::json json = RigidBodyProblem::state();
        if (debug_min_distance_ < 0) {
            json["min_distance"] = nullptr;
        } else {
            json["min_distance"] = debug_min_distance_;
        }
        return json;
    }

    bool DistanceBarrierRBProblem::simulation_step(const double time_step)
    {
        bool has_collision = RigidBodyProblem::simulation_step(time_step);

        Eigen::VectorXd sigma = m_assembler.m_pose_to_dof
            * physics::Pose<double>::poses_to_dofs(m_assembler.rb_poses_t0());
        debug_min_distance_ = debug_min_distance(sigma);
        if (debug_min_distance_ >= 0) {
            spdlog::info(
                "candidate_step min_distance={:.8e}", debug_min_distance_);

            // our constraint is really d > min_d, we want to run
            // the optimization when we end the step with small
            // distances
            if (debug_min_distance_ <= constraint_.min_distance) {
                has_collision = true;
            }
        }

        return has_collision;
    }

    bool DistanceBarrierRBProblem::take_step(
        const Eigen::VectorXd& sigma, const double time_step)
    {
        debug_min_distance_ = debug_min_distance(sigma);
        if (debug_min_distance_ < 0) {
            spdlog::info("final_step min_distance=N/A");
        } else {
            spdlog::info("final_step min_distance={:.8e}", debug_min_distance_);
        }

        return RigidBodyProblem::take_step(sigma, time_step);
    }

    void DistanceBarrierRBProblem::eval_f_and_fdiff(
        const Eigen::VectorXd& sigma,
        double& f_uk,
        Eigen::VectorXd& f_uk_grad,
        Eigen::SparseMatrix<double>& f_uk_hessian)
    {
        f_uk = eval_f(sigma);
        f_uk_grad = eval_grad_f(sigma);
        f_uk_hessian = eval_hessian_f(sigma);
    }

    void DistanceBarrierRBProblem::eval_f_and_fdiff(
        const Eigen::VectorXd& sigma, double& f_uk, Eigen::VectorXd& f_uk_grad)
    {
        f_uk = eval_f(sigma);
        f_uk_grad = eval_grad_f(sigma);
    }

#if defined(DEBUG_LINESEARCH) || defined(DEBUG_COLLISIONS)
    Eigen::MatrixXd
    DistanceBarrierRBProblem::debug_vertices(const Eigen::VectorXd& sigma) const
    {
        Eigen::VectorXd qk = m_assembler.m_dof_to_pose * sigma;
        return m_assembler.world_vertices(qk);
    }
#endif
    double DistanceBarrierRBProblem::debug_min_distance(
        const Eigen::VectorXd& sigma) const
    {
        Eigen::VectorXd qk = m_assembler.m_dof_to_pose * sigma;
        Eigen::MatrixXd uk = m_assembler.world_vertices(qk) - vertices_t0;

        Eigen::VectorXd d;
        constraint_.debug_compute_distances(uk, d);
        if (d.rows() > 0) {
            return d.minCoeff();
        }
        return -1;
    }

    Eigen::VectorXd
    DistanceBarrierRBProblem::eval_g(const Eigen::VectorXd& sigma)
    {
        Eigen::VectorXd qk = m_assembler.m_dof_to_pose * sigma;
        Eigen::MatrixXd uk = m_assembler.world_vertices(qk) - vertices_t0;

        Eigen::VectorXd g_uk;
        constraint_.compute_constraints(uk, g_uk);
        return g_uk;
    }

    bool DistanceBarrierRBProblem::has_collisions(
        const Eigen::VectorXd& sigma_i, const Eigen::VectorXd& sigma_j) const
    {
        Eigen::VectorXd qi = m_assembler.m_dof_to_pose * sigma_i;
        Eigen::MatrixXd xi = m_assembler.world_vertices(qi);

        Eigen::VectorXd qj = m_assembler.m_dof_to_pose * sigma_j;
        Eigen::MatrixXd xj = m_assembler.world_vertices(qj);
        return constraint_.has_active_collisions(xi, xj);
    }

    Eigen::Matrix<Multiprecision, Eigen::Dynamic, 1>
    DistanceBarrierRBProblem::eval_mp_g(const Eigen::VectorXd& /*sigma*/)
    {
        // Eigen::VectorXd qk = m_assembler.m_dof_to_pose * sigma;
        // Eigen::MatrixXd uk = m_assembler.world_vertices(qk) - vertices_t0;

        Eigen::Matrix<Multiprecision, Eigen::Dynamic, 1> g_uk;
        // EdgeVertexCandidates ev_candidates;
        // auto check = constraint_.get_active_barrier_set(uk, ev_candidates);

        // if (check == DistanceBarrierConstraint::HAS_COLLISION) {
        g_uk.resize(1);
        g_uk(0) = Multiprecision(std::numeric_limits<double>::infinity(), 256);

        // } else {
        //     Eigen::Matrix<Multiprecision, Eigen::Dynamic, Eigen::Dynamic>
        //     uk_mp;
        //
        //     uk_mp.resizeLike(uk);
        //     for (int i = 0; i < uk.size(); ++i) {
        //         uk_mp(i) = Multiprecision(uk(i), 256);
        //     }
        //     constraint_.compute_candidates_constraints<Multiprecision>(
        //         uk_mp, ev_candidates, g_uk);
        // }
        return g_uk;
    }

    Eigen::MatrixXd
    DistanceBarrierRBProblem::eval_jac_g(const Eigen::VectorXd& sigma)
    {
        NAMED_PROFILE_POINT("eval_jac_g__update_constraints", UPDATE)
        NAMED_PROFILE_POINT("eval_jac_g__eval_jac", EVAL)

        PROFILE_START(UPDATE)

        Eigen::VectorXd qk = m_assembler.m_dof_to_pose * sigma;
        Eigen::MatrixXd uk = m_assembler.world_vertices(qk) - vertices_t0;

        EdgeVertexCandidates ev_candidates;
        constraint_.get_active_barrier_set(uk, ev_candidates);
        PROFILE_END(UPDATE)

        PROFILE_START(EVAL)
        Eigen::MatrixXd gx_jacobian = eval_jac_g_core(sigma, ev_candidates);
        PROFILE_END(EVAL)

#ifdef WITH_DERIVATIVE_CHECK
        bool is_grad_correct = compare_jac_g(sigma, ev_candidates, gx_jacobian);
        assert(is_grad_correct);
#endif
        return gx_jacobian;
    }

    Eigen::MatrixXd DistanceBarrierRBProblem::eval_jac_g_full(
        const Eigen::VectorXd& sigma, const EdgeVertexCandidates& ev_candidates)
    {
        typedef AutodiffType<Eigen::Dynamic> Diff;
        Diff::activate(num_vars_);
        assert(sigma.size() == num_vars_);

        Diff::D1VectorXd d_sigma = Diff::d1vars(0, sigma);

        Diff::D1VectorXd d_qk =
            m_assembler.m_dof_to_pose.cast<Diff::DDouble1>() * d_sigma;
        Diff::D1MatrixXd d_uk = m_assembler.world_vertices<Diff::DDouble1>(d_qk)
            - vertices_t0.cast<Diff::DDouble1>();
        Diff::D1VectorXd d_g_uk;
        constraint_.compute_candidates_constraints<Diff::DDouble1>(
            d_uk, ev_candidates, d_g_uk);

        assert(ev_candidates.size() == d_g_uk.rows());
        return Diff::get_gradient(d_g_uk);
    }

    std::vector<Eigen::SparseMatrix<double>>
    DistanceBarrierRBProblem::eval_hessian_g(const Eigen::VectorXd& sigma)
    {
        NAMED_PROFILE_POINT("eval_hess_g__update_constraints", UPDATE)
        NAMED_PROFILE_POINT("eval_hess_g__eval", EVAL)

        PROFILE_START(UPDATE)
        Eigen::VectorXd qk = m_assembler.m_dof_to_pose * sigma;
        Eigen::MatrixXd uk = m_assembler.world_vertices(qk) - vertices_t0;

        EdgeVertexCandidates ev_candidates;
        constraint_.get_active_barrier_set(uk, ev_candidates);
        PROFILE_END(UPDATE)

        std::vector<Eigen::SparseMatrix<double>> gx_hessian;
        PROFILE_START(EVAL)
        gx_hessian = eval_hessian_g_core(sigma, ev_candidates);
        PROFILE_END(EVAL)

        return gx_hessian;
    }

    void DistanceBarrierRBProblem::eval_g_and_gdiff(
        const Eigen::VectorXd& sigma,
        Eigen::VectorXd& gx,
        Eigen::MatrixXd& gx_jacobian,
        std::vector<Eigen::SparseMatrix<double>>& gx_hessian)
    {
        NAMED_PROFILE_POINT("eval_g_and_gdiff__update_constraints", UPDATE)
        NAMED_PROFILE_POINT("eval_hess_g__eval_grad", EVAL_GRAD)
        NAMED_PROFILE_POINT("eval_hess_g__eval_hess", EVAL_HESS)

        PROFILE_START(UPDATE)
        Eigen::VectorXd qk = m_assembler.m_dof_to_pose * sigma;
        Eigen::MatrixXd uk = m_assembler.world_vertices(qk) - vertices_t0;

        EdgeVertexCandidates ev_candidates;
        constraint_.get_active_barrier_set(uk, ev_candidates);
        PROFILE_END(UPDATE)

        constraint_.compute_candidates_constraints(uk, ev_candidates, gx);

        PROFILE_START(EVAL_GRAD)
        gx_jacobian = eval_jac_g_core(sigma, ev_candidates);
        PROFILE_END(EVAL_GRAD)

        PROFILE_START(EVAL_HESS)
        gx_hessian = eval_hessian_g_core(sigma, ev_candidates);
        PROFILE_END(EVAL_HESS)

#ifdef WITH_DERIVATIVE_CHECK
        bool is_grad_correct = compare_jac_g(sigma, ev_candidates, gx_jacobian);
        assert(is_grad_correct);
#endif
    }

    Eigen::MatrixXd DistanceBarrierRBProblem::eval_jac_g_core(
        const Eigen::VectorXd& sigma,
        const EdgeVertexCandidates& distance_candidates)
    {
        if (dim() != 2) {
            throw NotImplementedError(
                "DistanceBarrierRBProblem::eval_jac_g_core() has not been "
                "implmented for 3D!");
        }
        Eigen::MatrixXd jac_g =
            Eigen::MatrixXd::Zero(distance_candidates.size(), num_vars_);

        typedef AutodiffType<Eigen::Dynamic> Diff;
        int ndof = physics::Pose<double>::dim_to_ndof(dim());
        Diff::activate(2 * ndof);

        for (size_t i = 0; i < distance_candidates.size(); ++i) {
            const auto& ev_candidate = distance_candidates[i];

            RB2Candidate rbc;
            extract_local_system(ev_candidate, rbc);

            Eigen::VectorXd gradient =
                distance_barrier<Diff::DDouble1>(sigma, rbc).getGradient();

            size_t cstr_id = i;
            jac_g.block(int(cstr_id), ndof* rbc.vertex_body_id, 1, ndof) =
                gradient.head(ndof).transpose();
            jac_g.block(int(cstr_id), ndof* rbc.edge_body_id, 1, ndof) =
                gradient.tail(ndof).transpose();
        }

        return jac_g;
    }

    std::vector<Eigen::SparseMatrix<double>>
    DistanceBarrierRBProblem::eval_hessian_g_core(
        const Eigen::VectorXd& sigma,
        const EdgeVertexCandidates& distance_candidates)
    {
        if (dim() != 2) {
            throw NotImplementedError(
                "DistanceBarrierRBProblem::eval_hessian_g_core() has not been "
                "implmented for 3D!");
        }

        std::vector<Eigen::SparseMatrix<double>> gx_hessian;
        gx_hessian.resize(distance_candidates.size());

        typedef AutodiffType<Eigen::Dynamic> Diff;
        typedef Eigen::Triplet<double> M;
        std::vector<M> triplets;

        for (size_t i = 0; i < distance_candidates.size(); ++i) {
            const auto& ev_candidate = distance_candidates[i];

            RB2Candidate rbc;
            extract_local_system(ev_candidate, rbc);

            Eigen::MatrixXd hessian =
                distance_barrier<Diff::DDouble2>(sigma, rbc).getHessian();

            triplets.clear();
            triplets.reserve(6 * 6);

            int bodies[2] = { rbc.vertex_body_id, rbc.edge_body_id };

            for (int b_i = 0; b_i < 2; b_i++) {
                for (int b_j = 0; b_j < 2; b_j++) {
                    for (int dim_i = 0; dim_i < 3; dim_i++) {
                        for (int dim_j = 0; dim_j < 3; dim_j++) {
                            double v =
                                hessian(3 * b_i + dim_i, 3 * b_j + dim_j);
                            int r = 3 * bodies[b_i] + dim_i;
                            int c = 3 * bodies[b_j] + dim_j;
                            triplets.push_back(M(r, c, v));
                        }
                    }
                }
            }
            Eigen::SparseMatrix<double> global_el_hessian(num_vars_, num_vars_);
            global_el_hessian.setFromTriplets(triplets.begin(), triplets.end());

            size_t cstr_id = i;
            gx_hessian[cstr_id] = global_el_hessian;
        }

        return gx_hessian;
    }

    template <typename T>
    T DistanceBarrierRBProblem::distance_barrier(
        const Eigen::VectorXd& sigma, const RB2Candidate& rbc)
    {
        if (dim() != 2) {
            throw NotImplementedError(
                "DistanceBarrierRBProblem::distance_barrier<double>() has not "
                "been implmented for 3D!");
        }
        T d = distance<T>(sigma, rbc);
        T barrier = constraint_.distance_barrier<T>(d);
        return barrier;
    }

    template <typename T>
    T DistanceBarrierRBProblem::distance(
        const Eigen::VectorXd& sigma, const RB2Candidate& rbc)
    {
        if (dim() != 2) {
            throw NotImplementedError(
                "DistanceBarrierRBProblem::distance<T>() has not been "
                "implmented for 3D!");
        }
        typedef AutodiffType<Eigen::Dynamic> Diff;

        int ndof = physics::Pose<double>::dim_to_ndof(dim());
        Diff::activate(2 * ndof);
        typedef Eigen::Matrix<T, Eigen::Dynamic, 1> VectorXT;

        VectorXT sigma_E, sigma_V, pose_E, pose_V;

        sigma_V =
            Diff::dTvars<T>(0, sigma.segment(ndof * rbc.vertex_body_id, ndof));
        sigma_E =
            Diff::dTvars<T>(ndof, sigma.segment(ndof * rbc.edge_body_id, ndof));

        pose_V = sigma_V.array()
            * m_assembler.m_dof_to_pose.diagonal()
                  .segment(ndof * rbc.vertex_body_id, ndof)
                  .cast<T>()
                  .array();

        pose_E = sigma_E.array()
            * m_assembler.m_dof_to_pose.diagonal()
                  .segment(ndof * rbc.edge_body_id, ndof)
                  .cast<T>()
                  .array();

        const auto& rbs = m_assembler.m_rbs;
        VectorXT da = rbs[size_t(rbc.edge_body_id)].world_vertex<T>(
            pose_E, rbc.edge0_local_id);
        VectorXT db = rbs[size_t(rbc.edge_body_id)].world_vertex<T>(
            pose_E, rbc.edge1_local_id);
        VectorXT dc = rbs[size_t(rbc.vertex_body_id)].world_vertex<T>(
            pose_V, rbc.vertex_local_id);

        // T distance = sqrt(point_to_edge_sq_distance<T>(da, db, dc));
        T distance = ccd::geometry::point_segment_distance<T>(dc, da, db);
        return distance;
    }

    template <>
    double DistanceBarrierRBProblem::distance<double>(
        const Eigen::VectorXd& sigma, const RB2Candidate& rbc)
    {
        if (dim() != 2) {
            throw NotImplementedError(
                "DistanceBarrierRBProblem::distance<double>() has not been "
                "implmented for 3D!");
        }

        Eigen::VectorXd sigma_E, sigma_V, pose_E, pose_V;

        int ndof = physics::Pose<double>::dim_to_ndof(dim());
        sigma_V = sigma.segment(ndof * rbc.vertex_body_id, ndof);
        sigma_E = sigma.segment(ndof * rbc.edge_body_id, ndof);

        pose_V = sigma_V.array()
            * m_assembler.m_dof_to_pose.diagonal()
                  .segment(ndof * rbc.vertex_body_id, ndof)
                  .array();

        pose_E = sigma_E.array()
            * m_assembler.m_dof_to_pose.diagonal()
                  .segment(ndof * rbc.edge_body_id, ndof)
                  .array();

        const auto& rbs = m_assembler.m_rbs;
        Eigen::VectorXd da = rbs[size_t(rbc.edge_body_id)].world_vertex<double>(
            pose_E, rbc.edge0_local_id);
        Eigen::VectorXd db = rbs[size_t(rbc.edge_body_id)].world_vertex<double>(
            pose_E, rbc.edge1_local_id);
        Eigen::VectorXd dc =
            rbs[size_t(rbc.vertex_body_id)].world_vertex<double>(
                pose_V, rbc.vertex_local_id);

        // double distance = sqrt(point_to_edge_sq_distance<double>(da, db,
        // dc));
        double distance =
            ccd::geometry::point_segment_distance<double>(dc, da, db);
        return distance;
    }

    void DistanceBarrierRBProblem::extract_local_system(
        const EdgeVertexCandidate& ev_candidate, RB2Candidate& rbc)
    {
        if (dim() != 2) {
            throw NotImplementedError(
                "DistanceBarrierRBProblem::extract_local_system() has not been "
                "implmented for 3D!");
        }
        const long v_id = ev_candidate.vertex_index;
        const int e0_id = m_assembler.m_edges(ev_candidate.edge_index, 0);
        const int e1_id = m_assembler.m_edges(ev_candidate.edge_index, 1);
        int body_V_id, lv_id, body_E_id, le0_id, le1_id;

        m_assembler.global_to_local(v_id, body_V_id, lv_id);
        m_assembler.global_to_local(e0_id, body_E_id, le0_id);
        m_assembler.global_to_local(e1_id, body_E_id, le1_id);
        rbc.vertex_body_id = body_V_id;
        rbc.edge_body_id = body_E_id;
        rbc.vertex_local_id = lv_id;
        rbc.edge0_local_id = le0_id;
        rbc.edge1_local_id = le1_id;
    }

    bool DistanceBarrierRBProblem::compare_fd(
        const Eigen::VectorXd& sigma,
        const EdgeVertexCandidate& ev_candidate,
        const Eigen::VectorXd& grad)
    {
        if (dim() != 2) {
            throw NotImplementedError("DistanceBarrierRBProblem::compare_fd() "
                                      "has not been implmented for 3D!");
        }
        typedef AutodiffType<Eigen::Dynamic> Diff;
        int ndof = physics::Pose<double>::dim_to_ndof(dim());
        Diff::activate(2 * ndof);

        RB2Candidate rbc;
        extract_local_system(ev_candidate, rbc);
        Diff::DDouble1 d = distance<Diff::DDouble1>(sigma, rbc);

        // distance finite diff
        auto f = [&](const Eigen::VectorXd& sigma_k) -> double {
            double dk = distance<double>(sigma_k, rbc);
            return dk;
        };

        // distance finite diff
        Eigen::VectorXd approx_grad;
        Eigen::VectorXd exact_grad(sigma.rows());
        Eigen::VectorXd local_exact_grad = d.getGradient();
        exact_grad.setZero();
        exact_grad.segment(ndof * rbc.vertex_body_id, ndof) =
            local_exact_grad.head(ndof);
        exact_grad.segment(ndof * rbc.edge_body_id, ndof) =
            local_exact_grad.tail(ndof);

        finite_gradient(
            sigma, f, approx_grad, fd::AccuracyOrder::SECOND,
            Constants::FINITE_DIFF_H);
        if (!fd::compare_gradient(
                approx_grad, exact_grad, Constants::FINITE_DIFF_TEST,
                fmt::format(
                    "check_finite_diff DISTANCE barrier_eps={:3e} d={:3e}",
                    constraint_.get_barrier_epsilon(), d.getValue()))) {
        }

        // barrier finite diff - chain rule
        double distance_grad = constraint_.distance_barrier_grad(d.getValue());
        approx_grad = approx_grad * distance_grad;

        return fd::compare_gradient(
            approx_grad, grad, Constants::FINITE_DIFF_TEST,
            fmt::format(
                "check_finite_diff BARRIER barrier_eps={:3e} d={:3e}",
                constraint_.get_barrier_epsilon(), d.getValue()));
    }

    bool DistanceBarrierRBProblem::compare_jac_g(
        const Eigen::VectorXd& sigma,
        const EdgeVertexCandidates& ev_candidates,
        const Eigen::MatrixXd& jac_g)
    {
        auto jac_full = eval_jac_g_full(sigma, ev_candidates);

        bool pass = fd::compare_jacobian(
            jac_full, jac_g, /*test_eps=*/Constants::FULL_GRADIENT_TEST);
        if (!pass) {
            spdlog::error("autodiff_gradients_dont_match");
        }

        for (size_t i = 0; i < ev_candidates.size(); ++i) {
            const auto& ev = ev_candidates[i];
            compare_fd(sigma, ev, jac_full.row(int(i)));
            compare_fd(sigma, ev, jac_g.row(int(i)));
        }

        return pass;
    }

} // namespace opt
} // namespace ccd
