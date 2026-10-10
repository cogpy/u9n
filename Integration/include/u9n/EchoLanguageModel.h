// EchoLanguageModel.h — character-level language model on the Deep Tree Echo reservoir.
//
// Two memories, one prediction:
//   echo     p_e(c | history) = softmax(β · W [h; h⊙h; 1])  where h is the reservoir state after
//            reading the history one-hot, character by character. W is a ridge readout whose
//            Gram matrix is accumulated in batches, so states are never stored. β is calibrated.
//   episodic p_n(c | last k chars): Witten-Bell interpolated character n-gram counts.
//   mixture  p(c) = λ p_e + (1-λ) p_n, λ chosen on validation text.
// The reservoir carries a smooth, fading trace of the whole history; the counts recall
// exact phrases verbatim. Each covers the other's blind spot.
#pragma once

#include "EchoReservoir.h"

#include <array>
#include <cmath>
#include <map>
#include <memory>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

namespace u9n {

struct LanguageModelConfig {
    int Size = 512;
    double LeakRate = 1.0, InputScale = 0.5, SpectralRadius = 0.95, Ridge = 1e-2;
    int Order = 5;   // n-gram context length
    unsigned Seed = 42;
};

class EchoLanguageModel {
public:
    using Config = LanguageModelConfig;

    explicit EchoLanguageModel(const Config& cfg = {}) : Cfg(cfg) {}

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

        Counts.clear();
        for (size_t t = 0; t < train.size(); ++t)
            for (int o = 0; o <= Cfg.Order && o <= (int)t; ++o) {
                auto& c = Counts[train.substr(t - o, o)];
                if (c.empty()) c.assign(V(), 0.0);
                c[Index(train[t])] += 1.0;
            }
    }

    // Picks β (echo temperature) and λ (mixture weight) on validation text.
    void Calibrate(const std::string& val) {
        const auto e = EchoLogits(val); const auto n = NgramProbs(val);
        double best = 1e300;
        for (double beta : {4.0, 8.0, 12.0, 16.0, 24.0, 32.0}) {
            const double b = MeanBits(e, n, val, beta, 1.0);
            if (b < best) { best = b; Beta = beta; }
        }
        best = 1e300;
        for (int i = 0; i <= 20; ++i) {
            const double b = MeanBits(e, n, val, Beta, i / 20.0);
            if (b < best) { best = b; Lambda = i / 20.0; }
        }
    }

    // Mean bits per character of the mixture on `text` (and of each component, if asked).
    double BitsPerChar(const std::string& text, double* echoOnly = nullptr, double* ngramOnly = nullptr) const {
        const auto e = EchoLogits(text); const auto n = NgramProbs(text);
        if (echoOnly) *echoOnly = MeanBits(e, n, text, Beta, 1.0);
        if (ngramOnly) *ngramOnly = MeanBits(e, n, text, Beta, 0.0);
        return MeanBits(e, n, text, Beta, Lambda);
    }

    // Samples `count` characters after `prompt` from the mixture.
    std::string Generate(const std::string& prompt, int count, std::mt19937& rng) const {
        std::string s = prompt;
        Res->Reset();
        for (char c : prompt) Res->Step(OneHot(c));
        for (int k = 0; k < count; ++k) {
            const Eigen::VectorXd pe = Softmax(Beta * (W * Feature(Res->State())));
            const std::vector<double> pn = Ngram(s, s.size());
            std::vector<double> p(V());
            for (int i = 0; i < V(); ++i) p[i] = Lambda * pe[i] + (1 - Lambda) * pn[i];
            const char c = Chars[std::discrete_distribution<int>(p.begin(), p.end())(rng)];
            s += c;
            Res->Step(OneHot(c));
        }
        return s.substr(prompt.size());
    }

    int V() const { return (int)Chars.size(); }
    double Temperature() const { return Beta; }
    double MixtureWeight() const { return Lambda; }

private:
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
    // Witten-Bell: interpolate from order 0 upward while the context has been seen.
    std::vector<double> Ngram(const std::string& s, size_t t) const {
        std::vector<double> p(V(), 1.0 / V());
        for (int o = 0; o <= Cfg.Order && o <= (int)t; ++o) {
            auto it = Counts.find(s.substr(t - o, o));
            if (it == Counts.end()) break;
            double tot = 0, types = 0;
            for (double q : it->second) { tot += q; types += q > 0; }
            const double lam = tot / (tot + types);
            for (int i = 0; i < V(); ++i) p[i] = lam * it->second[i] / tot + (1 - lam) * p[i];
        }
        return p;
    }
    std::vector<std::vector<double>> NgramProbs(const std::string& text) const {
        std::vector<std::vector<double>> out;
        for (size_t t = 1; t < text.size(); ++t) out.push_back(Ngram(text, t));
        return out;
    }
    double MeanBits(const std::vector<Eigen::VectorXd>& e, const std::vector<std::vector<double>>& n,
                    const std::string& text, double beta, double lambda) const {
        double bits = 0;
        for (size_t i = 0; i < e.size(); ++i) {
            const int y = Index(text[i + 1]);
            const double q = lambda * Softmax(beta * e[i])[y] + (1 - lambda) * n[i][y];
            bits -= std::log2(std::max(q, 1e-300));
        }
        return bits / std::max<size_t>(e.size(), 1);
    }

    Config Cfg;
    std::map<unsigned char, int> Id;
    std::vector<char> Chars;
    std::unique_ptr<EchoReservoir> Res;
    Eigen::MatrixXd W;
    std::unordered_map<std::string, std::vector<double>> Counts;
    double Beta = 12.0, Lambda = 0.5;
};

} // namespace u9n
