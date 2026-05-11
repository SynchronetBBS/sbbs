#include "mainwindow.h"
#include "settingsdialog.h"
#include <QToolBar>
#include <QMenuBar>
#include <QStatusBar>
#include <QTabBar>
#include <QInputDialog>
#include <QMessageBox>
#include <QSslError>
#include <QTimer>
#include <QRegularExpression>
#include <QApplication>

static const QStringList Servers = {"term", "mail", "ftp", "web", "srvc"};
static const QHash<QString, QString> ServerLabels = {
	{"term", "Terminal"}, {"mail", "Mail"}, {"ftp", "FTP"},
	{"web", "Web"}, {"srvc", "Services"},
};

static const QString DarkStyle = QStringLiteral(
	"QMainWindow { background-color: #2b2b2b; }"
	"QDockWidget { color: #cccccc; }"
	"QDockWidget::title { background-color: #353535; padding: 4px; border: 1px solid #555; }"
	"QMenuBar { background-color: #353535; color: #cccccc; }"
	"QMenuBar::item:selected { background-color: #505050; }"
	"QMenu { background-color: #353535; color: #cccccc; }"
	"QMenu::item:selected { background-color: #505050; }"
	"QToolBar { background-color: #353535; border: none; spacing: 2px; }"
	"QToolButton { color: #cccccc; padding: 4px 8px; }"
	"QToolButton:hover { background-color: #505050; }"
	"QStatusBar { background-color: #353535; color: #aaaaaa; }"
	"QPushButton { background-color: #404040; color: #cccccc; border: 1px solid #555; padding: 3px 8px; }"
	"QPushButton:hover { background-color: #505050; }"
	"QComboBox { background-color: #404040; color: #cccccc; border: 1px solid #555; }"
);

MainWindow::MainWindow(const QString &mqttHost, quint16 mqttPort,
                       const QString &bbsId,
                       const QString &mqttUser, const QString &mqttPass,
                       const QString &pskId, const QString &pskKey,
                       const QString &caFile, const QString &certFile,
                       const QString &keyFile,
                       QWidget *parent)
	: QMainWindow(parent)
	, m_settings("Synchronet", "Monitor")
	, m_mqtt(new MqttClient(this))
{
	setWindowTitle("Synchronet Monitor");
	resize(1200, 800);

	setupCentral();
	setupDocks();
	setupMenus();
	setupStatusbar();
	connectMqttSignals();
	restoreState();
	setupToolbar();
	applyGlobalStyle();
	applyLogMaxLines();

	if (!mqttHost.isEmpty()) m_settings.setValue("mqtt/host", mqttHost);
	if (mqttPort)            m_settings.setValue("mqtt/port", mqttPort);
	if (!bbsId.isEmpty())    m_settings.setValue("mqtt/bbs_id", bbsId);
	if (!mqttUser.isEmpty()) m_settings.setValue("mqtt/username", mqttUser);
	if (!mqttPass.isEmpty()) m_settings.setValue("mqtt/password", mqttPass);
	if (!pskId.isEmpty())    m_settings.setValue("mqtt/psk_identity", pskId);
	if (!pskKey.isEmpty())   m_settings.setValue("mqtt/psk_key", pskKey);
	if (!caFile.isEmpty())   m_settings.setValue("mqtt/ca_file", caFile);
	if (!certFile.isEmpty()) m_settings.setValue("mqtt/cert_file", certFile);
	if (!keyFile.isEmpty())  m_settings.setValue("mqtt/key_file", keyFile);

	QString host = m_settings.value("mqtt/host").toString();
	quint16 port = m_settings.value("mqtt/port", 0).toUInt();
	if (host.isEmpty() || !port) {
		statusBar()->showMessage("MQTT not configured — use File > Settings or pass -m HOST -p PORT", 10000);
		return;
	}

	m_mqtt->configure(
		host, port,
		m_settings.value("mqtt/bbs_id").toString(),
		m_settings.value("mqtt/username").toString(),
		m_settings.value("mqtt/password").toString(),
		m_settings.value("mqtt/psk_identity").toByteArray(),
		m_settings.value("mqtt/psk_key").toByteArray(),
		m_settings.value("mqtt/ca_file").toString(),
		m_settings.value("mqtt/cert_file").toString(),
		m_settings.value("mqtt/key_file").toString()
	);
	m_mqtt->connectToBroker();
}

