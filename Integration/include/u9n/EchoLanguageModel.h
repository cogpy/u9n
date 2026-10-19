// EchoLanguageModel.h — character-level language model on the Deep Tree Echo reservoir.
//
// Two memories, one prediction:
//   echo     p_e(c | history) = softmax(β · W [h; h⊙h; 1])  where h is the reservoir state after
//            reading the history one-hot, character by character. W is a ridge readout whose
//            Gram matrix is accumulated in batches, so states are never stored. β is calibrated.
//   episodic p_n(c | last k chars): Witten-Bell interpolated character n-gram counts up to
//            order k, held sparsely under 64-bit context hashes. With Online set, the counts
//            keep learning while reading: each character is predicted first, then counted.
//   mixture  log p(c) = g_d,e · log p_e(c) + g_d,n · log p_n(c) − log Z, a geometric (product-of-
//            experts) blend whose weight pair g_d is chosen by d, the longest context the counts
//            have seen. Where the experts agree the product sharpens; where one is unsure it
//            flattens. g_d starts at the best linear weight λ_d, is fitted by gradient descent on
//            validation text, and keeps descending while reading (predict first, then step):
//            ∂(−log p(y))/∂g_k = E_p[log p_k] − log p_k(y).
// The reservoir carries a smooth, fading trace of the whole history; the counts recall
// exact phrases verbatim. Each covers the other's blind spot.
#pragma once

#include "EchoReservoir.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <map>
#include <memory>
#include <random>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace u9n {

struct LanguageModelConfig {
    int Size = 512;
    double LeakRate = 1.0, InputScale = 0.5, SpectralRadius = 0.95, Ridge = 1e-2;
    int Order = 12;        // longest n-gram context, in characters
    bool Online = true;    // counts and mixer weights learn from text as it is read
    double MixRate = 0.005; int MixEpochs = 3;   // gradient step and validation passes for g_d
    unsigned Seed = 42;
};

class EchoLanguageModel {
public:
    using Config = LanguageModelConfig;

    explicit EchoLanguageModel(const Config& cfg = {}) : Cfg(cfg), Lambda(cfg.Order + 1, 0.5), G(cfg.Order + 1, {0.5, 0.5}) {}

    // Builds the alphabet from `alphabetText` (should cover train/val/test), then fits the
    // readout and the n-gram counts on `train`. Documents are separated by '\n'.
    void Fit(const std::string& alphabetText, const std::string& train) {
        Id.clear();
        for (unsigned char c : alphabetText) Id.emplace(c, 0);
        int v = 0; Chars.clear();
        for (auto& kv : Id) { kv.second = v++; Chars.push_back((char)kv.first); }
        ReservoirConfig rc;
        rc.Size = Cfg.Size; rc.InputDim = V(); rc.LeakRate = Cfg.LeakRate; rc.InputScale = Cfg.InputScale;
        rc.SpectralRadius = Cfg.SpectralRadius; rc.Seed = Cfg.Seed;
        Res = std::make_unique<EchoReservoir>(rc);

        const int D = Features();
        Eigen::MatrixXd G = Eigen::MatrixXd::Zero(D, D), YX = Eigen::MatrixXd::Zero(V(), D);
        const int B = 512; Eigen::MatrixXd buf(D, B); std::vector<int> tgt(B); int nb = 0;
        auto flush = [&] {
            G.noalias() += buf.leftCols(nb) * buf.leftCols(nb).transpose();
            for (int i = 0; i < nb; ++i) YX.row(tgt[i]) += buf.col(i).transpose();
            nb = 0;
        };
        Run(train, [&](const Eigen::VectorXd& z, int y) { buf.col(nb) = z; tgt[nb] = y; if (++nb == B) flush(); });
        if (nb) flush();
        G.diagonal().array() += Cfg.Ridge;
        W = G.ldlt().solve(YX.transpose()).transpose();

        Base.clear();
        for (size_t t = 0; t < train.size(); ++t) Count(Base, train, t);
    }

