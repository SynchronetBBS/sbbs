#include "mainwindow.h"
#include "settingsdialog.h"

#include <QAction>
#include <QApplication>
#include <QMenu>
#include <QMenuBar>
#include <QSettings>
#include <QStandardPaths>
#include <QTabBar>
#include <QTest>
#include <QTemporaryDir>
#include <QToolBar>

class DockBehaviorTest : public QObject
{
	Q_OBJECT

private:
	static void settle()
	{
		QCoreApplication::processEvents();
		QTest::qWait(1);
		QCoreApplication::processEvents();
	}

	static QAction *findAction(QObject *parent, const QString &text)
	{
		for (auto *action : parent->findChildren<QAction *>())
			if (action->text() == text)
				return action;
		return nullptr;
	}

	static bool isCurrentDockTab(MainWindow &window, const QString &title)
	{
		for (auto *tabBar : window.findChildren<QTabBar *>())
			if (tabBar->currentIndex() >= 0 && tabBar->tabText(tabBar->currentIndex()) == title)
				return true;
		return false;
	}

private slots:
	void tlsPortDefaultsTo8883()
	{
		QSettings settings("Synchronet", "qtmonitor");
		settings.clear();
		SettingsDialog dialog(&settings);
		auto *port = dialog.findChild<QSpinBox *>("mqtt_port");
		QVERIFY(port);
		QCOMPARE(port->value(), 8883);

		settings.setValue("mqtt/port", 0);
		SettingsDialog zeroPortDialog(&settings);
		port = zeroPortDialog.findChild<QSpinBox *>("mqtt_port");
		QVERIFY(port);
		QCOMPARE(port->value(), 8883);
	}

	void brokerRestoresSavedPlacement()
	{
		QSettings("Synchronet", "qtmonitor").clear();
		{
			MainWindow window({}, 0, {}, {}, {}, {}, {});
			window.show();
			settle();
			auto *mqtt = window.findChild<MqttClient *>();
			QVERIFY(QMetaObject::invokeMethod(mqtt, "brokerVersion", Qt::DirectConnection,
			                                  Q_ARG(QString, "Synchronet MQTT Broker test")));
			settle();
			auto *brokerDock = window.findChild<QDockWidget *>("dock_Broker");
			QVERIFY(brokerDock);
			window.addDockWidget(Qt::TopDockWidgetArea, brokerDock);
			brokerDock->close();
			settle();
			window.close();
		}

		MainWindow restored({}, 0, {}, {}, {}, {}, {});
		restored.show();
		settle();
		auto *mqtt = restored.findChild<MqttClient *>();
		QVERIFY(QMetaObject::invokeMethod(mqtt, "brokerVersion", Qt::DirectConnection,
		                                  Q_ARG(QString, "Synchronet MQTT Broker test")));
		settle();
		auto *brokerDock = restored.findChild<QDockWidget *>("dock_Broker");
		QVERIFY(brokerDock);
		QCOMPARE(restored.dockWidgetArea(brokerDock), Qt::TopDockWidgetArea);
		QVERIFY(!brokerDock->toggleViewAction()->isChecked());
	}

	void resetRestoresStartupState()
	{
		QSettings("Synchronet", "qtmonitor").clear();
		MainWindow window({}, 0, {}, {}, {}, {}, {});
		window.show();
		settle();

		QHash<QString, QRect> startupGeometry;
		QHash<QString, Qt::DockWidgetArea> startupAreas;
		QHash<QString, QStringList> startupTabs;
		const auto docks = window.findChildren<QDockWidget *>(QString(), Qt::FindDirectChildrenOnly);
		for (auto *dock : docks) {
			const QString name = dock->objectName();
			if (dock->geometry().x() >= 0 && dock->geometry().y() >= 0)
				startupGeometry[name] = dock->geometry();
			startupAreas[name] = window.dockWidgetArea(dock);
			QStringList tabNames;
			for (auto *tab : window.tabifiedDockWidgets(dock))
				tabNames.append(tab->objectName());
			tabNames.sort();
			startupTabs[name] = tabNames;
		}

		auto *nodesDock = window.findChild<QDockWidget *>("dock_Nodes");
		auto *terminalDock = window.findChild<QDockWidget *>("dock_Terminal");
		QVERIFY(nodesDock);
		QVERIFY(terminalDock);
		nodesDock->setFloating(true);
		terminalDock->close();
		settle();
		QVERIFY(nodesDock->isFloating());
		QVERIFY(!terminalDock->toggleViewAction()->isChecked());

		auto *resetAction = findAction(&window, "&Reset Layout");
		QVERIFY(resetAction);
		resetAction->trigger();
		settle();
		for (auto *dock : docks) {
			const QString name = dock->objectName();
			QCOMPARE(window.dockWidgetArea(dock), startupAreas.value(name));
			if (startupGeometry.contains(name))
				QCOMPARE(dock->geometry(), startupGeometry.value(name));
			QVERIFY(!dock->isFloating());
			QVERIFY(dock->toggleViewAction()->isChecked());
			QStringList tabNames;
			for (auto *tab : window.tabifiedDockWidgets(dock))
				tabNames.append(tab->objectName());
			tabNames.sort();
			QCOMPARE(tabNames, startupTabs.value(name));
		}
	}

