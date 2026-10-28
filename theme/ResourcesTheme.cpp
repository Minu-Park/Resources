#include "ResourcesTheme.h"
#include "ThemeRuntime.h"
#include "ThemeStyleSheet.h"
#include <QApplication>
#include <QWidget>
#include <QVariant>
#include <QDir>
#include <QFile>
#include <QFontDatabase>
#include <QIcon>
#include <QJsonDocument>
#include <QHash>
#include <QSet>
#include <QPalette>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStyle>
#include <QThread>
#include <QDebug>
#include <cmath>

// Q_INIT_RESOURCE must resolve the generated global resource symbol.
static void initThemeResources() { Q_INIT_RESOURCE(Resources); }

namespace Resources {
namespace {
QString canonicalSelector(const QString& selector)
{
    static const QRegularExpression state(QStringLiteral("(?<!:):!?[A-Za-z][\\w-]*"));
    QStringList result;
    for (auto compound : selector.simplified().split(QLatin1Char(' '), Qt::SkipEmptyParts)) {
        QStringList states;
        auto matches = state.globalMatch(compound);
        while (matches.hasNext()) states.append(matches.next().captured());
        states.sort();
        compound.remove(state);
        result.append(compound + states.join(QString()));
    }
    return result.join(QLatin1Char(' '));
}
struct ThemeDeclaration { QString property; QString value; int id; };
struct ThemeRule { QStringList selectors; QList<ThemeDeclaration> declarations; };

QList<ThemeRule> themeRules(QString source, QString* error = nullptr)
{
    source.remove(QRegularExpression(QStringLiteral("/\\*.*?\\*/"), QRegularExpression::DotMatchesEverythingOption));
    static const QRegularExpression block(QStringLiteral("([^{}]+)\\{([^{}]*)\\}"));
    static const QRegularExpression declaration(QStringLiteral("([\\w-]+)\\s*:\\s*([^;{}]+);"));
    QList<ThemeRule> rules;
    int id = 0;
    auto blocks = block.globalMatch(source);
    while (blocks.hasNext()) {
        const auto match = blocks.next();
        ThemeRule rule;
        for (const auto& selector : match.captured(1).split(QLatin1Char(','), Qt::SkipEmptyParts))
            rule.selectors.append(selector.simplified());
        const auto body = match.captured(2);
        auto declarations = declaration.globalMatch(body);
        int parsedEnd = 0;
        while (declarations.hasNext()) {
            const auto item = declarations.next();
            if (!body.mid(parsedEnd, item.capturedStart() - parsedEnd).trimmed().isEmpty()) {
                if (error) *error = QStringLiteral("Unparsed theme declaration in %1").arg(match.captured(1).trimmed());
                return {};
            }
            parsedEnd = item.capturedEnd();
            auto property = item.captured(1);
            if (!property.startsWith(QLatin1String("qproperty-"))) property = property.toLower();
            rule.declarations.append({property, item.captured(2).trimmed(), id++});
        }
        if (!body.mid(parsedEnd).trimmed().isEmpty()) {
            if (error) *error = QStringLiteral("Unparsed theme declaration in %1").arg(match.captured(1).trimmed());
            return {};
        }
        rules.append(rule);
    }
    return rules;
}
bool validateFragmentOwnership(const QString& name, const QString& source, QString* error)
{
    if (name == QLatin1String("00_base.qss")) return true;
    static const QSet<QString> commonTypes = {QStringLiteral("QPushButton"), QStringLiteral("QToolButton"),
        QStringLiteral("QLineEdit"), QStringLiteral("QComboBox"), QStringLiteral("QAbstractSpinBox"),
        QStringLiteral("QSpinBox"), QStringLiteral("QDoubleSpinBox"), QStringLiteral("QCheckBox"), QStringLiteral("QRadioButton")};
    for (const auto& rule : themeRules(source)) {
        for (const auto& selector : rule.selectors) {
            if (!commonTypes.contains(selector)) continue;
            if (error) *error = QStringLiteral("%1 redefines %2; standard control templates belong to 00_base.qss.").arg(name, selector);
            return false;
        }
    }
    return true;
}

QString definitionKey(const QString& selector, const QString& property)
{
    return canonicalSelector(selector) + QChar(0x1f) + property;
}

// Platform variants are explicit manager-owned patches, resolved before Qt receives QSS.
} // namespace
QString detail::composePlatformVariant(const QString& source, const QString& platform)
{
    if (platform.isEmpty()) return source;
    const auto rules = themeRules(source + platform);
    QHash<QString, int> finalDefinitions;
    for (const auto& rule : rules)
        for (const auto& selector : rule.selectors)
            for (const auto& declaration : rule.declarations)
                finalDefinitions.insert(definitionKey(selector, declaration.property), declaration.id);
    QString result;
    for (const auto& rule : rules) {
        QMap<QString, QStringList> groups;
        for (const auto& selector : rule.selectors) {
            QString body;
            for (const auto& declaration : rule.declarations) {
                if (finalDefinitions.value(definitionKey(selector, declaration.property)) == declaration.id)
                    body += declaration.property + QLatin1Char(':') + declaration.value + QLatin1Char(';');
            }
            if (!body.isEmpty()) groups[body].append(selector);
        }
        for (auto group = groups.begin(); group != groups.end(); ++group)
            result += group.value().join(QLatin1Char(',')) + QLatin1Char('{') + group.key() + QStringLiteral("}\n");
    }
    return result;
}

namespace {
QJsonObject readObject(const QString& path, QString* error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) *error = QStringLiteral("Could not read %1: %2").arg(path, file.errorString());
        return {};
    }
    QJsonParseError parse;
    const auto doc = QJsonDocument::fromJson(file.readAll(), &parse);
    if (parse.error != QJsonParseError::NoError || !doc.isObject()) {
        if (error) *error = QStringLiteral("Invalid JSON object: %1").arg(path);
        return {};
    }
    return doc.object();
}
const QJsonObject& presentationTokens()
{
    static const auto values = readObject(QStringLiteral(":/Resources/theme/presentation.json"), nullptr);
    return values;
}
const QJsonObject& factoryMetrics()
{
    static const auto values = readObject(QStringLiteral(":/Resources/theme/metrics.json"), nullptr);
    return values;
}
}

