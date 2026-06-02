#pragma once

#include <QDialog>

class QVBoxLayout;

class ThemedDialog : public QDialog {
    Q_OBJECT
public:
    explicit ThemedDialog(const QString& title, QWidget* parent = nullptr);

    QWidget* contentWidget() const;
    QVBoxLayout* contentLayout() const;
    void setTitleText(const QString& title);

private:
    QWidget* _contentWidget = nullptr;
    QVBoxLayout* _contentLayout = nullptr;
    class TitleBar;
    TitleBar* _titleBar = nullptr;
};
