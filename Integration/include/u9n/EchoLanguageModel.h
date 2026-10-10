// EchoLanguageModel.h — character-level language model on the Deep Tree Echo reservoir.
//
// Two memories, one prediction:
//   echo     p_e(c | history) = softmax(β · W [h; h⊙h; 1])  where h is the reservoir state after
//            reading the history one-hot, character by character. W is a ridge readout whose
//            Gram matrix is accumulated in batches, so states are never stored. β is calibrated.
//   episodic p_n(c | last k chars): Witten-Bell interpolated character n-gram counts up to
//            order k, held sparsely under 64-bit context hashes. With Online set, the counts
//            keep learning while reading: each character is predicted first, then counted.
//   mixture  p(c) = λ_d p_e + (1-λ_d) p_n, where d is the longest context the counts have
//            seen. Deep matches are verbatim recall and earn trust; shallow ones defer to the
//            echo. Each λ_d is fitted on validation text.
// The reservoir carries a smooth, fading trace of the whole history; the counts recall
// exact phrases verbatim. Each covers the other's blind spot.
#pragma once

#include "EchoReservoir.h"

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
    bool Online = true;    // counts learn from text as it is read (predict first, then count)
    unsigned Seed = 42;
};

class EchoLanguageModel {
public:
    using Config = LanguageModelConfig;

    explicit EchoLanguageModel(const Config& cfg = {}) : Cfg(cfg), Lambda(cfg.Order + 1, 0.5) {}

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

    // Picks β (echo temperature) on validation text, then each depth's mixture weight λ_d.
    // The loss separates over depth buckets, so each λ_d is a 1-D search.
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
                for (const auto& p : s) if (p.Depth == d) bits -= std::log2(std::max(lam * p.Echo + (1 - lam) * p.Ngram, 1e-300));
                if (bits < bestBits) { bestBits = bits; Lambda[d] = lam; }
            }
        }
    }

    // Mean bits per character of the mixture on `text` (and of each component, if asked).
    double BitsPerChar(const std::string& text, double* echoOnly = nullptr, double* ngramOnly = nullptr) const {
        const auto s = Score(text, EchoLogits(text));
        double mix = 0, echo = 0, ngram = 0;
        for (const auto& p : s) {
            const double lam = Lambda[p.Depth];
            mix -= std::log2(std::max(lam * p.Echo + (1 - lam) * p.Ngram, 1e-300));
            echo -= std::log2(std::max(p.Echo, 1e-300));
            ngram -= std::log2(std::max(p.Ngram, 1e-300));
        }
        const double n = (double)std::max<size_t>(s.size(), 1);
        if (echoOnly) *echoOnly = echo / n;
        if (ngramOnly) *ngramOnly = ngram / n;
        return mix / n;
    }

    // Samples `count` characters after `prompt` from the mixture (counts are not updated).
    std::string Generate(const std::string& prompt, int count, std::mt19937& rng) const {
        std::string s = prompt;
        Res->Reset();
        for (char c : prompt) Res->Step(OneHot(c));
        const Table none;
        for (int k = 0; k < count; ++k) {
            const Eigen::VectorXd pe = Softmax(Beta * (W * Feature(Res->State())));
            std::vector<double> p(V());
            for (int y = 0; y < V(); ++y) {
                int depth = 0;
                const double pn = Ngram(s, s.size(), y, none, &depth);
                p[y] = Lambda[depth] * pe[y] + (1 - Lambda[depth]) * pn;
            }
            const char c = Chars[std::discrete_distribution<int>(p.begin(), p.end())(rng)];
            s += c;
            Res->Step(OneHot(c));
        }
        return s.substr(prompt.size());
    }

    int V() const { return (int)Chars.size(); }
    double Temperature() const { return Beta; }
    // λ_d per matched context depth d = 0..Order (weight on the echo).
    const std::vector<double>& MixtureWeights() const { return Lambda; }

private:
    // Sparse counts for one context: (symbol, count) pairs plus the total.
    struct Ctx { double Total = 0; std::vector<std::pair<int, double>> N; };
    using Table = std::unordered_map<std::uint64_t, Ctx>;
    struct Point { double Echo, Ngram; int Depth; };

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
    // Witten-Bell probability of symbol y at position t, over Base plus an online overlay.
    // Interpolates from order 0 upward while the context has been seen; *depth = deepest seen.
    double Ngram(const std::string& s, size_t t, int y, const Table& online, int* depth) const {
        double p = 1.0 / V();
        *depth = 0;
        for (int o = 0; o <= Cfg.Order && o <= (int)t; ++o) {
            const std::uint64_t k = Key(s, t, o);
            const auto b = Base.find(k), u = online.find(k);
            const Ctx* cs[2] = {b == Base.end() ? nullptr : &b->second, u == online.end() ? nullptr : &u->second};
            double total = 0, ny = 0, types = 0;
            for (const Ctx* c : cs) if (c) total += c->Total;
            if (total == 0) break;
            for (int which = 0; which < 2; ++which) {
                if (!cs[which]) continue;
                for (const auto& [sym, n] : cs[which]->N) {
                    if (sym == y) ny += n;
                    // a symbol counts as a new type unless the other table already has it
                    bool dup = false;
                    if (which == 1 && cs[0]) for (const auto& [s0, n0] : cs[0]->N) if (s0 == sym) { dup = true; break; }
                    types += !dup;
                }
            }
            const double lam = total / (total + types);
            p = lam * ny / total + (1 - lam) * p;
            *depth = o;
        }
        return p;
    }

    // Per-position echo and n-gram probabilities of the true next char, with matched depth.
    // Online counting runs on a fresh overlay per call, so evaluation never alters Base.
    std::vector<Point> Score(const std::string& text, const std::vector<Eigen::VectorXd>& e) const {
        std::vector<Point> out;
        out.reserve(e.size());
        Table online;
        if (Cfg.Online && !text.empty()) Count(online, text, 0);
        for (size_t i = 0; i < e.size(); ++i) {
            const int y = Index(text[i + 1]);
            int depth = 0;
            const double pn = Ngram(text, i + 1, y, online, &depth);
            out.push_back({Softmax(Beta * e[i])[y], pn, depth});
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
};

} // namespace u9n