ResourcesTheme::ResourcesTheme() : palette_(defaultThemePalette())
{
    initThemeResources();
    metrics_ = factoryMetrics();
    QString error;
    if (presentationTokens().isEmpty() || !validateMetrics(metrics_, &error))
        qFatal("Invalid embedded theme contract: %s", qPrintable(error));
}

ResourcesTheme& ResourcesTheme::instance()
{
    static QPointer<ResourcesTheme> owner;
    if (!owner) owner = new ResourcesTheme;
    return *owner;
}


ThemePalette ResourcesTheme::defaultThemePalette()
{
    return {
        {QStringLiteral("text"), QColor(QStringLiteral("#3c3c3b"))},
        {QStringLiteral("selectionAccent"), QColor(QStringLiteral("#15487f"))},
        {QStringLiteral("secondaryText"), QColor(QStringLiteral("#6e6e6d"))},
        {QStringLiteral("disabledText"), QColor(QStringLiteral("#9b9b9a"))},
        {QStringLiteral("surface"), QColor(QStringLiteral("#ffffff"))},
        {QStringLiteral("workspace"), QColor(QStringLiteral("#eeeeee"))},
        {QStringLiteral("border"), QColor(QStringLiteral("#d9d9d8"))},
        {QStringLiteral("controlBorder"), QColor(QStringLiteral("#d9d9d8"))},
        {QStringLiteral("hoverBorder"), QColor(QStringLiteral("#8eafd1"))},
        {QStringLiteral("focusBorder"), QColor(QStringLiteral("#15487f"))},
        {QStringLiteral("hover"), QColor(QStringLiteral("#f4f4f3"))},
        {QStringLiteral("pressed"), QColor(QStringLiteral("#eeeeed"))},
        {QStringLiteral("disabledSurface"), QColor(QStringLiteral("#f7f7f7"))},
        {QStringLiteral("selection"), QColor(QStringLiteral("#f4f7fb"))},
        {QStringLiteral("current"), QColor(QStringLiteral("#eef3f9"))},
        {QStringLiteral("itemHover"), QColor(QStringLiteral("#f6f9fc"))},
        {QStringLiteral("statusBorder"), QColor(QStringLiteral("#d4d4d3"))},
        {QStringLiteral("progressTrack"), QColor(QStringLiteral("#ededed"))},
        {QStringLiteral("progressFill"), QColor(QStringLiteral("#1e5ea6"))},
        {QStringLiteral("shadow"), QColor(QStringLiteral("#3c3c3b"))},
        {QStringLiteral("warning"), QColor(QStringLiteral("#fb8c00"))},
        {QStringLiteral("warningText"), QColor(QStringLiteral("#b45309"))},
        {QStringLiteral("error"), QColor(QStringLiteral("#dc2626"))},
        {QStringLiteral("errorText"), QColor(QStringLiteral("#b44747"))},
        {QStringLiteral("success"), QColor(QStringLiteral("#10b981"))},
        {QStringLiteral("nativePath"), QColor(QStringLiteral("#1565c0"))},
        {QStringLiteral("sampledPath"), QColor(QStringLiteral("#edb078"))}
    };
}


