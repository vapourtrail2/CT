#pragma once

class QWidget;

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