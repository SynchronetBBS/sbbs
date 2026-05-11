#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QDockWidget>
#include <QLabel>
#include <QComboBox>
#include <QSettings>
#include "mqttclient.h"
#include "logwidget.h"
#include "nodewidget.h"
#include "clientwidget.h"
#include "statswidget.h"
#include "loginattemptswidget.h"
#include "actionwidget.h"
#include "maxconcurrentwidget.h"

class MainWindow : public QMainWindow
{
	Q_OBJECT

public:
	MainWindow(const QString &mqttHost, quint16 mqttPort,
	           const QString &bbsId,
	           const QString &mqttUser, const QString &mqttPass,
	           const QString &pskId, const QString &pskKey,
	           const QString &caFile = {}, const QString &certFile = {},
	           const QString &keyFile = {},
	           QWidget *parent = nullptr);

protected:
	void closeEvent(QCloseEvent *event) override;

private:
	void setupCentral();
	void setupDocks();
	void setupMenus();
	void setupToolbar();
	void setupStatusbar();
	void connectMqttSignals();
	void restoreState();
	void applyGlobalStyle();
	void applyLogMaxLines();
	QString selectedHost() const;
	bool hostMatches(const QString &host) const;
	bool multiHost() const;
	void forEachHost(std::function<void(const QString &)> fn);
	void setDarkMode(bool dark);

	QDockWidget *makeDock(const QString &title, QWidget *widget);
	void showDock(QDockWidget *dock);

	QSettings m_settings;
	MqttClient *m_mqtt;
	bool m_dark = true;

	NodeWidget *m_nodeWidget;
	StatsWidget *m_statsWidget;
	ClientWidget *m_clientWidget;
	LoginAttemptsWidget *m_loginAttemptsWidget;
	ActionWidget *m_actionWidget;
	MaxConcurrentWidget *m_maxConcurrentWidget;
	QHash<QString, LogWidget *> m_logPanes;
	QHash<QString, QDockWidget *> m_logDocks;

	QDockWidget *m_nodeDock;
	QDockWidget *m_statsDock;
	QDockWidget *m_clientDock;
	QDockWidget *m_loginAttemptsDock;
	QDockWidget *m_actionDock;
	QDockWidget *m_maxConcurrentDock;

	QHash<QString, QLabel *> m_serverStateLabels;
	QHash<QString, QString> m_serverVersions;
	QHash<QString, QString> m_serverStates;
	QLabel *m_bbsNameLabel;
	QLabel *m_clientsLabel;
	QLabel *m_servedLabel;
	QLabel *m_failedLabel;
	QLabel *m_errorsLabel;
	QLabel *m_mqttLabel;
	QHash<QString, QHash<QString, int>> m_statPerServer;
	QAction *m_darkAction;
	QComboBox *m_hostCombo;
	QAction *m_connectBtn;
};

#endif
