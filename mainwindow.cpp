#include "mainwindow.h"

#include <QDebug>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QRandomGenerator>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget *parent)
    : QWidget(parent)
    , m_mode(Mode::Cycle)
    , m_clickHandlerAlt(false)
    , m_rootLayout(nullptr)
    , m_paletteGrid(nullptr)
    , m_statusLabel(nullptr)
    , m_intervalLabel(nullptr)
    , m_modeLabel(nullptr)
{
    setupUi();

    m_timer.setInterval(600);
    rebuildTimeoutConnection();
    reconnectCanvasHandler();

    setStatus(QStringLiteral("Готово. Нажмите «Старт/Стоп», чтобы запустить таймер."));
}

MainWindow::~MainWindow()
{
    qDebug() << "[DESTROY] MainWindow";
    m_timer.stop();
}

void MainWindow::setupUi()
{
    setWindowTitle(QStringLiteral("ColorSwitcher"));
    resize(560, 640);

    m_rootLayout = new QVBoxLayout(this);
    m_rootLayout->addWidget(&m_canvas);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setWordWrap(true);
    m_rootLayout->addWidget(m_statusLabel);

    m_intervalLabel = new QLabel(this);
    m_modeLabel = new QLabel(this);
    m_rootLayout->addWidget(m_intervalLabel);
    m_rootLayout->addWidget(m_modeLabel);

    auto *controls = new QHBoxLayout();
    auto *btnStart = new QPushButton(QStringLiteral("Старт/Стоп"), this);
    auto *btnSlow = new QPushButton(QStringLiteral("Медленнее"), this);
    auto *btnFast = new QPushButton(QStringLiteral("Быстрее"), this);
    auto *btnMode = new QPushButton(QStringLiteral("Режим"), this);
    controls->addWidget(btnStart);
    controls->addWidget(btnSlow);
    controls->addWidget(btnFast);
    controls->addWidget(btnMode);
    m_rootLayout->addLayout(controls);

    auto *palHeader = new QLabel(
        QStringLiteral("Палитра (кнопки создаются динамически):"), this);
    m_rootLayout->addWidget(palHeader);

    m_paletteGrid = new QGridLayout();
    m_rootLayout->addLayout(m_paletteGrid);

    auto *palControls = new QHBoxLayout();
    auto *btnAdd = new QPushButton(QStringLiteral("+ Цвет"), this);
    auto *btnClear = new QPushButton(QStringLiteral("Очистить палитру"), this);
    auto *btnClick = new QPushButton(
        QStringLiteral("Сменить обработчик клика"), this);
    palControls->addWidget(btnAdd);
    palControls->addWidget(btnClear);
    palControls->addWidget(btnClick);
    m_rootLayout->addLayout(palControls);

    connect(btnStart, &QPushButton::clicked, this, &MainWindow::toggleTimer);
    connect(btnSlow, &QPushButton::clicked, this, &MainWindow::decreaseInterval);
    connect(btnFast, &QPushButton::clicked, this, &MainWindow::increaseInterval);
    connect(btnMode, &QPushButton::clicked, this, &MainWindow::cycleMode);
    connect(btnAdd, &QPushButton::clicked, this, &MainWindow::addColorButton);
    connect(btnClear, &QPushButton::clicked, this, &MainWindow::clearPaletteButtons);
    connect(btnClick, &QPushButton::clicked, this, &MainWindow::toggleClickHandler);

    m_rootLayout->addStretch();
    updateIntervalLabel();
    updateModeLabel();
}

void MainWindow::setStatus(const QString &text)
{
    if (m_statusLabel)
        m_statusLabel->setText(text);
}

void MainWindow::updateIntervalLabel()
{
    if (m_intervalLabel)
        m_intervalLabel->setText(
            QStringLiteral("Интервал: %1 мс").arg(m_timer.interval()));
}

void MainWindow::updateModeLabel()
{
    if (m_modeLabel)
        m_modeLabel->setText(QStringLiteral("Режим: %1").arg(modeName()));
}

QString MainWindow::modeName() const
{
    switch (m_mode) {
    case Mode::Cycle: return QStringLiteral("Цикл");
    case Mode::Random: return QStringLiteral("Случайный");
    case Mode::Blink: return QStringLiteral("Мигание");
    }
    return {};
}

void MainWindow::toggleTimer()
{
    if (m_timer.isActive()) {
        m_timer.stop();
        setStatus(QStringLiteral("Таймер остановлен."));
        qDebug() << "[TIMER] stop()";
    } else {
        m_timer.start();
        setStatus(QStringLiteral("Таймер запущен (интервал %1 мс).")
                      .arg(m_timer.interval()));
        qDebug() << "[TIMER] start(), interval =" << m_timer.interval();
    }
}

