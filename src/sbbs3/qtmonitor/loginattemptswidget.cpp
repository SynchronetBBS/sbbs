#include "loginattemptswidget.h"
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

LoginAttemptsWidget::LoginAttemptsWidget(bool dark, QWidget *parent)
	: QWidget(parent), m_dark(dark)
{
	auto *layout = new QVBoxLayout(this);
	layout->setContentsMargins(2, 2, 2, 2);

	m_tree = new QTreeWidget;
	m_tree->setHeaderLabels({"IP Address", "First Attempt", "Last Attempt",
	                          "Count", "Dupes", "Protocol", "Username"});
	m_tree->setColumnCount(7);
	m_tree->setRootIsDecorated(false);
	m_tree->setAlternatingRowColors(true);
	m_tree->setSortingEnabled(true);
	setDark(m_dark);
	layout->addWidget(m_tree);
}

void LoginAttemptsWidget::updateAttempt(const QString &ip, const QString &action, const QVariantMap &data)
{
	if (action == "clear") {
		auto it = m_attempts.find(ip);
		if (it != m_attempts.end()) {
			delete m_tree->takeTopLevelItem(m_tree->indexOfTopLevelItem(*it));
			m_attempts.erase(it);
		}
		emit countChanged(m_attempts.size());
		return;
	}

	QTreeWidgetItem *item;
	auto it = m_attempts.find(ip);
	if (it == m_attempts.end()) {
		item = new SortableItem;
		m_tree->addTopLevelItem(item);
		m_attempts[ip] = item;
	} else {
		item = *it;
	}

	item->setText(0, ip);
	for (auto &[col, key] : std::initializer_list<std::pair<int, const char *>>{{1, "first"}, {2, "last"}}) {
		QString ts = data.value(key).toString();
		QDateTime dt = QDateTime::fromString(ts, Qt::ISODate);
		if (!dt.isValid())
			dt = QDateTime::fromString(ts.left(15), "yyyyMMdd'T'HHmmss");
		item->setText(col, dt.isValid() ? dt.toString("MMM dd hh:mm:ss") : ts);
		item->setData(col, Qt::UserRole, dt.isValid() ? dt : QDateTime());
	}
	item->setText(5, data.value("protocol").toString());
	item->setText(6, data.value("username").toString());

	int count = data.value("count", "0").toString().toInt();
	item->setData(3, Qt::DisplayRole, count);
	item->setData(4, Qt::DisplayRole, data.value("dupes", "0").toString().toInt());

	QColor color;
	if (count >= 10)
		color = m_dark ? QColor("#FF4444") : QColor("red");
	else if (count >= 5)
		color = m_dark ? QColor("#FF00FF") : QColor("magenta");
	if (color.isValid())
		for (int i = 0; i < 7; ++i)
			item->setForeground(i, color);

	emit countChanged(m_attempts.size());
}

void LoginAttemptsWidget::setDark(bool dark)
{
	m_dark = dark;
	m_tree->setStyleSheet(dark
		? "QTreeWidget { background-color: #1e1e1e; color: #cccccc; alternate-background-color: #252525; }"
		: QString());
}