    // Picks β (echo temperature) on validation text, then each depth's linear weight λ_d (a
    // 1-D search per depth bucket), then fits the geometric weights g_d starting from (λ_d, 1−λ_d).
    void Calibrate(const std::string& val) {
        const auto e = EchoLogits(val);
        double best = 1e300;
        for (double beta : {4.0, 8.0, 12.0, 16.0, 24.0, 32.0}) {
            double bits = 0;
            for (size_t i = 0; i < e.size(); ++i) bits -= std::log2(Softmax(beta * e[i])[Index(val[i + 1])]);
            if (bits < best) { best = bits; Beta = beta; }
        }
        const auto s = Score(val, e);
        for (int d = 0; d <= Cfg.Order; ++d) {
            double bestBits = 1e300;
            for (int k = 0; k <= 20; ++k) {
                const double lam = k / 20.0;
                double bits = 0;
                for (const auto& p : s) if (p.Depth == d) bits -= std::log2(std::max(lam * p.Pe[p.Y] + (1 - lam) * p.Pn[p.Y], 1e-300));
                if (bits < bestBits) { bestBits = bits; Lambda[d] = lam; }
            }
            G[d] = {Lambda[d], 1 - Lambda[d]};
        }
        for (int ep = 0; ep < Cfg.MixEpochs; ++ep) for (const auto& p : s) Mix(p, G[p.Depth], Cfg.MixRate);
    }

    // Mean bits per character of the mixture on `text` (and of each component and of the linear
    // λ_d blend, if asked). With Online set, g_d keeps learning on a copy, so the model is unchanged.
    double BitsPerChar(const std::string& text, double* echoOnly = nullptr, double* ngramOnly = nullptr,
                       double* linear = nullptr) const {
        const auto s = Score(text, EchoLogits(text));
        auto g = G;
        double mix = 0, echo = 0, ngram = 0, lin = 0;
        for (const auto& p : s) {
            const double lam = Lambda[p.Depth];
            mix -= std::log2(std::max(Mix(p, g[p.Depth], Cfg.Online ? Cfg.MixRate : 0.0)[p.Y], 1e-300));
            lin -= std::log2(std::max(lam * p.Pe[p.Y] + (1 - lam) * p.Pn[p.Y], 1e-300));
            echo -= std::log2(std::max(p.Pe[p.Y], 1e-300));
            ngram -= std::log2(std::max(p.Pn[p.Y], 1e-300));
        }
        const double n = (double)std::max<size_t>(s.size(), 1);
        if (echoOnly) *echoOnly = echo / n;
        if (ngramOnly) *ngramOnly = ngram / n;
        if (linear) *linear = lin / n;
        return mix / n;
    }

    // Samples `count` characters after `prompt` from the mixture (counts are not updated).
    std::string Generate(const std::string& prompt, int count, std::mt19937& rng) const {
        std::string s = prompt;
        Res->Reset();
        for (char c : prompt) Res->Step(OneHot(c));
        const Table none;
        for (int k = 0; k < count; ++k) {
            Point pt{Softmax(Beta * (W * Feature(Res->State()))), Eigen::VectorXd(), 0, 0};
            pt.Pn = Ngram(s, s.size(), none, &pt.Depth);
            auto g = G[pt.Depth];
            const Eigen::VectorXd q = Mix(pt, g, 0.0);
            const char c = Chars[std::discrete_distribution<int>(q.data(), q.data() + q.size())(rng)];
            s += c;
            Res->Step(OneHot(c));
        }
        return s.substr(prompt.size());
    }

    int V() const { return (int)Chars.size(); }
    double Temperature() const { return Beta; }
    // λ_d per matched context depth d = 0..Order (linear weight on the echo).
    const std::vector<double>& MixtureWeights() const { return Lambda; }
    // g_d = (echo, counts) exponents of the geometric mixture, per matched depth, after calibration.
    const std::vector<std::array<double, 2>>& GeometricWeights() const { return G; }

private:
    // Sparse counts for one context: (symbol, count) pairs plus the total.
    struct Ctx { double Total = 0; std::vector<std::pair<int, double>> N; };
    using Table = std::unordered_map<std::uint64_t, Ctx>;
    struct Point { Eigen::VectorXd Pe, Pn; int Y, Depth; };   // full distributions at one position

    int Features() const { return 2 * Cfg.Size + 1; }
    int Index(char c) const { auto it = Id.find((unsigned char)c); return it == Id.end() ? 0 : it->second; }
    Eigen::VectorXd OneHot(char c) const { Eigen::VectorXd u = Eigen::VectorXd::Zero(V()); u[Index(c)] = 1.0; return u; }
    Eigen::VectorXd Feature(const Eigen::VectorXd& h) const {
        Eigen::VectorXd z(Features());
        z << h, h.array().square().matrix(), 1.0;
        return z;
    }
    static Eigen::VectorXd Softmax(const Eigen::VectorXd& l) {
        Eigen::VectorXd p = (l.array() - l.maxCoeff()).exp();
        return p / p.sum();
    }