	void closeReopenAndReset()
	{
		QSettings("Synchronet", "qtmonitor").clear();
		MainWindow window({}, 0, {}, {}, {}, {}, {});
		window.show();
		settle();

		auto *terminalDock = window.findChild<QDockWidget *>("dock_Terminal");
		auto *terminalButton = window.findChild<QAction *>("toolbar_log_term");
		QVERIFY(terminalDock);
		QVERIFY(terminalButton);

		// The View action is the authoritative open/closed state.
		QAction *terminalView = terminalDock->toggleViewAction();
		QVERIFY(terminalView->isChecked());
		terminalDock->close();
		settle();
		QVERIFY(!terminalView->isChecked());

		// A server toolbar button always opens and selects its dock.
		terminalButton->trigger();
		settle();
		QVERIFY(terminalView->isChecked());
		QVERIFY(isCurrentDockTab(window, "Terminal"));

		// The View checkbox follows the same path in both directions.
		terminalView->trigger();
		settle();
		QVERIFY(!terminalView->isChecked());
		terminalView->trigger();
		settle();
		QVERIFY(terminalView->isChecked());
		QVERIFY(isCurrentDockTab(window, "Terminal"));

		// Broker is dynamic, but must receive the same View and toolbar controls.
		auto *mqtt = window.findChild<MqttClient *>();
		QVERIFY(mqtt);
		QVERIFY(QMetaObject::invokeMethod(mqtt, "brokerVersion", Qt::DirectConnection,
		                                  Q_ARG(QString, "Synchronet MQTT Broker test")));
		settle();
		auto *brokerDock = window.findChild<QDockWidget *>("dock_Broker");
		auto *brokerButton = window.findChild<QAction *>("toolbar_log_broker");
		QVERIFY(brokerDock);
		QVERIFY(brokerButton);
		QVERIFY(brokerDock->toggleViewAction()->isChecked());
		QMenu *viewMenu = nullptr;
		for (auto *menuAction : window.menuBar()->actions())
			if (menuAction->text() == "&View")
				viewMenu = menuAction->menu();
		QVERIFY(viewMenu);
		QVERIFY(viewMenu->actions().contains(brokerDock->toggleViewAction()));

		brokerDock->close();
		settle();
		QVERIFY(!brokerDock->toggleViewAction()->isChecked());
		brokerButton->trigger();
		settle();
		QVERIFY(brokerDock->toggleViewAction()->isChecked());
		QVERIFY(isCurrentDockTab(window, "Broker"));

		// Reset is a canonical rebuild: it restores every dock, including Broker.
		terminalDock->close();
		brokerDock->setFloating(true);
		brokerDock->close();
		settle();
		auto *resetAction = findAction(&window, "&Reset Layout");
		QVERIFY(resetAction);
		resetAction->trigger();
		settle();
		QVERIFY(terminalView->isChecked());
		QVERIFY(brokerDock->toggleViewAction()->isChecked());
		QVERIFY(!brokerDock->isFloating());
		QCOMPARE(window.dockWidgetArea(terminalDock), Qt::BottomDockWidgetArea);
		QVERIFY(window.tabifiedDockWidgets(terminalDock).contains(brokerDock));

		// A reset requested during a tab press waits for mouse release.  This is
		// the sequence that previously left Qt delivering tabMoved to an obsolete
		// dock layout and crashed in QDockAreaLayoutInfo::moveTab().
		terminalDock->close();
		settle();
		QTabBar *pressedTabBar = nullptr;
		for (auto *tabBar : window.findChildren<QTabBar *>())
			if (tabBar->count() > 1) {
				pressedTabBar = tabBar;
				break;
			}
		QVERIFY(pressedTabBar);
		const QPoint tabCenter = pressedTabBar->tabRect(pressedTabBar->currentIndex()).center();
		QTest::mousePress(pressedTabBar, Qt::LeftButton, Qt::NoModifier, tabCenter);
		resetAction->trigger();
		settle();
		QVERIFY(!terminalView->isChecked());
		const QPoint dragPoint = tabCenter + QPoint(pressedTabBar->tabRect(0).width(), 0);
		QTest::mouseMove(pressedTabBar, dragPoint, 1);
		QTest::mouseRelease(pressedTabBar, Qt::LeftButton, Qt::NoModifier, dragPoint);
		QTRY_VERIFY_WITH_TIMEOUT(terminalView->isChecked(), 1000);
	}
};

int main(int argc, char **argv)
{
	QStandardPaths::setTestModeEnabled(true);
	QTemporaryDir settingsDir;
	if (!settingsDir.isValid())
		return 1;
	QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, settingsDir.path());
	QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsDir.path());
	QApplication app(argc, argv);
	DockBehaviorTest test;
	return QTest::qExec(&test, argc, argv);
}

#include "dock_behavior_test.moc"
