#include "clientwidget.h"
#include <QVBoxLayout>
#include <QDateTime>

ClientWidget::ClientWidget(bool dark, QWidget *parent)
	: QWidget(parent), m_dark(dark)
{
	auto *layout = new QVBoxLayout(this);
	layout->setContentsMargins(2, 2, 2, 2);

	m_tree = new QTreeWidget;
	m_tree->setHeaderLabels({"Socket", "Protocol", "User", "Address", "Hostname", "Port", "Time"});
	m_tree->setColumnCount(7);
	m_tree->setRootIsDecorated(false);
	m_tree->setAlternatingRowColors(true);
	m_tree->setSortingEnabled(true);
	setDark(m_dark);
	layout->addWidget(m_tree);
}

void ClientWidget::updateClient(const QString &server, const QString &action, const QVariantMap &data)
{
	QString sock = data.value("socket").toString();
	QString key = server + ":" + sock;

	if (action == "disconnect") {
		auto it = m_clients.find(key);
		if (it != m_clients.end()) {
			delete m_tree->takeTopLevelItem(m_tree->indexOfTopLevelItem(*it));
			m_clients.erase(it);
		}
		return;
	}

	QTreeWidgetItem *item;
	auto it = m_clients.find(key);
	if (it == m_clients.end()) {
		item = new QTreeWidgetItem;
		m_tree->addTopLevelItem(item);
		m_clients[key] = item;
	} else {
		item = *it;
	}

	item->setText(0, sock);
	item->setText(1, data.value("protocol", server).toString());
	item->setText(2, data.value("username").toString());
	item->setText(3, data.value("ip").toString());
	item->setText(4, data.value("hostname").toString());
	item->setText(5, data.value("port").toString());

	QString ts = data.value("timestamp").toString();
	QDateTime dt = QDateTime::fromString(ts, Qt::ISODate);
	if (!dt.isValid())
		dt = QDateTime::fromString(ts.left(15), "yyyyMMdd'T'HHmmss");
	item->setText(6, dt.isValid() ? dt.toString("MMM dd hh:mm:ss") : ts);
}

void ClientWidget::setDark(bool dark)
{
	m_dark = dark;
	m_tree->setStyleSheet(dark
		? "QTreeWidget { background-color: #1e1e1e; color: #cccccc; alternate-background-color: #252525; }"
		: QString());
}
