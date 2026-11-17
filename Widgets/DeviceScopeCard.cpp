#include "Widgets/DeviceScopeCard.h"

#include <QSignalBlocker>
#include <QStackedWidget>
#include <QTabBar>
#include <QVBoxLayout>

namespace {
constexpr char separatorName[] = "DeviceScopeCardSeparator";
}

DeviceScopeCard::DeviceScopeCard(QWidget* parent)
    : QFrame(parent)
{
    setProperty("tabRole", QStringLiteral("DeviceScopeCard"));
    setFrameShape(QFrame::StyledPanel);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    _tabs = new QTabBar(this);
    _tabs->setObjectName(QStringLiteral("DeviceScopeCardTabs"));
    _tabs->setExpanding(false);
    _tabs->setDrawBase(false);
    _tabs->setUsesScrollButtons(true);
    layout->addWidget(_tabs);

    auto* separator = new QFrame(this);
    separator->setObjectName(QString::fromLatin1(separatorName));
    separator->setAttribute(Qt::WA_TransparentForMouseEvents);
    layout->addWidget(separator);

    _pages = new QStackedWidget(this);
    _pages->setObjectName(QStringLiteral("DeviceScopeCardPages"));
    layout->addWidget(_pages, 1);

    connect(_tabs, &QTabBar::currentChanged, _pages, &QStackedWidget::setCurrentIndex);
    connect(_tabs, &QTabBar::currentChanged, this, &DeviceScopeCard::currentChanged);
}

int DeviceScopeCard::count() const
{
    return _tabs->count();
}

QWidget* DeviceScopeCard::widget(int index) const
{
    return _pages->widget(index);
}

int DeviceScopeCard::addTab(QWidget* page, const QString& label)
{
    if (!page) return -1;
    const int pageIndex = _pages->addWidget(page);
    const int tabIndex = _tabs->addTab(label);
    Q_ASSERT(pageIndex == tabIndex);
    return tabIndex;
}

void DeviceScopeCard::removeTab(int index)
{
    if (index < 0 || index >= _tabs->count()) return;
    QWidget* page = _pages->widget(index);
    QWidget* previousPage = _pages->currentWidget();
    {
        const QSignalBlocker blocker(_tabs);
        _tabs->removeTab(index);
    }
    if (page) _pages->removeWidget(page);
    const int current = _tabs->currentIndex();
    _pages->setCurrentIndex(current);
    if (_pages->currentWidget() != previousPage) emit currentChanged(current);
}

int DeviceScopeCard::currentIndex() const
{
    return _tabs->currentIndex();
}

void DeviceScopeCard::setCurrentIndex(int index)
{
    _tabs->setCurrentIndex(index);
}

void DeviceScopeCard::setTabToolTip(int index, const QString& tip)
{
    _tabs->setTabToolTip(index, tip);
}
