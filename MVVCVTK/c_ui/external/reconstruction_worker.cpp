#include "reconstruction_worker.h"

#include "alg.hpp"

#include <QByteArray>
#include <QElapsedTimer>

#include <exception>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace fdkui {

ReconstructionWorker::ReconstructionWorker(ReconstructionRequest request)
    : request_(std::move(request)) {}

void ReconstructionWorker::requestCancellation() noexcept {
    cancellationRequested_.store(true, std::memory_order_relaxed);
}

void ReconstructionWorker::run() {
    QElapsedTimer elapsed;
    elapsed.start();

    try {
        const QByteArray iniUtf8 = request_.configIniPath.toUtf8();

        ct::algFDK::ReconConfig config;
        config.volume_size_x = request_.volumeSizeX;
        config.volume_size_y = request_.volumeSizeY;
        config.volume_size_z = request_.volumeSizeZ;
        config.fov_x_mm = request_.fovXmm;
        config.fov_y_mm = request_.fovYmm;
        config.fov_z_mm = request_.fovZmm;
        config.volume_center_x_mm = request_.centerXmm;
        config.volume_center_y_mm = request_.centerYmm;
        config.volume_center_z_mm = request_.centerZmm;
        config.detector_binning = request_.detectorBinning;
        config.maximum_batch_views = request_.maximumBatchViews;
        config.maximum_batch_latency_ms = request_.maximumBatchLatencyMs;
        config.gpu_index = request_.gpuIndex;

        ct::algFDK::incrementalFDK reconstruction(
            std::string(iniUtf8.constData(), static_cast<std::size_t>(iniUtf8.size())),
            config);
        reconstruction.start();

        bool cancellationSent = false;
        while (true) {
            if (cancellationRequested_.load(std::memory_order_relaxed) && !cancellationSent) {
                reconstruction.cancel();
                cancellationSent = true;
            }

            const bool stopped = reconstruction.wait(100);
            const ct::algFDK::ReconstructionProgress progress = reconstruction.progress();
            emit progressChanged(
                static_cast<int>(progress.state),
                progress.expected_frames,
                progress.received_frames,
                progress.processed_frames);
            if (stopped) {
                break;
            }
        }

        const ct::algFDK::ReconstructionProgress progress = reconstruction.progress();
        if (progress.state == ct::algFDK::ReconstructionState::completed) {
            auto volume = std::make_shared<std::vector<float>>(reconstruction.download());
            auto result = std::make_shared<ReconstructionResult>();
            result->sizeX = request_.volumeSizeX;
            result->sizeY = request_.volumeSizeY;
            result->sizeZ = request_.volumeSizeZ;
            result->fovXmm = request_.fovXmm;
            result->fovYmm = request_.fovYmm;
            result->fovZmm = request_.fovZmm;
            result->centerXmm = request_.centerXmm;
            result->centerYmm = request_.centerYmm;
            result->centerZmm = request_.centerZmm;
            result->configIniPath = request_.configIniPath;
            result->elapsedMs = elapsed.elapsed();
            result->voxelsZyx = std::move(volume);
            emit succeeded(std::move(result));
        } else if (progress.state == ct::algFDK::ReconstructionState::cancelled) {
            emit cancelled();
        } else {
            QString message = QString::fromUtf8(reconstruction.last_error().c_str());
            if (message.trimmed().isEmpty()) {
                message = tr("FDK reconstruction failed.");
            }
            emit failed(message);
        }
    } catch (const std::exception& error) {
        emit failed(QString::fromUtf8(error.what()));
    } catch (...) {
        emit failed(tr("Unknown error while running FDK reconstruction."));
    }

    emit workFinished();
}

} // namespace fdkui
