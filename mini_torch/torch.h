#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <numeric>
#include <random>
#include <stdexcept>
#include <utility>
#include <vector>

namespace torch {

enum ScalarType {
    kFloat32
};

class Tensor {
public:
    Tensor() = default;

    Tensor(std::vector<float> data, std::vector<int64_t> shape)
        : data_(std::move(data)), shape_(std::move(shape)) {}

    Tensor contiguous() const { return *this; }
    Tensor to(ScalarType) const { return *this; }
    Tensor clone() const { return *this; }

    int64_t numel() const {
        return static_cast<int64_t>(data_.size());
    }

    template <typename T>
    T* data_ptr() {
        return reinterpret_cast<T*>(data_.data());
    }

    template <typename T>
    const T* data_ptr() const {
        return reinterpret_cast<const T*>(data_.data());
    }

    Tensor reshape(std::initializer_list<int64_t> shape) const {
        Tensor out = *this;
        out.shape_ = std::vector<int64_t>(shape);
        if (product(out.shape_) != numel()) {
            throw std::runtime_error("reshape size mismatch");
        }
        return out;
    }

    Tensor unsqueeze(int64_t dim) const {
        Tensor out = *this;
        if (dim < 0) {
            dim += static_cast<int64_t>(out.shape_.size()) + 1;
        }
        if (dim < 0 || dim > static_cast<int64_t>(out.shape_.size())) {
            throw std::runtime_error("unsqueeze dim out of range");
        }
        out.shape_.insert(out.shape_.begin() + dim, 1);
        return out;
    }

    Tensor squeeze(int64_t dim) const {
        Tensor out = *this;
        if (dim < 0) {
            dim += static_cast<int64_t>(out.shape_.size());
        }
        if (dim < 0 || dim >= static_cast<int64_t>(out.shape_.size())) {
            throw std::runtime_error("squeeze dim out of range");
        }
        if (out.shape_[dim] == 1) {
            out.shape_.erase(out.shape_.begin() + dim);
        }
        return out;
    }

    Tensor max() const {
        if (data_.empty()) {
            throw std::runtime_error("max from empty tensor");
        }
        return Tensor({*std::max_element(data_.begin(), data_.end())}, {});
    }

    template <typename T>
    T item() const {
        if (data_.empty()) {
            throw std::runtime_error("item from empty tensor");
        }
        return static_cast<T>(data_[0]);
    }

    const std::vector<int64_t>& sizes() const { return shape_; }
    const std::vector<float>& data() const { return data_; }

private:
    static int64_t product(const std::vector<int64_t>& shape) {
        return std::accumulate(shape.begin(), shape.end(), int64_t{1}, std::multiplies<int64_t>());
    }

    std::vector<float> data_;
    std::vector<int64_t> shape_;
};

inline std::mt19937& rng() {
    static std::mt19937 gen(0);
    return gen;
}

inline void manual_seed(uint32_t seed) {
    rng().seed(seed);
}

inline Tensor randn(std::initializer_list<int64_t> shape, ScalarType) {
    std::vector<int64_t> dims(shape);
    int64_t n = std::accumulate(dims.begin(), dims.end(), int64_t{1}, std::multiplies<int64_t>());
    std::normal_distribution<float> dist(0.0f, 1.0f);
    std::vector<float> data(static_cast<size_t>(n));
    for (float& v : data) {
        v = dist(rng());
    }
    return Tensor(std::move(data), std::move(dims));
}

inline Tensor from_blob(float* ptr, std::initializer_list<int64_t> shape, ScalarType) {
    std::vector<int64_t> dims(shape);
    int64_t n = std::accumulate(dims.begin(), dims.end(), int64_t{1}, std::multiplies<int64_t>());
    return Tensor(std::vector<float>(ptr, ptr + n), std::move(dims));
}

inline Tensor matmul(const Tensor& a, const Tensor& b) {
    const auto& as = a.sizes();
    const auto& bs = b.sizes();
    const auto& ad = a.data();
    const auto& bd = b.data();

    if (as.size() == 2 && bs.size() == 1 && as[1] == bs[0]) {
        std::vector<float> out(static_cast<size_t>(as[0]), 0.0f);
        for (int64_t i = 0; i < as[0]; ++i) {
            for (int64_t k = 0; k < as[1]; ++k) {
                out[static_cast<size_t>(i)] += ad[static_cast<size_t>(i * as[1] + k)] * bd[static_cast<size_t>(k)];
            }
        }
        return Tensor(std::move(out), {as[0]});
    }

    if (as.size() == 2 && bs.size() == 2 && as[1] == bs[0]) {
        std::vector<float> out(static_cast<size_t>(as[0] * bs[1]), 0.0f);
        for (int64_t i = 0; i < as[0]; ++i) {
            for (int64_t j = 0; j < bs[1]; ++j) {
                for (int64_t k = 0; k < as[1]; ++k) {
                    out[static_cast<size_t>(i * bs[1] + j)] +=
                        ad[static_cast<size_t>(i * as[1] + k)] * bd[static_cast<size_t>(k * bs[1] + j)];
                }
            }
        }
        return Tensor(std::move(out), {as[0], bs[1]});
    }

    throw std::runtime_error("unsupported matmul shape");
}

inline Tensor operator/(const Tensor& t, float value) {
    std::vector<float> out = t.data();
    for (float& v : out) {
        v /= value;
    }
    return Tensor(std::move(out), t.sizes());
}

inline Tensor operator-(const Tensor& a, const Tensor& b) {
    if (a.data().size() != b.data().size()) {
        throw std::runtime_error("tensor subtraction size mismatch");
    }
    std::vector<float> out = a.data();
    for (size_t i = 0; i < out.size(); ++i) {
        out[i] -= b.data()[i];
    }
    return Tensor(std::move(out), a.sizes());
}

inline Tensor abs(const Tensor& t) {
    std::vector<float> out = t.data();
    for (float& v : out) {
        v = std::fabs(v);
    }
    return Tensor(std::move(out), t.sizes());
}

inline Tensor softmax(const Tensor& t, int64_t dim) {
    if (dim != 0 || t.sizes().size() != 1) {
        throw std::runtime_error("minimal softmax only supports dim 0 for 1D tensors");
    }
    const auto& input = t.data();
    float m = *std::max_element(input.begin(), input.end());
    std::vector<float> out(input.size());
    float sum = 0.0f;
    for (size_t i = 0; i < input.size(); ++i) {
        out[i] = std::exp(input[i] - m);
        sum += out[i];
    }
    for (float& v : out) {
        v /= sum;
    }
    return Tensor(std::move(out), t.sizes());
}

} // namespace torch
