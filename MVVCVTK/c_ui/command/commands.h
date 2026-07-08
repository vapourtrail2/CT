#pragma once
#include <QHash>
#include <QString>
#include <functional>

class QWidget;

class Commands {
public:
    void add(const QString& name, std::function<void()> handler) {
        table_.insert(name, std::move(handler));
    }

    // 按名字执行；没注册就安全返回 false，不会崩
    bool run(const QString& name) const {
        auto it = table_.find(name);
        if (it == table_.end() || !*it) {
            return false;
        }
        (*it)();
        return true;
    }

    bool contains(const QString& name) const {
        return table_.contains(name);
    }

private:
    QHash<QString, std::function<void()>> table_;
};

namespace TabIndex
{
    constexpr int File = 0;
    constexpr int Start = 1;
    constexpr int Edit = 2;
    constexpr int Volume = 3;
    constexpr int Select = 4;
    constexpr int Align = 5;
    constexpr int Geometry = 6;
    constexpr int Measure = 7;
    constexpr int Cad = 8;
    constexpr int Analysis = 9;
    constexpr int Report = 10;
    constexpr int Animation = 11;
    constexpr int Window = 12;
    constexpr int Count = 13;
}

enum class ContentTarget {
    Document,
    Workspace,
    Empty
};

struct UiState {
    bool showRibbon = false;
    int ribbonHeight = 0;
    int tabIndex = 0;
    ContentTarget contentTarget = ContentTarget::Document;
    QWidget* ribbonPage = nullptr;
};