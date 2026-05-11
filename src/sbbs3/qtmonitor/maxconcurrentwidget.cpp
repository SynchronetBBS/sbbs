#include "maxconcurrentwidget.h"
#include <QVBoxLayout>

MaxConcurrentWidget::MaxConcurrentWidget(bool dark, QWidget *parent)
	: QWidget(parent), m_dark(dark)
{
	auto *layout = new QVBoxLayout(this);
	layout->setContentsMargins(2, 2, 2, 2);

	m_tree = new QTreeWidget;
	m_tree->setHeaderLabels({"IP Address", "Server", "Strikes"});
	m_tree->setColumnCount(3);
	m_tree->setRootIsDecorated(false);
	m_tree->setAlternatingRowColors(true);
	m_tree->setSortingEnabled(true);
	m_tree->setSelectionMode(QAbstractItemView::ExtendedSelection);
	setDark(m_dark);
	layout->addWidget(m_tree);
}

void MaxConcurrentWidget::updateEntry(const QString &server, const QString &ip,
                                       const QString &action, int strikes)
{
	QString key = server + "/" + ip;

	if (action == "clear") {
		auto it = m_entries.find(key);
		if (it != m_entries.end()) {
			delete m_tree->takeTopLevelItem(m_tree->indexOfTopLevelItem(*it));
			m_entries.erase(it);
		}
		emit countChanged(m_entries.size());
		return;
	}

	QTreeWidgetItem *item;
	auto it = m_entries.find(key);
	if (it == m_entries.end()) {
		item = new QTreeWidgetItem;
		m_tree->addTopLevelItem(item);
		m_entries[key] = item;
	} else {
		item = *it;
	}

	item->setText(0, ip);
	item->setText(1, server);
	item->setData(2, Qt::DisplayRole, strikes);

	QColor color;
	if (strikes >= 10)
		color = m_dark ? QColor("#FF4444") : QColor("red");
	else if (strikes >= 5)
		color = m_dark ? QColor("#FF00FF") : QColor("magenta");
	if (color.isValid())
		for (int i = 0; i < 3; ++i)
			item->setForeground(i, color);

	emit countChanged(m_entries.size());
}

void MaxConcurrentWidget::setDark(bool dark)
{
	m_dark = dark;
	m_tree->setStyleSheet(dark
		? "QTreeWidget { background-color: #1e1e1e; color: #cccccc; alternate-background-color: #252525; }"
		: QString());
}
