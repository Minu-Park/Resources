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
    /**
     * @brief Runs the dialog synchronously with the same window-modal presentation as open().
     * QDialog::exec() would switch to application modality and lose the native sheet; a dialog
     * without a shown parent window keeps QDialog::exec() behavior.
     */
    int exec() override;

protected:
    void showEvent(QShowEvent* event) override;

private:
    QWidget* _contentWidget = nullptr;
    QVBoxLayout* _contentLayout = nullptr;
    class TitleBar;
    TitleBar* _titleBar = nullptr;
};