void MainWindow::setupCentral()
{
	auto *w = new QWidget;
	setCentralWidget(w);
	w->hide();
}

void MainWindow::setupDocks()
{
	m_nodeWidget = new NodeWidget;
	m_nodeDock = makeDock("Nodes", m_nodeWidget);
	addDockWidget(Qt::TopDockWidgetArea, m_nodeDock);

	m_statsWidget = new StatsWidget;
	m_statsDock = makeDock("Statistics", m_statsWidget);
	addDockWidget(Qt::TopDockWidgetArea, m_statsDock);
	splitDockWidget(m_nodeDock, m_statsDock, Qt::Horizontal);

	QDockWidget *firstLogDock = nullptr;
	for (const auto &server : Servers) {
		QString label = ServerLabels.value(server);
		int savedLevel = m_settings.value("logLevel/" + server, 6).toInt();
		auto *log = new LogWidget(label, server);
		log->setLevel(savedLevel);
		m_mqtt->setLogLevel(server, savedLevel);
		connect(log, &LogWidget::levelChanged, this, [this](const QString &srv, int level) {
			m_mqtt->setLogLevel(srv, level);
			m_settings.setValue("logLevel/" + srv, level);
		});
		connect(log, &LogWidget::recycleServer, this, [this](const QString &srv) { forEachHost([this, srv](const QString &h) { m_mqtt->recycleServer(h, srv); }); });
		connect(log, &LogWidget::pauseServer, this, [this](const QString &srv) { forEachHost([this, srv](const QString &h) { m_mqtt->pauseServer(h, srv); }); });
		connect(log, &LogWidget::resumeServer, this, [this](const QString &srv) { forEachHost([this, srv](const QString &h) { m_mqtt->resumeServer(h, srv); }); });
		m_logPanes[server] = log;
		auto *dock = makeDock(label, log);
		m_logDocks[server] = dock;
		addDockWidget(Qt::BottomDockWidgetArea, dock);
		if (!firstLogDock) firstLogDock = dock;
		else tabifyDockWidget(firstLogDock, dock);
	}

	auto *bbsLog = new LogWidget("BBS", "bbs", "System");
	bbsLog->setLevel(m_settings.value("logLevel/bbs", 6).toInt());
	connect(bbsLog, &LogWidget::levelChanged, this, [this](const QString &srv, int level) {
		m_settings.setValue("logLevel/" + srv, level);
	});
	connect(bbsLog, &LogWidget::recycleServer, this, [this] { forEachHost([this](const QString &h) { m_mqtt->recycleHost(h); }); });
	connect(bbsLog, &LogWidget::pauseServer, this, [this] { forEachHost([this](const QString &h) { m_mqtt->pauseHost(h); }); });
	connect(bbsLog, &LogWidget::resumeServer, this, [this] { forEachHost([this](const QString &h) { m_mqtt->resumeHost(h); }); });
	m_logPanes["bbs"] = bbsLog;
	auto *bbsDock = makeDock("BBS", bbsLog);
	m_logDocks["bbs"] = bbsDock;
	addDockWidget(Qt::BottomDockWidgetArea, bbsDock);
	if (firstLogDock) tabifyDockWidget(firstLogDock, bbsDock);

	auto *eventsLog = new LogWidget("Events", "events", {});
	int eventsLevel = m_settings.value("logLevel/events", 6).toInt();
	eventsLog->setLevel(eventsLevel);
	m_mqtt->setLogLevel("events", eventsLevel);
	connect(eventsLog, &LogWidget::levelChanged, this, [this](const QString &srv, int level) {
		m_mqtt->setLogLevel(srv, level);
		m_settings.setValue("logLevel/" + srv, level);
	});
	m_logPanes["events"] = eventsLog;
	auto *eventsDock = makeDock("Events", eventsLog);
	m_logDocks["events"] = eventsDock;
	addDockWidget(Qt::BottomDockWidgetArea, eventsDock);
	if (firstLogDock) tabifyDockWidget(firstLogDock, eventsDock);

	m_clientWidget = new ClientWidget;
	m_clientDock = makeDock("Clients", m_clientWidget);
	addDockWidget(Qt::BottomDockWidgetArea, m_clientDock);
	if (firstLogDock) splitDockWidget(firstLogDock, m_clientDock, Qt::Horizontal);

	m_loginAttemptsWidget = new LoginAttemptsWidget(Servers, ServerLabels);
	m_loginAttemptsDock = makeDock("Login Attempts", m_loginAttemptsWidget);
	addDockWidget(Qt::BottomDockWidgetArea, m_loginAttemptsDock);
	tabifyDockWidget(m_clientDock, m_loginAttemptsDock);

	m_maxConcurrentWidget = new MaxConcurrentWidget;
	m_maxConcurrentDock = makeDock("Max Connections", m_maxConcurrentWidget);
	addDockWidget(Qt::BottomDockWidgetArea, m_maxConcurrentDock);
	tabifyDockWidget(m_loginAttemptsDock, m_maxConcurrentDock);

	m_actionWidget = new ActionWidget;
	m_actionDock = makeDock("Activity", m_actionWidget);
	addDockWidget(Qt::BottomDockWidgetArea, m_actionDock);
	tabifyDockWidget(m_maxConcurrentDock, m_actionDock);

	if (firstLogDock) firstLogDock->raise();
}