void ResourcesTheme::applyInstalledStyleSheet()
{
    if (!app_) {
        return;
    }

    const QString styleSheet = renderStyleSheet(detail::composePlatformVariant(styleSheetTemplate_, platformStyleSheet_), metrics_, palette_);
    app_->setStyleSheet(styleSheet);

    QPalette palette = app_->palette();
    palette.setColor(QPalette::WindowText, palette_.value(QStringLiteral("text")));
    palette.setColor(QPalette::Text, palette_.value(QStringLiteral("text")));
    palette.setColor(QPalette::ButtonText, palette_.value(QStringLiteral("text")));
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
    // Native styles can paint selected tree branches from Accent instead of QSS Highlight.
    palette.setColor(QPalette::Accent, palette_.value(QStringLiteral("text")));
#endif
    palette.setColor(QPalette::Highlight, palette_.value(QStringLiteral("selection")));
    palette.setColor(QPalette::HighlightedText, palette_.value(QStringLiteral("text")));
    palette.setColor(QPalette::Link, palette_.value(QStringLiteral("text")));
    palette.setColor(QPalette::LinkVisited, palette_.value(QStringLiteral("secondaryText")));
    palette.setColor(QPalette::Base, palette_.value(QStringLiteral("surface")));
    palette.setColor(QPalette::Window, palette_.value(QStringLiteral("surface")));
    for (auto role : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText})
        palette.setColor(QPalette::Disabled, role, palette_.value(QStringLiteral("disabledText")));
    app_->setPalette(palette);
}

ThemePalette ResourcesTheme::themePalette() const
{
    return palette_;
}

bool ResourcesTheme::setThemePalette(const ThemePalette& colors)
{
    Q_ASSERT(!app_ || QThread::currentThread() == app_->thread());
    if (colors.isEmpty()) return false;
    auto candidate = palette_;
    for (auto it = colors.cbegin(); it != colors.cend(); ++it) {
        if (!candidate.contains(it.key()) || !it.value().isValid()) return false;
        candidate[it.key()] = QColor(it.value().red(), it.value().green(), it.value().blue());
    }
    if (candidate == palette_) return true;
    palette_ = candidate;
    applyInstalledStyleSheet();
    // Existing popups and custom-painted chrome may not receive another style event until shown.
    refreshWidgets();
    return true;
}

void ResourcesTheme::resetThemePalette()
{
    setThemePalette(defaultThemePalette());
}

ThemePalette ResourcesTheme::defaultPaletteGroup(PaletteGroup group)
{
    auto colors = defaultThemePalette();
    const QStringList fontRoles = {QStringLiteral("text"), QStringLiteral("secondaryText"),
        QStringLiteral("disabledText"), QStringLiteral("warningText"), QStringLiteral("errorText")};
    for (auto it = colors.begin(); it != colors.end();) {
        if (fontRoles.contains(it.key()) != (group == PaletteGroup::Font)) it = colors.erase(it);
        else ++it;
    }
    return colors;
}

bool ResourcesTheme::savePaletteFile(const QString& path, QString* error) const
{
    if (error) error->clear();
    QJsonObject colors;
    const auto roles = defaultThemePalette();
    const auto current = themePalette();
    for (auto it = roles.cbegin(); it != roles.cend(); ++it)
        colors.insert(it.key(), current.value(it.key()).name(QColor::HexRgb));
    const QByteArray bytes = QJsonDocument(QJsonObject{{QStringLiteral("colors"), colors}}).toJson();
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
        if (error) *error = file.errorString();
        return false;
    }
    return true;
}

