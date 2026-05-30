#include "mini_torch/torch.h"
#include <cmath>
#include <iostream>
#include <vector>
#include <string>

#include "rtl_like_attention/flash_array.hpp"

std::vector<float> makeValues(int n, float scale) {
    std::vector<float> values;
    values.reserve(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) {
        values.push_back(scale * static_cast<float>((i % 7) - 3));
    }
    return values;
}

torch::Tensor goldenAttention(const torch::Tensor& Q, const torch::Tensor& K, const torch::Tensor& V) {
    const auto score = torch::matmul(K, Q) / std::sqrt(static_cast<float>(DIM));
    const auto prob = torch::softmax(score, 0);
    return torch::matmul(prob.unsqueeze(0), V).squeeze(0);
}

int main(int argc, char** argv) {
    auto Q = torch::Tensor(makeValues(DIM, 0.1f), {DIM});
    auto K = torch::Tensor(makeValues(TOKEN * DIM, 0.05f), {TOKEN, DIM});
    auto V = torch::Tensor(makeValues(TOKEN * DIM, 0.2f), {TOKEN, DIM});

    if (argc >= 2 && std::string(argv[1]) == "--trace") {
        FlashArray dut;
        dut.writeTrace(Q, K, V, argc >= 3 ? argv[2] : "trace.json");
        return 0;
    }

    FlashArray dut;
    const auto out = dut.run(Q, K, V).data();
    const auto golden = goldenAttention(Q, K, V).data();

    float maxErr = 0.0f;
    std::cout << "out:";
    for (float x : out) {
        std::cout << " " << x;
    }
    std::cout << "\ngolden:";
    for (float x : golden) {
        std::cout << " " << x;
    }
    std::cout << "\nerr:";
    for (std::size_t i = 0; i < golden.size(); ++i) {
        const float err = std::fabs(out[i] - golden[i]);
        maxErr = std::max(maxErr, err);
        std::cout << " " << err;
    }
    std::cout << "\nmaxerr " << maxErr << "\n";

    return 0;
}