QDockWidget *MainWindow::makeDock(const QString &title, QWidget *widget)
{
	auto *dock = new QDockWidget(title, this);
	dock->setObjectName("dock_" + title);
	dock->setWidget(widget);
	dock->setAllowedAreas(Qt::AllDockWidgetAreas);
	dock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable | QDockWidget::DockWidgetClosable);
	return dock;
}

void MainWindow::setupMenus()
{
	auto *menubar = this->menuBar();

	// File
	auto *fileMenu = menubar->addMenu("&File");
	auto *settingsAct = fileMenu->addAction("&Settings...");
	settingsAct->setShortcut(QKeySequence("Ctrl+,"));
	connect(settingsAct, &QAction::triggered, this, [this] {
		SettingsDialog dlg(&m_settings, this);
		if (dlg.exec()) {
			m_mqtt->disconnectFromBroker();
			m_mqtt->configure(
				m_settings.value("mqtt/host").toString(),
				m_settings.value("mqtt/port", 0).toUInt(),
				m_settings.value("mqtt/bbs_id").toString(),
				m_settings.value("mqtt/username").toString(),
				m_settings.value("mqtt/password").toString(),
				m_settings.value("mqtt/psk_identity").toByteArray(),
				m_settings.value("mqtt/psk_key").toByteArray(),
				m_settings.value("mqtt/ca_file").toString(),
				m_settings.value("mqtt/cert_file").toString(),
				m_settings.value("mqtt/key_file").toString()
			);
			m_mqtt->connectToBroker();
			applyLogMaxLines();
		}
	});
	fileMenu->addSeparator();
	auto *quitAct = fileMenu->addAction("&Quit");
	quitAct->setShortcut(QKeySequence::Quit);
	connect(quitAct, &QAction::triggered, this, &QWidget::close);

	// View
	auto *viewMenu = menubar->addMenu("&View");
	int shortcutNum = 1;
	auto addDockToggle = [&](QDockWidget *dock) {
		auto *act = dock->toggleViewAction();
		if (shortcutNum <= 9)
			act->setShortcut(QKeySequence(QStringLiteral("Alt+%1").arg(shortcutNum++)));
		viewMenu->addAction(act);
	};
	addDockToggle(m_nodeDock);
	addDockToggle(m_statsDock);
	addDockToggle(m_clientDock);
	addDockToggle(m_loginAttemptsDock);
	addDockToggle(m_actionDock);
	addDockToggle(m_maxConcurrentDock);
	viewMenu->addSeparator();
	for (const auto &key : Servers + QStringList{"bbs", "events"})
		if (auto *dock = m_logDocks.value(key))
			addDockToggle(dock);
	viewMenu->addSeparator();
	m_darkAction = viewMenu->addAction("&Dark Mode");
	m_darkAction->setCheckable(true);
	m_darkAction->setChecked(m_settings.value("ui/dark_mode", true).toBool());
	connect(m_darkAction, &QAction::toggled, this, &MainWindow::setDarkMode);
	viewMenu->addSeparator();
	auto *resetAct = viewMenu->addAction("&Reset Layout");
	resetAct->setShortcut(QKeySequence("Ctrl+Shift+R"));
	connect(resetAct, &QAction::triggered, this, [this] {
		m_settings.remove("window/state");
		QTimer::singleShot(0, this, [this] {
			for (auto *dock : findChildren<QDockWidget *>()) {
				dock->setFloating(false);
				dock->setVisible(true);
			}
			QList<QDockWidget *> logDockList;
			for (const auto &key : Servers + QStringList{"bbs", "events"})
				if (auto *d = m_logDocks.value(key))
					logDockList.append(d);
			for (int i = 1; i < logDockList.size(); ++i)
				tabifyDockWidget(logDockList[0], logDockList[i]);
			tabifyDockWidget(m_clientDock, m_loginAttemptsDock);
			tabifyDockWidget(m_loginAttemptsDock, m_maxConcurrentDock);
			tabifyDockWidget(m_maxConcurrentDock, m_actionDock);
			if (!logDockList.isEmpty()) logDockList[0]->raise();
		});
	});

	// BBS (host-level)
	auto *globalMenu = menubar->addMenu("&BBS");
	connect(globalMenu->addAction("&Recycle All"), &QAction::triggered, this, [this] { forEachHost([this](const QString &h) { m_mqtt->recycleHost(h); }); });
	connect(globalMenu->addAction("&Pause All"), &QAction::triggered, this, [this] { forEachHost([this](const QString &h) { m_mqtt->pauseHost(h); }); });
	connect(globalMenu->addAction("Resu&me All"), &QAction::triggered, this, [this] { forEachHost([this](const QString &h) { m_mqtt->resumeHost(h); }); });
	globalMenu->addSeparator();
	connect(globalMenu->addAction("&Clear All Login Attempts"), &QAction::triggered, this, [this] { forEachHost([this](const QString &h) { m_mqtt->clearHost(h); }); });
	globalMenu->addSeparator();
	connect(globalMenu->addAction("Force &Timed Event..."), &QAction::triggered, this, [this] {
		auto code = QInputDialog::getText(this, "Force Timed Event", "Event code:");
		if (!code.isEmpty()) m_mqtt->triggerEvent(code);
	});
	connect(globalMenu->addAction("Force &Network Callout..."), &QAction::triggered, this, [this] {
		auto hubId = QInputDialog::getText(this, "Force Network Callout", "QHub ID:");
		if (!hubId.isEmpty()) m_mqtt->triggerCallout(hubId);
	});

	// Terminal
	auto *bbsMenu = menubar->addMenu("&Terminal");
	connect(bbsMenu->addAction("&Recycle"), &QAction::triggered, this, [this] { forEachHost([this](const QString &h) { m_mqtt->recycleServer(h, "term"); }); });
	connect(bbsMenu->addAction("&Pause"), &QAction::triggered, this, [this] { forEachHost([this](const QString &h) { m_mqtt->pauseServer(h, "term"); }); });
	connect(bbsMenu->addAction("Resu&me"), &QAction::triggered, this, [this] { forEachHost([this](const QString &h) { m_mqtt->resumeServer(h, "term"); }); });
	bbsMenu->addSeparator();
	connect(bbsMenu->addAction("&Clear Login Attempts"), &QAction::triggered, this, [this] { forEachHost([this](const QString &h) { m_mqtt->clearServer(h, "term"); }); });

	// Per-server menus
	for (const auto &server : Servers) {
		if (server == "term") continue;
		QString label = ServerLabels.value(server);
		auto *srvMenu = menubar->addMenu("&" + label);
		connect(srvMenu->addAction("&Recycle"), &QAction::triggered, this, [this, server] { forEachHost([this, server](const QString &h) { m_mqtt->recycleServer(h, server); }); });
		connect(srvMenu->addAction("&Pause"), &QAction::triggered, this, [this, server] { forEachHost([this, server](const QString &h) { m_mqtt->pauseServer(h, server); }); });
		connect(srvMenu->addAction("Resu&me"), &QAction::triggered, this, [this, server] { forEachHost([this, server](const QString &h) { m_mqtt->resumeServer(h, server); }); });
		srvMenu->addSeparator();
		connect(srvMenu->addAction("&Clear Login Attempts"), &QAction::triggered, this, [this, server] { forEachHost([this, server](const QString &h) { m_mqtt->clearServer(h, server); }); });
	}
}