bool ResourcesTheme::loadPaletteFile(const QString& path, QString* error)
{
    if (error) error->clear();
    const auto fail = [this, error](const QString& message) {
        resetThemePalette();
        if (error) *error = message + QStringLiteral(" Built-in defaults restored.");
        return false;
    };
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return fail(file.errorString());
    if (file.size() > 65536) return fail(QStringLiteral("Palette file exceeds 64 KiB."));
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
        return fail(QStringLiteral("Invalid palette JSON: %1").arg(parseError.errorString()));
    const auto root = document.object();
    if (root.size() != 1 || !root.value(QStringLiteral("colors")).isObject())
        return fail(QStringLiteral("Expected a single colors object."));
    const auto colors = root.value(QStringLiteral("colors")).toObject();
    const auto roles = defaultThemePalette();
    if (colors.size() != roles.size()) return fail(QStringLiteral("Theme must contain every supported color role."));
    ThemePalette patch;
    const QRegularExpression rgb(QStringLiteral("^#[0-9a-fA-F]{6}$"));
    for (auto it = colors.begin(); it != colors.end(); ++it) {
        if (!roles.contains(it.key()) || !it.value().isString() || !rgb.match(it.value().toString()).hasMatch())
            return fail(QStringLiteral("Unknown role or invalid RGB color: %1").arg(it.key()));
        patch.insert(it.key(), QColor(it.value().toString()));
    }
    return setThemePalette(patch);
}


bool ResourcesTheme::validateMetrics(const QJsonObject& values, QString* error)
{
    const auto& expected = factoryMetrics();
    if (expected.isEmpty() || values.keys() != expected.keys()) {
        if (error) *error = QStringLiteral("Widget metrics must contain exactly the supported metric names.");
        return false;
    }
    for (auto it = values.begin(); it != values.end(); ++it) {
        const double number = it.value().toDouble(-1);
        if (!it.value().isDouble() || !std::isfinite(number)
            || number <= 0 || number > 256 || number != std::floor(number)) {
            if (error) *error = QStringLiteral("Invalid positive logical-pixel metric: %1").arg(it.key());
            return false;
        }
    }
    return true;
}

QString ResourcesTheme::renderStyleSheet(QString source, const QJsonObject& metrics,
                                         const ThemePalette& palette)
{
    auto tokens = presentationTokens();
    for (auto it = metrics.begin(); it != metrics.end(); ++it) tokens.insert(it.key(), it.value());
    for (auto it = tokens.begin(); it != tokens.end(); ++it)
        source.replace(QLatin1Char('@') + it.key() + QLatin1Char('@'), it.value().toVariant().toString());
    for (auto it = palette.begin(); it != palette.end(); ++it) {
        const auto name = it.key().toUpper();
        source.replace(QLatin1Char('@') + name + QLatin1Char('@'), it.value().name(QColor::HexRgb));
        source.replace(QLatin1Char('@') + name + QStringLiteral("_RGB@"),
            QStringLiteral("%1, %2, %3").arg(it.value().red()).arg(it.value().green()).arg(it.value().blue()));
    }
    return source;
}

bool ResourcesTheme::validateStyleSheet(const QString& source, const QJsonObject& metrics,
                                        QString* error)
{
    QSet<QString> definitions;
    QString parseError;
    const auto rules = themeRules(source, &parseError);
    if (!parseError.isEmpty()) {
        if (error) *error = parseError;
        return false;
    }
    for (const auto& rule : rules) {
        for (const auto& selector : rule.selectors) {
            for (const auto& declaration : rule.declarations) {
                const auto key = definitionKey(selector, declaration.property);
                if (definitions.contains(key)) {
                    if (error) *error = QStringLiteral("Duplicate theme definition: %1 { %2 }")
                        .arg(canonicalSelector(selector), declaration.property);
                    return false;
                }
                definitions.insert(key);
            }
        }
    }
    const auto compiled = renderStyleSheet(source, metrics, defaultThemePalette());
    const auto unresolved = QRegularExpression(QStringLiteral("@[A-Za-z_][A-Za-z_0-9]*@")).match(compiled);
    if (unresolved.hasMatch()) {
        if (error) *error = QStringLiteral("Unknown theme token: %1").arg(unresolved.captured());
        return false;
    }
    return true;
}

int ResourcesTheme::metric(const char* name) const
{
    auto value = metrics_.value(QLatin1String(name));
    if (value.isUndefined()) value = presentationTokens().value(QLatin1String(name));
    if (!value.isDouble() || value.toInt() <= 0) qFatal("Invalid theme metric: %s", name);
    return value.toInt();
}

