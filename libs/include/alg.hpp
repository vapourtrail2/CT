#pragma once

#include "ct_incremental_fdk.h"

#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace ct::algFDK {

enum class ReconstructionState : std::uint32_t {
    idle = CT_FDK_STATE_IDLE,
    running = CT_FDK_STATE_RUNNING,
    completed = CT_FDK_STATE_COMPLETED,
    cancelled = CT_FDK_STATE_CANCELLED,
    failed = CT_FDK_STATE_FAILED
};

struct ReconstructionProgress {
    ReconstructionState state{ReconstructionState::idle};
    std::uint32_t expected_frames{};
    std::uint32_t received_frames{};
    std::uint32_t processed_frames{};
};

struct ReconConfig {
    std::uint32_t volume_size_x{};
    std::uint32_t volume_size_y{};
    std::uint32_t volume_size_z{};
    double fov_x_mm{};
    double fov_y_mm{};
    double fov_z_mm{};
    double volume_center_x_mm{};
    double volume_center_y_mm{};
    double volume_center_z_mm{};
    std::uint32_t detector_binning{1};
    std::uint32_t maximum_batch_views{8};
    std::uint32_t maximum_batch_latency_ms{250};
    int gpu_index{};
};

namespace detail {

inline std::string last_error(const ct_fdk_handle* handle) {
    std::size_t required{};
    const auto query = ct_fdk_get_last_error(handle, nullptr, 0, &required);
    if (query != CT_FDK_BUFFER_TOO_SMALL && query != CT_FDK_OK) {
        return "ct_incrementalFDK operation failed";
    }
    std::vector<char> buffer(required == 0 ? 1 : required);
    if (ct_fdk_get_last_error(handle, buffer.data(), buffer.size(), &required)
        != CT_FDK_OK) {
        return "ct_incrementalFDK operation failed";
    }
    return buffer.data();
}

[[noreturn]] inline void throw_result(
    ct_fdk_result result,
    const ct_fdk_handle* handle) {
    auto message = last_error(handle);
    if (message.empty()) {
        message = "ct_incrementalFDK error "
            + std::to_string(static_cast<int>(result));
    }
    if (result == CT_FDK_INVALID_ARGUMENT) throw std::invalid_argument(message);
    if (result == CT_FDK_INVALID_STATE) throw std::logic_error(message);
    throw std::runtime_error(message);
}

} // namespace detail

class incrementalFDK {
public:
    incrementalFDK(std::string ini_path, ReconConfig config) {
        ct_fdk_recon_config native{};
        native.struct_size = sizeof(ct_fdk_recon_config);
        native.volume_size_x = config.volume_size_x;
        native.volume_size_y = config.volume_size_y;
        native.volume_size_z = config.volume_size_z;
        native.fov_x_mm = config.fov_x_mm;
        native.fov_y_mm = config.fov_y_mm;
        native.fov_z_mm = config.fov_z_mm;
        native.volume_center_x_mm = config.volume_center_x_mm;
        native.volume_center_y_mm = config.volume_center_y_mm;
        native.volume_center_z_mm = config.volume_center_z_mm;
        native.detector_binning = config.detector_binning;
        native.maximum_batch_views = config.maximum_batch_views;
        native.maximum_batch_latency_ms = config.maximum_batch_latency_ms;
        native.gpu_index = config.gpu_index;
        const auto result = ct_fdk_create(ini_path.c_str(), &native, &handle_);
        if (result != CT_FDK_OK) detail::throw_result(result, nullptr);
    }

    ~incrementalFDK() { ct_fdk_destroy(handle_); }

    incrementalFDK(const incrementalFDK&) = delete;
    incrementalFDK& operator=(const incrementalFDK&) = delete;

    incrementalFDK(incrementalFDK&& other) noexcept
        : handle_(std::exchange(other.handle_, nullptr)) {}

    incrementalFDK& operator=(incrementalFDK&& other) noexcept {
        if (this != &other) {
            ct_fdk_destroy(handle_);
            handle_ = std::exchange(other.handle_, nullptr);
        }
        return *this;
    }

    void start() {
        const auto result = ct_fdk_start(handle_);
        if (result != CT_FDK_OK) detail::throw_result(result, handle_);
    }

    void cancel() noexcept { static_cast<void>(ct_fdk_cancel(handle_)); }

    [[nodiscard]] bool wait(
        std::uint32_t timeout_ms = std::numeric_limits<std::uint32_t>::max()) const {
        const auto result = ct_fdk_wait(handle_, timeout_ms);
        if (result == CT_FDK_TIMEOUT) return false;
        if (result != CT_FDK_OK) detail::throw_result(result, handle_);
        return true;
    }

    [[nodiscard]] ReconstructionProgress progress() const {
        ct_fdk_progress native{};
        native.struct_size = sizeof(ct_fdk_progress);
        const auto result = ct_fdk_get_progress(handle_, &native);
        if (result != CT_FDK_OK) detail::throw_result(result, handle_);
        ReconstructionProgress progress;
        progress.state = static_cast<ReconstructionState>(native.state);
        progress.expected_frames = native.expected_frames;
        progress.received_frames = native.received_frames;
        progress.processed_frames = native.processed_frames;
        return progress;
    }

    [[nodiscard]] std::string last_error() const {
        return detail::last_error(handle_);
    }

    [[nodiscard]] std::vector<float> download() const {
        ct_fdk_volume_info info{};
        info.struct_size = sizeof(ct_fdk_volume_info);
        auto result = ct_fdk_get_volume_info(handle_, &info);
        if (result != CT_FDK_OK) detail::throw_result(result, handle_);
        if (info.voxel_count > std::numeric_limits<std::size_t>::max()) {
            throw std::length_error("reconstruction volume is too large for this process");
        }
        std::vector<float> volume(static_cast<std::size_t>(info.voxel_count));
        std::uint64_t written{};
        result = ct_fdk_copy_volume(
            handle_, volume.data(), info.voxel_count, &written);
        if (result != CT_FDK_OK) detail::throw_result(result, handle_);
        if (written != info.voxel_count) {
            throw std::runtime_error("reconstruction volume size changed during download");
        }
        return volume;
    }

private:
    ct_fdk_handle* handle_{};
};

} // namespace ct::algFDK