void MainWindow::setupToolbar()
{
	auto *toolbar = new QToolBar("Main");
	toolbar->setObjectName("toolbar_main");
	toolbar->setMovable(false);
	addToolBar(toolbar);

	for (auto &[name, dock] : std::initializer_list<std::pair<const char *, QDockWidget *>>{
		{"Nodes", m_nodeDock}, {"Stats", m_statsDock}, {"Clients", m_clientDock},
		{"Logins", m_loginAttemptsDock}, {"Max Conn", m_maxConcurrentDock},
		{"Activity", m_actionDock},
	}) {
		auto *act = new QAction(name, this);
		connect(act, &QAction::triggered, this, [this, d = dock] { showDock(d); });
		toolbar->addAction(act);
	}

	toolbar->addSeparator();
	for (const auto &key : Servers + QStringList{"bbs", "events"}) {
		static const QHash<QString, QString> extraLabels = {{"bbs", "BBS"}, {"events", "Events"}};
		QString label = ServerLabels.value(key, extraLabels.value(key, key));
		auto *act = new QAction(label, this);
		connect(act, &QAction::triggered, this, [this, key] {
			if (auto *dock = m_logDocks.value(key))
				showDock(dock);
		});
		toolbar->addAction(act);
	}

	toolbar->addSeparator();
	m_hostCombo = new QComboBox;
	m_hostCombo->addItem("All Hosts");
	m_hostCombo->setToolTip("Filter display and target host for control actions");
	connect(m_hostCombo, &QComboBox::currentIndexChanged, this, [this] {
		QString host = selectedHost();
		for (auto *pane : m_logPanes)
			pane->setHostFilter(host);
	});
	toolbar->addWidget(m_hostCombo);

	toolbar->addSeparator();
	m_connectBtn = toolbar->addAction("Connect");
	connect(m_connectBtn, &QAction::triggered, this, [this] {
		if (m_mqtt->isConnected()) {
			m_mqtt->disconnectFromBroker();
		} else {
			m_mqtt->connectToBroker();
		}
	});
}

