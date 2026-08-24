#pragma once

#include <QtGlobal>
#include <QString>
#include <QMetaType>

#include <memory>
#include <vector>

namespace fdkui {

// The buffer is float32 in ZYX order: X is the fastest-changing index.
// Shared ownership lets the dialog close without invalidating data held by the
// visualization layer and avoids copying large reconstruction volumes.
struct ReconstructionResult {
    quint32 sizeX{};
    quint32 sizeY{};
    quint32 sizeZ{};
    double fovXmm{};
    double fovYmm{};
    double fovZmm{};
    double centerXmm{};
    double centerYmm{};
    double centerZmm{};
    QString configIniPath;
    qint64 elapsedMs{};
    std::shared_ptr<const std::vector<float>> voxelsZyx;
};

using ReconstructionResultPtr = std::shared_ptr<const ReconstructionResult>;

} // namespace fdkui

Q_DECLARE_METATYPE(fdkui::ReconstructionResultPtr)