    // FNV-1a over the order-o context ending before position t, salted with o.
    static std::uint64_t Key(const std::string& s, size_t t, int o) {
        std::uint64_t h = 1469598103934665603ull ^ (std::uint64_t)(o + 1) * 0x9E3779B97F4A7C15ull;
        for (size_t i = t - o; i < t; ++i) { h ^= (unsigned char)s[i]; h *= 1099511628211ull; }
        return h;
    }
    void Count(Table& tab, const std::string& s, size_t t) const {
        const int y = Index(s[t]);
        for (int o = 0; o <= Cfg.Order && o <= (int)t; ++o) {
            Ctx& c = tab[Key(s, t, o)];
            c.Total += 1;
            bool found = false;
            for (auto& [sym, n] : c.N) if (sym == y) { n += 1; found = true; break; }
            if (!found) c.N.emplace_back(y, 1.0);
        }
    }
    // Witten-Bell distribution at position t, over Base plus an online overlay. Interpolates from
    // order 0 upward while the context has been seen; *depth = deepest seen. A symbol counts as
    // one type however many tables hold it.
    Eigen::VectorXd Ngram(const std::string& s, size_t t, const Table& online, int* depth) const {
        Eigen::VectorXd p = Eigen::VectorXd::Constant(V(), 1.0 / V()), n(V());
        *depth = 0;
        for (int o = 0; o <= Cfg.Order && o <= (int)t; ++o) {
            const std::uint64_t k = Key(s, t, o);
            const auto b = Base.find(k), u = online.find(k);
            double total = 0;
            n.setZero();
            for (const Ctx* c : {b == Base.end() ? nullptr : &b->second, u == online.end() ? nullptr : &u->second})
                if (c) { total += c->Total; for (const auto& [sym, cnt] : c->N) n[sym] += cnt; }
            if (total == 0) break;
            const double types = (double)(n.array() > 0).count(), lam = total / (total + types);
            p = lam * n / total + (1 - lam) * p;
            *depth = o;
        }
        return p;
    }
    // Geometric mixture at one position under weights g; with rate > 0, then steps g down the
    // gradient of −log p(y).
    Eigen::VectorXd Mix(const Point& pt, std::array<double, 2>& g, double rate) const {
        const Eigen::ArrayXd le = pt.Pe.array().max(1e-12).log(), ln = pt.Pn.array().max(1e-12).log();
        Eigen::ArrayXd z = g[0] * le + g[1] * ln;
        z = (z - z.maxCoeff()).exp();
        z /= z.sum();
        if (rate > 0) {
            g[0] -= rate * ((z * le).sum() - le[pt.Y]);
            g[1] -= rate * ((z * ln).sum() - ln[pt.Y]);
        }
        return z.matrix();
    }

    // Per-position echo and n-gram distributions of the next char, with the true char and depth.
    // Online counting runs on a fresh overlay per call, so evaluation never alters Base.
    std::vector<Point> Score(const std::string& text, const std::vector<Eigen::VectorXd>& e) const {
        std::vector<Point> out;
        out.reserve(e.size());
        Table online;
        if (Cfg.Online && !text.empty()) Count(online, text, 0);
        for (size_t i = 0; i < e.size(); ++i) {
            Point p{Softmax(Beta * e[i]), Eigen::VectorXd(), Index(text[i + 1]), 0};
            p.Pn = Ngram(text, i + 1, online, &p.Depth);
            out.push_back(std::move(p));
            if (Cfg.Online) Count(online, text, i + 1);
        }
        return out;
    }

    // Feeds text one char at a time (reset at the start); sink(features after char t, id of char t+1).
    template <class Sink> void Run(const std::string& text, Sink&& sink) const {
        Res->Reset();
        for (size_t t = 0; t + 1 < text.size(); ++t) {
            Res->Step(OneHot(text[t]));
            sink(Feature(Res->State()), Index(text[t + 1]));
        }
    }
    std::vector<Eigen::VectorXd> EchoLogits(const std::string& text) const {
        std::vector<Eigen::VectorXd> out;
        Run(text, [&](const Eigen::VectorXd& z, int) { out.push_back(W * z); });
        return out;
    }

    Config Cfg;
    std::map<unsigned char, int> Id;
    std::vector<char> Chars;
    std::unique_ptr<EchoReservoir> Res;
    Eigen::MatrixXd W;
    Table Base;
    double Beta = 12.0;
    std::vector<double> Lambda;
    std::vector<std::array<double, 2>> G;
};

} // namespace u9n