void MainWindow::setupStatusbar()
{
	auto *sb = statusBar();
	m_bbsNameLabel = new QLabel;
	sb->addWidget(m_bbsNameLabel);
	for (const auto &server : Servers) {
		auto *lbl = new QLabel(ServerLabels.value(server) + ": --");
		m_serverStateLabels[server] = lbl;
		sb->addWidget(lbl);
	}
	m_clientsLabel = new QLabel("Clients: 0");
	m_servedLabel = new QLabel("Served: 0");
	m_failedLabel = new QLabel("Failed: 0");
	m_errorsLabel = new QLabel("Errors: 0");
	m_mqttLabel = new QLabel("MQTT: Disconnected");
	sb->addPermanentWidget(m_clientsLabel);
	sb->addPermanentWidget(m_servedLabel);
	sb->addPermanentWidget(m_failedLabel);
	sb->addPermanentWidget(m_errorsLabel);
	sb->addPermanentWidget(m_mqttLabel);
}

void MainWindow::connectMqttSignals()
{
	connect(m_mqtt, &MqttClient::hostDiscovered, this, [this](const QString &host) {
		if (m_hostCombo->findText(host) < 0)
			m_hostCombo->addItem(host);
		if (m_hostCombo->count() == 2)
			m_hostCombo->setCurrentIndex(1);
	});
	connect(m_mqtt, &MqttClient::bbsName, this, [this](const QString &name) {
		m_bbsNameLabel->setText(name);
		setWindowTitle(name + " - qtmonitor");
	});
	connect(m_mqtt, &MqttClient::brokerVersion, this, [this](const QString &version) {
		if (!version.startsWith("Synchronet MQTT Broker "))
			return;
		if (m_logPanes.contains("broker"))
			return;
		auto *brokerLog = new LogWidget("Broker", "broker", {});
		int brokerLevel = m_settings.value("logLevel/broker", 6).toInt();
		brokerLog->setLevel(brokerLevel);
		m_mqtt->setLogLevel("broker", brokerLevel);
		connect(brokerLog, &LogWidget::levelChanged, this, [this](const QString &srv, int level) {
			m_mqtt->setLogLevel(srv, level);
			m_settings.setValue("logLevel/" + srv, level);
		});
		m_logPanes["broker"] = brokerLog;
		auto *brokerDock = makeDock("Broker", brokerLog);
		m_logDocks["broker"] = brokerDock;
		addDockWidget(Qt::BottomDockWidgetArea, brokerDock);
		if (auto *firstLogDock = m_logDocks.value(Servers.first()))
			tabifyDockWidget(firstLogDock, brokerDock);
		brokerDock->toggleViewAction()->setShortcut(QKeySequence());
		applyLogMaxLines();
		setDarkMode(m_dark);
	});
	connect(m_mqtt, &MqttClient::brokerLog, this, [this](int level, const QString &ts, const QString &text) {
		if (auto *pane = m_logPanes.value("broker"))
			pane->appendLog(level, ts, text);
		if (auto *bbs = m_logPanes.value("bbs"))
			bbs->appendLog(level, ts, "[Broker] " + text);
	});
	connect(m_mqtt, &MqttClient::connected, this, [this] {
		statusBar()->showMessage("Connected to MQTT broker", 3000);
		m_mqttLabel->setText("MQTT: Connected");
		m_connectBtn->setText("Disconnect");
	});
	connect(m_mqtt, &MqttClient::disconnected, this, [this] {
		m_mqttLabel->setText("MQTT: Disconnected");
		m_connectBtn->setText("Connect");
	});
	connect(m_mqtt, &MqttClient::errorOccurred, this, [this](const QString &msg) {
		statusBar()->showMessage(msg, 5000);
		m_mqttLabel->setText("MQTT: Error");
		if (auto *events = m_logPanes.value("events"))
			events->appendLog(3, {}, msg);
	});
	connect(m_mqtt, &MqttClient::sslErrorsOccurred, this,
		[this](const QList<QSslError> &errors, const QStringList &descriptions) {
		auto answer = QMessageBox::warning(this, "TLS Certificate Error",
			descriptions.join("\n") + "\n\nContinue?",
			QMessageBox::Yes | QMessageBox::No);
		if (answer == QMessageBox::Yes)
			m_mqtt->ignoreSslErrors(errors);
		else
			m_mqtt->disconnectFromBroker();
	});
	connect(m_mqtt, &MqttClient::logMessage, this, [this](const QString &host, const QString &server, int level, const QString &ts, const QString &text) {
		QString prefix = (multiHost() && !host.isEmpty()) ? "[" + host + "] " : "";
		if (auto *pane = m_logPanes.value(server))
			pane->appendLog(level, ts, prefix + text, host);
		if (auto *bbs = m_logPanes.value("bbs"))
			bbs->appendLog(level, ts, prefix + "[" + ServerLabels.value(server, server) + "] " + text, host);
	});
	connect(m_mqtt, &MqttClient::eventLogMessage, this, [this](const QString &host, int level, const QString &ts, const QString &text) {
		QString prefix = (multiHost() && !host.isEmpty()) ? "[" + host + "] " : "";
		if (auto *events = m_logPanes.value("events"))
			events->appendLog(level, ts, prefix + text, host);
		if (auto *bbs = m_logPanes.value("bbs"))
			bbs->appendLog(level, ts, prefix + "[Events] " + text, host);
	});
	connect(m_mqtt, &MqttClient::nodeStatus, this, [this](const QString &host, int nodeNum, const QVariantMap &fields) {
		if (!hostMatches(host)) return;
		m_nodeWidget->updateNode(nodeNum, fields);
	});
	connect(m_mqtt, &MqttClient::nodeVerbose, this, [this](const QString &host, int nodeNum, const QString &desc) {
		if (!hostMatches(host)) return;
		m_nodeWidget->updateNodeVerbose(nodeNum, desc);
	});
	connect(m_mqtt, &MqttClient::clientUpdate, this, [this](const QString &host, const QString &server, const QString &action, const QVariantMap &fields) {
		if (!hostMatches(host)) return;
		m_clientWidget->updateClient(server, action, fields);
	});
	connect(m_mqtt, &MqttClient::loginAttempt, this, [this](const QString &host, const QString &ip, const QString &action, const QVariantMap &fields) {
		if (!hostMatches(host)) return;
		m_loginAttemptsWidget->updateAttempt(ip, action, fields);
	});
	connect(m_mqtt, &MqttClient::maxConcurrent, this, [this](const QString &host, const QString &server, const QString &ip, const QString &action, int strikes) {
		if (!hostMatches(host)) return;
		m_maxConcurrentWidget->updateEntry(server, ip, action, strikes);
	});
	connect(m_mqtt, &MqttClient::bbsAction, m_actionWidget, &ActionWidget::addAction);
	connect(m_mqtt, &MqttClient::bbsAction, this, [this](const QString &action, const QString &detail,
	        const QString &timestamp, const QString &payload) {
		if (action != "page")
			return;
		if (!m_settings.value("notify/page_alert", true).toBool())
			return;
		QDateTime dt = QDateTime::fromString(timestamp, Qt::ISODate);
		if (!dt.isValid())
			dt = QDateTime::fromString(timestamp.left(15), "yyyyMMdd'T'HHmmss");
		if (dt.isValid() && dt.secsTo(QDateTime::currentDateTime()) > 60)
			return;
		QStringList fields = payload.split('\t');
		QString user = fields.size() >= 2 ? fields[1] : "Unknown";
		QApplication::alert(this);
		QApplication::beep();
		statusBar()->showMessage("Sysop page from " + user + " (node " + detail + ")", 15000);
	});
	auto updateServerLabel = [this](const QString &server) {
		if (auto *lbl = m_serverStateLabels.value(server)) {
			QString label = ServerLabels.value(server, server);
			QString ver = m_serverVersions.value(server);
			QString state = m_serverStates.value(server, "--");
			if (!ver.isEmpty())
				lbl->setText(label + " " + ver + ": " + state);
			else
				lbl->setText(label + ": " + state);
		}
	};
	connect(m_mqtt, &MqttClient::serverState, this, [this, updateServerLabel](const QString &host, const QString &server, const QString &state) {
		if (!hostMatches(host)) return;
		m_serverStates[server] = state;
		updateServerLabel(server);
	});
	connect(m_mqtt, &MqttClient::serverVersion, this, [this, updateServerLabel](const QString &host, const QString &server, const QString &version) {
		if (!hostMatches(host)) return;
		static QRegularExpression verRe("(\\d+\\.\\d+\\w*)");
		QString short_ver;
		auto match = verRe.match(version);
		if (match.hasMatch())
			short_ver = match.captured(1);
		if (version.contains("Debug", Qt::CaseInsensitive))
			short_ver += "-Dbg";
		m_serverVersions[server] = short_ver;
		updateServerLabel(server);
	});
	connect(m_mqtt, &MqttClient::serverStat, this, [this](const QString &host, const QString &server, const QString &stat, const QString &value) {
		if (!hostMatches(host)) return;
		bool ok;
		int v = value.toInt(&ok);
		if (!ok) return;
		m_statPerServer[server][stat] = v;
		int clients = 0, served = 0, errors = 0;
		for (auto &s : m_statPerServer) {
			clients += s.value("clients", 0);
			served += s.value("served", 0);
			errors += s.value("error_count", 0);
		}
		m_clientsLabel->setText(QStringLiteral("Clients: %1").arg(clients));
		m_servedLabel->setText(QStringLiteral("Served: %1").arg(served));
		m_errorsLabel->setText(QStringLiteral("Errors: %1").arg(errors));
	});
	connect(m_nodeWidget, &NodeWidget::nodeAction, this, [this](int n, const QString &action) {
		static const QHash<QString, QString> propMap = {
			{"lock", "lock"}, {"down", "down"}, {"interrupt", "interrupt"},
			{"rerun", "rerun"}, {"clear_errors", "errors"},
		};
		QString prop = propMap.value(action, action);
		m_mqtt->setNode(n, prop, action == "clear_errors" ? "0" : "1");
	});
	connect(m_nodeWidget, &NodeWidget::nodeStatus, this, [this](int n, int status) {
		m_mqtt->setNode(n, "status", QString::number(status));
	});
	connect(m_nodeWidget, &NodeWidget::nodeMessage, this, [this](int n) {
		auto text = QInputDialog::getText(this, "Send Message", QStringLiteral("Message to node %1:").arg(n));
		if (!text.isEmpty())
			m_mqtt->sendNodeMessage(n, text);
	});
	connect(m_loginAttemptsWidget, &LoginAttemptsWidget::clearAttempt, this, [this](const QString &ip) {
		forEachHost([this, ip](const QString &h) { m_mqtt->clearLoginAttempt(h, ip); });
	});
	connect(m_loginAttemptsWidget, &LoginAttemptsWidget::clearAllAttempts, this, [this](const QString &server) {
		forEachHost([this, server](const QString &h) { m_mqtt->clearServer(h, server); });
	});
	connect(m_loginAttemptsWidget, &LoginAttemptsWidget::countChanged, this, [this](int count) {
		m_failedLabel->setText(QStringLiteral("Failed: %1").arg(count));
	});
}

