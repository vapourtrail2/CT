#pragma once

#include "reconstruction_result.h"

#include <QObject>
#include <QString>

#include <atomic>
#include <cstdint>

namespace fdkui {

struct ReconstructionRequest {
    QString configIniPath;
    std::uint32_t volumeSizeX{512};
    std::uint32_t volumeSizeY{512};
    std::uint32_t volumeSizeZ{512};
    double fovXmm{50.0};
    double fovYmm{50.0};
    double fovZmm{50.0};
    double centerXmm{0.0};
    double centerYmm{0.0};
    double centerZmm{0.0};
    std::uint32_t detectorBinning{1};
    std::uint32_t maximumBatchViews{8};
    std::uint32_t maximumBatchLatencyMs{250};
    int gpuIndex{0};
};

class ReconstructionWorker final : public QObject {
    Q_OBJECT

public:
    explicit ReconstructionWorker(ReconstructionRequest request);

    // Safe to call directly from the GUI thread. The worker checks this flag
    // between finite SDK waits, so cancellation does not depend on a queued slot.
    void requestCancellation() noexcept;

public slots:
    void run();

signals:
    void progressChanged(int state, quint32 expected, quint32 received, quint32 processed);
    void succeeded(fdkui::ReconstructionResultPtr result);
    void failed(const QString& message);
    void cancelled();
    void workFinished();

private:
    ReconstructionRequest request_;
    std::atomic_bool cancellationRequested_{false};
};

} // namespace fdkui