void MainWindow::decreaseInterval()
{
    m_timer.setInterval(qMin(m_timer.interval() + 200, 5000));
    updateIntervalLabel();
    qDebug() << "[TIMER] interval ->" << m_timer.interval();
}

void MainWindow::increaseInterval()
{
    m_timer.setInterval(qMax(m_timer.interval() - 200, 50));
    updateIntervalLabel();
    qDebug() << "[TIMER] interval ->" << m_timer.interval();
}

void MainWindow::cycleMode()
{
    switch (m_mode) {
    case Mode::Cycle: m_mode = Mode::Random; break;
    case Mode::Random: m_mode = Mode::Blink; break;
    case Mode::Blink: m_mode = Mode::Cycle; break;
    }

    updateModeLabel();
    rebuildTimeoutConnection();
    setStatus(QStringLiteral("Режим переключён: %1").arg(modeName()));
    qDebug() << "[TIMER] режим ->" << modeName()
             << "(переподключение timeout)";
}

void MainWindow::rebuildTimeoutConnection()
{
    if (m_timeoutConn) {
        disconnect(m_timeoutConn);
        m_timeoutConn = QMetaObject::Connection();
    }

    m_timeoutConn = connect(&m_timer, &QTimer::timeout, this, [this]() {
        switch (m_mode) {
        case Mode::Cycle: m_canvas.stepForward(); break;
        case Mode::Random: m_canvas.setRandomColor(); break;
        case Mode::Blink: m_canvas.toggleBlink(); break;
        }
        qDebug() << "[TIMER] timeout, режим =" << modeName()
                 << ", цвет =" << m_canvas.currentColor().name();
    });
}

void MainWindow::reconnectCanvasHandler()
{
    if (m_clickConn) {
        disconnect(m_clickConn);
        m_clickConn = QMetaObject::Connection();
    }

    if (!m_clickHandlerAlt) {
        m_clickConn = connect(&m_canvas, &ColorWidget::colorChanged, this,
                              [this](const QColor &color) {
            setStatus(QStringLiteral("Цвет холста: %1").arg(color.name()));
        });
    } else {
        m_clickConn = connect(&m_canvas, &ColorWidget::colorChanged, this,
                              [this](const QColor &color) {
            setWindowTitle(QStringLiteral("ColorSwitcher - %1").arg(color.name()));
        });
    }
}

void MainWindow::toggleClickHandler()
{
    m_clickHandlerAlt = !m_clickHandlerAlt;
    reconnectCanvasHandler();
    setStatus(m_clickHandlerAlt
                  ? QStringLiteral("Обработчик: заголовок окна.")
                  : QStringLiteral("Обработчик: строка состояния."));
    qDebug() << "[CANVAS] переподключение colorChanged, альтернатива ="
             << m_clickHandlerAlt;
}

void MainWindow::addColorButton()
{
    const QColor color = QColor::fromHsv(
        QRandomGenerator::global()->bounded(360),
        200 + QRandomGenerator::global()->bounded(55),
        200 + QRandomGenerator::global()->bounded(55));

    auto *button = new PaletteButton(color, this);
    const int index = m_paletteButtons.size();
    m_paletteGrid->addWidget(button, index / 4, index % 4);

    QMetaObject::Connection connection =
        connect(button, &QPushButton::clicked, this, [this, button]() {
            m_canvas.addColor(button->color());
            setStatus(QStringLiteral("Добавлен цвет %1 в палитру холста.")
                          .arg(button->color().name()));
            qDebug() << "[DYNAMIC] нажата кнопка-цвет" << button->color().name();
        });

    m_paletteButtons.append(QPointer<PaletteButton>(button));
    m_dynamicConnections.append(connection);

    setStatus(QStringLiteral("Создана кнопка-цвет %1.")
                  .arg(color.name()));
    qDebug() << "[DYNAMIC] создано соединение для" << color.name()
             << ", всего динамических:" << m_dynamicConnections.size();
}

void MainWindow::clearPaletteButtons()
{
    for (QMetaObject::Connection &connection : m_dynamicConnections) {
        if (connection)
            disconnect(connection);
    }
    m_dynamicConnections.clear();

    for (QPointer<PaletteButton> &button : m_paletteButtons) {
        if (!button.isNull())
            button->deleteLater();
    }
    m_paletteButtons.clear();

    m_canvas.resetPalette();
    setStatus(QStringLiteral("Палитра очищена: соединения разорваны, кнопки удалены."));
    qDebug() << "[DYNAMIC] очищена палитра, соединения разорваны";
}