ControlMetrics ResourcesTheme::metrics(ControlKind kind, Density density) const
{
    if (kind == ControlKind::TreeFrame)
        return {0, 0, 0, 0, metric("TREE_FRAME_BORDER_WIDTH"), metric("TREE_FRAME_RADIUS")};
    if (kind == ControlKind::Field)
        return {metric(density == Density::Compact ? "COMPACT_FIELD_CONTENT_HEIGHT" : "FIELD_CONTENT_HEIGHT"),
                0, metric("FIELD_PADDING_X"), 0, metric("CONTROL_BORDER_WIDTH"), metric("CONTROL_RADIUS")};
    if (density == Density::Compact)
        return {metric("COMPACT_ACTION_CONTENT_SIZE"), metric("COMPACT_ACTION_CONTENT_SIZE"), 0, 0,
                metric("CONTROL_BORDER_WIDTH"), metric("COMPACT_ACTION_RADIUS")};
    return {metric("CONTROL_HEIGHT"), 0, metric("TOOLBUTTON_PADDING_X"), metric("TOOLBUTTON_PADDING_Y"),
            metric("CONTROL_BORDER_WIDTH"), metric("TOOLBUTTON_RADIUS")};
}

void ResourcesTheme::refreshWidget(QWidget& widget)
{
    widget.style()->unpolish(&widget);
    widget.style()->polish(&widget);
    widget.updateGeometry();
    widget.update();
}

void ResourcesTheme::setDensity(QWidget& widget, Density density)
{
    Q_ASSERT(QThread::currentThread() == widget.thread());
    const QString name = density == Density::Compact ? QStringLiteral("compact")
        : QStringLiteral("default");
    widget.setProperty("resourcesAutomaticDensity", false);
    if (widget.property("resourcesDensity").toString() == name) return;
    widget.setProperty("resourcesDensity", name);
    refreshWidget(widget);
}

void ResourcesTheme::polishWidget(QWidget& widget)
{
    if (!widget.inherits("QToolButton")) return;
    if (widget.property("resourcesDensity").isValid()
        && !widget.property("resourcesAutomaticDensity").toBool()) return;
    bool compact = false;
    // Existing consumer surfaces are admitted here once; their QSS shares one density rule.
    for (auto* parent = widget.parentWidget(); parent; parent = parent->parentWidget()) {
        // Qt's tab scrollers own their borderless slot, including inside device panels.
        if (parent->inherits("QTabBar")) break;
        if (parent->inherits("QFramegrabberWidget") || parent->inherits("QCameraWidget")
            || parent->inherits("QGocatorWidget") || parent->inherits("QHeliotisC4Widget")
            || parent->inherits("QOneGenWidget") || parent->inherits("QOneGenDeviceWidget")
            || parent->inherits("StaticImageControlWidget") || parent->inherits("Static3DDataControlWidget")) {
            compact = true;
            break;
        }
    }
    const QString density = compact ? QStringLiteral("compact") : QStringLiteral("default");
    if (widget.property("resourcesDensity").toString() == density) return;
    widget.setProperty("resourcesAutomaticDensity", true);
    widget.setProperty("resourcesDensity", density);
    if (widget.testAttribute(Qt::WA_WState_Polished)) refreshWidget(widget);
}

void ResourcesTheme::refreshWidgets()
{
    if (!app_) return;
    for (QWidget* widget : QApplication::allWidgets()) {
        widget->updateGeometry();
        widget->update();
    }
}

bool ResourcesTheme::reloadThemeFromDirectory(const QString& directory, QString* error)
{
    if (error) error->clear();
    if (!app_ || themeFileNames_.isEmpty()) {
        if (error) *error = QStringLiteral("Install Resources before reloading a source theme.");
        return false;
    }
    Q_ASSERT(QThread::currentThread() == app_->thread());
    // Both consumers of geometry see the same validated candidate before style events are emitted.
    const auto candidateMetrics = readObject(QDir(directory).filePath(QStringLiteral("../metrics.json")), error);
    if (candidateMetrics.isEmpty() || !validateMetrics(candidateMetrics, error)) return false;
    QString candidate;
    for (const auto& name : themeFileNames_) {
        QFile file(QDir(directory).filePath(name));
        if (!file.open(QIODevice::ReadOnly)) {
            if (error) *error = QStringLiteral("Could not read theme fragment: %1").arg(file.fileName());
            return false;
        }
        const auto fragment = QString::fromUtf8(file.readAll());
        if (!validateFragmentOwnership(name, fragment, error)) return false;
        candidate += fragment + QLatin1Char('\n');
    }
    if (!validateStyleSheet(candidate, candidateMetrics, error)) return false;
    metrics_ = candidateMetrics;
    styleSheetTemplate_ = candidate;
    applyInstalledStyleSheet();
    refreshWidgets();
    return true;
}

