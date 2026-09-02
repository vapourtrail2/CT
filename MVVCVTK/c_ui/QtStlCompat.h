#pragma once

// Qt 5.14 的 MSVC 适配仍引用已从 VS 2026 STL 删除的 stdext 数组迭代器。
// 先加载 Qt 编译器探测，再把这两个兼容宏收敛为标准原始迭代器。
#include <QtCore/qcompilerdetection.h>

#if defined(_MSC_VER)
#undef QT_MAKE_UNCHECKED_ARRAY_ITERATOR
#undef QT_MAKE_CHECKED_ARRAY_ITERATOR
#define QT_MAKE_UNCHECKED_ARRAY_ITERATOR(value) (value)
#define QT_MAKE_CHECKED_ARRAY_ITERATOR(value, size) (value)
#endif