void MainWindow::showDock(QDockWidget *dock)
{
	if (!dock->isVisible())
		dock->setVisible(true);
	dock->raise();
	QString title = dock->windowTitle();
	for (auto *tabBar : findChildren<QTabBar *>())
		for (int i = 0; i < tabBar->count(); ++i)
			if (tabBar->tabText(i) == title) {
				tabBar->setCurrentIndex(i);
				return;
			}
}

void MainWindow::applyGlobalStyle()
{
	setDarkMode(m_settings.value("ui/dark_mode", true).toBool());
}

QString MainWindow::selectedHost() const
{
	if (m_hostCombo->currentIndex() == 0)
		return {};
	return m_hostCombo->currentText();
}

bool MainWindow::hostMatches(const QString &host) const
{
	if (host.isEmpty())
		return true;
	QString sel = selectedHost();
	return sel.isEmpty() || sel == host;
}

bool MainWindow::multiHost() const
{
	return m_hostCombo->count() > 2;
}

void MainWindow::forEachHost(std::function<void(const QString &)> fn)
{
	QString sel = selectedHost();
	if (sel.isEmpty()) {
		for (int i = 1; i < m_hostCombo->count(); ++i)
			fn(m_hostCombo->itemText(i));
	} else {
		fn(sel);
	}
}