void ResourcesTheme::install(QApplication& app)
{
    Q_ASSERT(QThread::currentThread() == app.thread());
    if (app_) {
        Q_ASSERT(app_ == &app);
        applyInstalledStyleSheet();
        refreshWidgets();
        return;
    }
    setParent(&app);
    app_ = &app;
    QApplication::setEffectEnabled(Qt::UI_FadeTooltip, false);
    QApplication::setEffectEnabled(Qt::UI_AnimateTooltip, false);
    QApplication::setEffectEnabled(Qt::UI_AnimateCombo, false);
    for (const auto& name : {"Inter-Regular.ttf", "Inter-Bold.ttf", "JetBrainsMono-Regular.ttf", "JetBrainsMono-Bold.ttf"}) {
        const auto file = QStringLiteral(":/Resources/theme/fonts/") + QLatin1String(name);
        if (QFontDatabase::addApplicationFont(file) == -1) qWarning() << "Failed to load font:" << file;
    }
    QFont font(presentationTokens().value(QStringLiteral("FONT_UI")).toString());
#if defined(Q_OS_MAC)
    font.setPointSizeF(12.0);
#else
    font.setPointSizeF(10.0);
#endif
    app.setFont(font);
    themeFileNames_ = {QStringLiteral("00_base.qss"), QStringLiteral("10_graphics_engine.qss"),
        QStringLiteral("20_statusbar.qss"), QStringLiteral("30_device_controls.qss"),
        QStringLiteral("40_chrome.qss"), QStringLiteral("50_static_image.qss"),
        QStringLiteral("60_processing.qss"), QStringLiteral("70_plugin_manager.qss")};
    QString source;
    for (const auto& name : themeFileNames_) {
        QFile file(QStringLiteral(":/Resources/theme/qss/") + name);
        if (!file.open(QIODevice::ReadOnly)) qFatal("Cannot read embedded theme: %s", qPrintable(name));
        const auto fragment = QString::fromUtf8(file.readAll());
        QString ownershipError;
        if (!validateFragmentOwnership(name, fragment, &ownershipError))
            qFatal("Invalid embedded template ownership: %s", qPrintable(ownershipError));
        source += fragment + QLatin1Char('\n');
    }
#ifdef Q_OS_WIN
    platformStyleSheet_ += QStringLiteral(
        "\nQWidget#ThemedDialogContainer { border-radius: 8px; }\n"
        "QWidget#ThemedDialogTitleBar { border-top-left-radius: 8px; border-top-right-radius: 8px; }\n"
        "QFrame#DockContainerWidget[floatingState=\"true\"],\n"
        "QDockWidget[floatingState=\"true\"] QFrame#DockContainerWidget {\n"
        "    border-bottom-left-radius: 7px;\n"
        "    border-bottom-right-radius: 7px;\n"
        "}\n"
        "QDockWidget[floatingState=\"true\"] QWidget#ThemedDockTitleBar {\n"
        "    border-top-left-radius: 8px;\n"
        "    border-top-right-radius: 8px;\n"
        "}\n"
        "QDockWidget[floatingState=\"true\"] QFrame#DockContainerWidget > QWidget {\n"
        "    border-bottom-left-radius: 6px;\n"
        "    border-bottom-right-radius: 6px;\n"
        "}\n"
        "QStatusBar {\n"
        "    border-bottom-left-radius: 7px;\n"
        "    border-bottom-right-radius: 7px;\n"
        "}\n"
    );
#endif

    styleSheetTemplate_ = source;
    QString error;
    if (!validateStyleSheet(styleSheetTemplate_, metrics_, &error)) qFatal("Invalid embedded theme: %s", qPrintable(error));
    // Install before applying QSS so native geometry, states and metrics agree during polish.
    runtime_ = installThemeRuntime(app, this);
    applyInstalledStyleSheet();
#if !defined(Q_OS_MAC)
    app.setWindowIcon(QIcon(QStringLiteral(":/Resources/AppIcons/AppIcon.png")));
#endif
}

}
