#include "clientwidget.h"
#include <QVBoxLayout>
#include <QDateTime>

class SortableItem : public QTreeWidgetItem
{
public:
	bool operator<(const QTreeWidgetItem &other) const override {
		int col = treeWidget()->sortColumn();
		QVariant a = data(col, Qt::UserRole);
		QVariant b = other.data(col, Qt::UserRole);
		if (a.typeId() == QMetaType::QDateTime && b.typeId() == QMetaType::QDateTime)
			return a.toDateTime() < b.toDateTime();
		return QTreeWidgetItem::operator<(other);
	}
};

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
		item = new SortableItem;
		m_tree->addTopLevelItem(item);
		m_clients[key] = item;
	} else {
		item = *it;
	}

	item->setData(0, Qt::DisplayRole, sock.toInt());
	item->setText(1, data.value("protocol", server).toString());
	item->setText(2, data.value("username").toString());
	item->setText(3, data.value("ip").toString());
	item->setText(4, data.value("hostname").toString());
	item->setData(5, Qt::DisplayRole, data.value("port").toString().toInt());

	QString ts = data.value("timestamp").toString();
	QDateTime dt = QDateTime::fromString(ts, Qt::ISODate);
	if (!dt.isValid())
		dt = QDateTime::fromString(ts.left(15), "yyyyMMdd'T'HHmmss");
	item->setText(6, dt.isValid() ? dt.toString("MMM dd hh:mm:ss") : ts);
	item->setData(6, Qt::UserRole, dt.isValid() ? dt : QDateTime());
}

void ClientWidget::setDark(bool dark)
{
	m_dark = dark;
	m_tree->setStyleSheet(dark
		? "QTreeWidget { background-color: #1e1e1e; color: #cccccc; alternate-background-color: #252525; }"
		: QString());
}
