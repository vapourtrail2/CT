#include "measure/MeasurementToolPanel.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QShortcut>
#include <QVBoxLayout>

namespace measure {
MeasurementToolPanel::MeasurementToolPanel(QWidget* parent) : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    m_volume = new QLabel(this);
    m_status = new QLabel(QStringLiteral("在主界面的任意二维视图中测量。"), this);
    m_status->setWordWrap(true);
    layout->addWidget(m_volume);
    auto* manual = new QHBoxLayout;
    const std::pair<const char*, MeasureTool> tools[]{{"线段", MeasureTool::Line}, {"三点圆", MeasureTool::Circle3Point}, {"三点圆弧", MeasureTool::Arc3Point}};
    for (const auto& entry : tools) {
        auto* button = new QPushButton(QString::fromUtf8(entry.first), this);
        manual->addWidget(button);
        connect(button, &QPushButton::clicked, this, [this, tool = entry.second] { if (beginTool) beginTool(tool); });
    }
    layout->addLayout(manual);
    auto* captures = new QHBoxLayout;
    const std::pair<const char*, EdgeCaptureShape> shapes[]{{"抓线", EdgeCaptureShape::Line}, {"抓圆", EdgeCaptureShape::Circle}, {"抓弧", EdgeCaptureShape::Arc}};
    for (const auto& entry : shapes) {
        auto* button = new QPushButton(QString::fromUtf8(entry.first), this);
        captures->addWidget(button);
        connect(button, &QPushButton::clicked, this, [this, shape = entry.second] { if (beginCapture) beginCapture(shape); });
    }
    layout->addLayout(captures);
    auto* actions = new QHBoxLayout;
    auto* confirm = new QPushButton(QStringLiteral("确认抓边"), this);
    auto* cancelButton = new QPushButton(QStringLiteral("取消 / 导航"), this);
    m_undo = new QPushButton(QStringLiteral("撤销"), this);
    m_redo = new QPushButton(QStringLiteral("重做"), this);
    for (auto* button : {confirm, cancelButton, m_undo, m_redo}) actions->addWidget(button);
    layout->addLayout(actions);
    layout->addWidget(m_status);
    connect(confirm, &QPushButton::clicked, this, [this] { if (confirmCapture) confirmCapture(); });
    connect(cancelButton, &QPushButton::clicked, this, [this] { if (cancel) cancel(); });
    connect(m_undo, &QPushButton::clicked, this, [this] { if (m_session) m_session->Undo(); if (requestRender) requestRender(); });
    connect(m_redo, &QPushButton::clicked, this, [this] { if (m_session) m_session->Redo(); if (requestRender) requestRender(); });
    Refresh();
}

MeasurementToolPanel::~MeasurementToolPanel()
{
    if (m_session) m_session->Unsubscribe(m_subscription);
}

void MeasurementToolPanel::Bind(std::shared_ptr<MeasurementSession> session)
{
    if (m_session) m_session->Unsubscribe(m_subscription);
    m_session = std::move(session);
    m_subscription = m_session ? m_session->Subscribe([this](MeasurementSession::Change) { Refresh(); }) : 0;
    setEnabled(bool(m_session));
    Refresh();
}

void MeasurementToolPanel::SetVolumeName(const QString& name) { m_volume->setText(name); }

void MeasurementToolPanel::Refresh()
{
    m_undo->setEnabled(m_session && m_session->CanUndo());
    m_redo->setEnabled(m_session && m_session->CanRedo());
    if (m_session) m_status->setText(QString::fromUtf8(m_session->StatusMessage().c_str()));
}
}
