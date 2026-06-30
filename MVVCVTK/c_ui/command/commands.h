#pragma once
#include <QHash>
#include <QString>
#include <functional>

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