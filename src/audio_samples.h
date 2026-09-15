#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

namespace deckstatus::audio {
inline constexpr std::size_t sample_count = 1024;
struct Format { unsigned bits{}, channels{}, block_align{}; bool floating{}; };
inline bool supported(Format f) {
    return f.channels > 0 && f.channels <= 32 && f.block_align >= f.channels * (f.bits / 8) &&
        (f.floating ? f.bits == 32 : (f.bits == 8 || f.bits == 16 || f.bits == 24 || f.bits == 32));
}
inline float decode(const std::byte* p, Format f) {
    if (f.floating) {
        float value; std::memcpy(&value, p, sizeof value);
        return std::isfinite(value) ? std::clamp(value, -1.f, 1.f) : 0.f;
    }
    if (f.bits == 8) return (std::to_integer<int>(*p) - 128) / 128.f;
    std::uint32_t value = 0;
    for (unsigned i = 0; i < f.bits / 8; ++i) value |= std::to_integer<std::uint32_t>(p[i]) << (8 * i);
    const std::int64_t signed_value = (value & (1u << (f.bits - 1)))
        ? static_cast<std::int64_t>(value) - (1LL << f.bits) : value;
    return static_cast<float>(signed_value / static_cast<double>(1LL << (f.bits - 1)));
}
class Samples {
    std::array<float, sample_count> left_{}, right_{};
    std::size_t next_{};
public:
    void clear() { left_.fill(0); right_.fill(0); next_ = 0; }
    bool append(std::span<const std::byte> bytes, std::size_t frames, Format f, bool silent = false) {
        if (!supported(f) || (!silent && frames > bytes.size() / f.block_align)) return false;
        // Only the final window is needed, even when WASAPI supplies a large packet.
        for (std::size_t i = frames > sample_count ? frames - sample_count : 0; i < frames; ++i) {
            const auto* p = silent ? nullptr : bytes.data() + i * f.block_align;
            left_[next_] = silent ? 0 : decode(p, f);
            right_[next_] = silent ? 0 : decode(p + (f.channels > 1 ? f.bits / 8 : 0), f);
            next_ = (next_ + 1) % sample_count;
        }
        return true;
    }
    auto snapshot() const {
        std::array<std::array<float, sample_count>, 2> result{};
        for (std::size_t i = 0; i < sample_count; ++i) {
            result[0][i] = left_[(next_ + i) % sample_count];
            result[1][i] = right_[(next_ + i) % sample_count];
        }
        return result;
    }
};
}
