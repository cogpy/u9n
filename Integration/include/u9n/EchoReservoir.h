// EchoReservoir.h — portable leaky Echo State Network (no Unreal dependency).
//
// x(t+1) = (1-a) x(t) + a tanh(W x(t) + Win u(t) + b)
// Readout trained by closed-form ridge regression:  Wout = Y X^T (X X^T + λI)^-1
//
// Defaults follow u9n CLAUDE.md: spectral radius 0.9, leak rate 0.3.
#pragma once

#include <Eigen/Dense>
#include <random>
#include <vector>
#include <cmath>

namespace u9n {

struct ReservoirConfig {
    int    Size           = 512;
    int    InputDim       = 1;
    double SpectralRadius = 0.9;
    double LeakRate       = 0.3;
    double InputScale     = 0.5;
    double Density        = 0.1;
    double Ridge          = 1e-6;
    unsigned Seed         = 42;
};

class EchoReservoir {
public:
    explicit EchoReservoir(const ReservoirConfig& cfg = {}) : Cfg(cfg), Rng(cfg.Seed) {
        const int N = Cfg.Size;
        std::uniform_real_distribution<double> U(-1.0, 1.0);
        std::uniform_real_distribution<double> P(0.0, 1.0);
        W = Eigen::MatrixXd::Zero(N, N);
        for (int i = 0; i < N; ++i)
            for (int j = 0; j < N; ++j)
                if (P(Rng) < Cfg.Density) W(i, j) = U(Rng);
        const double rho = SpectralRadiusOf(W);
        if (rho > 1e-12) W *= Cfg.SpectralRadius / rho;
        Win = Eigen::MatrixXd::NullaryExpr(N, Cfg.InputDim, [&] { return U(Rng) * Cfg.InputScale; });
        Bias = Eigen::VectorXd::NullaryExpr(N, [&] { return U(Rng) * 0.1; });
        X = Eigen::VectorXd::Zero(N);
    }

    const Eigen::VectorXd& Step(const Eigen::VectorXd& u) {
        X = (1.0 - Cfg.LeakRate) * X + Cfg.LeakRate * (W * X + Win * u + Bias).array().tanh().matrix();
        return X;
    }
    const Eigen::VectorXd& Step(double u) { return Step(Eigen::VectorXd::Constant(1, u)); }

    void Reset() { X.setZero(); }

    // Episode key: concatenated states along the trajectory (every `stride` steps, always
    // including the last). The final state alone forgets early inputs (fading memory);
    // the trajectory keeps the whole episode.
    Eigen::VectorXd EncodeTrajectory(const std::vector<double>& seq, int stride = 1) {
        Reset();
        std::vector<Eigen::VectorXd> kept;
        for (int t = 0; t < (int)seq.size(); ++t) {
            Step(seq[t]);
            if ((t + 1) % stride == 0 || t + 1 == (int)seq.size()) kept.push_back(X);
        }
        Eigen::VectorXd key(Cfg.Size * (Eigen::Index)kept.size());
        for (size_t i = 0; i < kept.size(); ++i) key.segment(Cfg.Size * (Eigen::Index)i, Cfg.Size) = kept[i];
        return key;
    }

    // Run inputs, collect states (columns) after washout.
    Eigen::MatrixXd Harvest(const std::vector<Eigen::VectorXd>& inputs, int washout) {
        Eigen::MatrixXd S(Cfg.Size, (int)inputs.size() - washout);
        for (int t = 0; t < (int)inputs.size(); ++t) {
            Step(inputs[t]);
            if (t >= washout) S.col(t - washout) = X;
        }
        return S;
    }

    // Ridge readout: states S (N×T, bias row appended), targets Y (K×T).
    static Eigen::MatrixXd FitRidge(const Eigen::MatrixXd& S, const Eigen::MatrixXd& Y, double ridge) {
        Eigen::MatrixXd Sb(S.rows() + 1, S.cols());
        Sb << S, Eigen::RowVectorXd::Ones(S.cols());
        Eigen::MatrixXd A = Sb * Sb.transpose();
        A.diagonal().array() += ridge;
        return (A.ldlt().solve(Sb * Y.transpose())).transpose();
    }
    static Eigen::MatrixXd Predict(const Eigen::MatrixXd& Wout, const Eigen::MatrixXd& S) {
        Eigen::MatrixXd Sb(S.rows() + 1, S.cols());
        Sb << S, Eigen::RowVectorXd::Ones(S.cols());
        return Wout * Sb;
    }

    // Gelfand estimate ρ ≈ (‖M^k v‖ / ‖v‖)^(1/k): O(k·N²) instead of an O(N³) eigensolve,
    // and robust to complex-conjugate dominant pairs where plain power iteration oscillates.
    static double SpectralRadiusOf(const Eigen::MatrixXd& M, int iters = 200) {
        Eigen::VectorXd v = Eigen::VectorXd::Ones(M.rows()).normalized();
        double logGrowth = 0;
        for (int k = 0; k < iters; ++k) {
            v = M * v;
            const double n = v.norm();
            if (n < 1e-300) return 0.0;
            logGrowth += std::log(n);
            v /= n;
        }
        return std::exp(logGrowth / iters);
    }

    const Eigen::VectorXd& State() const { return X; }
    const ReservoirConfig& Config() const { return Cfg; }
    std::mt19937& Random() { return Rng; }

private:
    ReservoirConfig Cfg;
    std::mt19937 Rng;
    Eigen::MatrixXd W, Win;
    Eigen::VectorXd Bias, X;
};

} // namespace u9n