void MainWindow::applyLogMaxLines()
{
	int max = m_settings.value("log/max_lines", DefaultMaxLogLines).toInt();
	for (auto *pane : m_logPanes)
		pane->setMaxLines(max);
}

void MainWindow::setDarkMode(bool dark)
{
	m_dark = dark;
	setStyleSheet(dark ? DarkStyle : QString());
	m_nodeWidget->setDark(dark);
	m_statsWidget->setDark(dark);
	m_clientWidget->setDark(dark);
	m_loginAttemptsWidget->setDark(dark);
	m_maxConcurrentWidget->setDark(dark);
	m_actionWidget->setDark(dark);
	for (auto *pane : m_logPanes)
		pane->setDark(dark);
	m_darkAction->setChecked(dark);
	m_settings.setValue("ui/dark_mode", dark);
}

void MainWindow::restoreState()
{
	auto geom = m_settings.value("window/geometry").toByteArray();
	if (!geom.isEmpty()) restoreGeometry(geom);
	auto state = m_settings.value("window/state").toByteArray();
	if (!state.isEmpty()) QMainWindow::restoreState(state);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
	m_settings.setValue("window/geometry", saveGeometry());
	m_settings.setValue("window/state", saveState());
	m_mqtt->disconnectFromBroker();
	QMainWindow::closeEvent(event);
}
