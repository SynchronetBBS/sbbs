#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QDockWidget>
#include <QLabel>
#include <QSettings>
#include "mqttclient.h"
#include "logwidget.h"
#include "nodewidget.h"
#include "clientwidget.h"
#include "statswidget.h"
#include "loginattemptswidget.h"

class MainWindow : public QMainWindow
{
	Q_OBJECT

public:
	MainWindow(const QString &mqttHost, quint16 mqttPort,
	           const QString &bbsId,
	           const QString &mqttUser, const QString &mqttPass,
	           const QString &pskId, const QString &pskKey,
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
	QHash<QString, LogWidget *> m_logPanes;
	QHash<QString, QDockWidget *> m_logDocks;

	QDockWidget *m_nodeDock;
	QDockWidget *m_statsDock;
	QDockWidget *m_clientDock;
	QDockWidget *m_loginAttemptsDock;

	QHash<QString, QLabel *> m_serverStateLabels;
	QLabel *m_clientsLabel;
	QLabel *m_servedLabel;
	QLabel *m_failedLabel;
	QLabel *m_errorsLabel;
	QLabel *m_mqttLabel;
	QHash<QString, QHash<QString, int>> m_statPerServer;
	QAction *m_darkAction;
	QAction *m_connectBtn;
};

#endif
