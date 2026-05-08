#include "actionwidget.h"
#include "textutil.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDateTime>

static const int ActionTypeRole = Qt::UserRole + 1;

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

static QString formatAction(const QString &action, const QString &detail)
{
	if (!detail.isEmpty())
		return action + " (" + detail + ")";
	return action;
}

static void parsePayload(const QString &action, const QStringList &fields,
                         QString &user, QString &details)
{
	if (action == "login" || action == "login_fail") {
		if (action == "login" && fields.size() >= 2) {
			user = fields[1];
			if (fields.size() >= 3)
				details = fields[2];
			if (fields.size() >= 4 && fields[3] != fields[2])
				details += " (" + fields[3] + ")";
		} else if (action == "login_fail") {
			user = fields.value(0);
			if (fields.size() >= 2)
				details = fields[1];
			if (fields.size() >= 3 && fields[2] != fields[1])
				details += " (" + fields[2] + ")";
		}
	} else if (action == "logout") {
		if (fields.size() >= 2)
			user = fields[1];
		QStringList parts;
		if (fields.size() >= 3)
			parts << fields[2];
		if (fields.size() >= 5)
			parts << fields[4];
		details = parts.join(", ");
	} else if (action == "newuser") {
		if (fields.size() >= 2)
			user = fields[1];
	} else if (action == "post") {
		if (fields.size() >= 2)
			user = fields[1];
		QStringList parts;
		if (fields.size() >= 6)
			parts << fields[5];
		if (fields.size() >= 5 && !fields[4].isEmpty())
			parts << "to " + fields[4];
		details = parts.join(" ");
	} else if (action == "exec") {
		if (fields.size() >= 2)
			user = fields[1];
	} else if (action == "upload" || action == "download") {
		if (fields.size() >= 2)
			user = fields[1];
		QStringList parts;
		if (fields.size() >= 3)
			parts << fields[2];
		if (fields.size() >= 4) {
			qint64 bytes = fields[3].toLongLong();
			if (bytes >= 1048576)
				parts << QString::number(bytes / 1048576) + " MB";
			else if (bytes >= 1024)
				parts << QString::number(bytes / 1024) + " KB";
			else
				parts << fields[3] + " B";
		}
		if (fields.size() >= 5)
			parts << fields[4];
		details = parts.join(", ");
	} else if (action == "page") {
		if (fields.size() >= 2)
			user = fields[1];
	} else if (action == "hack") {
		user = fields.value(0);
		QStringList parts;
		if (fields.size() >= 4)
			parts << fields[3];
		if (fields.size() >= 3 && fields[2] != fields.value(3))
			parts << "(" + fields[2] + ")";
		if (fields.size() >= 5 && !fields[4].isEmpty())
			parts << fields[4];
		details = parts.join(" ");
	} else if (action == "spam") {
		QStringList parts;
		if (fields.size() >= 3)
			parts << fields[2];
		if (fields.size() >= 4)
			parts << "from " + fields[3];
		if (fields.size() >= 5 && !fields[4].isEmpty())
			parts << fields[4];
		if (fields.size() >= 6 && !fields[5].isEmpty())
			parts << fields[5];
		details = parts.join(", ");
	} else if (action == "error") {
		details = fields.join(" ");
	} else {
		details = fields.join(", ");
	}
}

ActionWidget::ActionWidget(bool dark, QWidget *parent)
	: QWidget(parent), m_dark(dark)
{
	auto *layout = new QVBoxLayout(this);
	layout->setContentsMargins(2, 2, 2, 2);

	auto *toolbar = new QHBoxLayout;
	m_showBtn = new QToolButton;
	m_showBtn->setText("Show");
	m_showBtn->setPopupMode(QToolButton::InstantPopup);
	m_showMenu = new QMenu(this);
	for (const auto &name : {"login", "logout", "login_fail", "newuser",
	                          "post", "exec", "upload", "download",
	                          "page", "hack", "spam", "error"}) {
		auto *act = m_showMenu->addAction(name);
		act->setCheckable(true);
		act->setChecked(true);
		QString key = name;
		connect(act, &QAction::toggled, this, [this, key](bool on) {
			if (on)
				m_hiddenActions.remove(key);
			else
				m_hiddenActions.insert(key);
			applyFilters();
		});
		m_filterActions[key] = act;
	}
	m_showBtn->setMenu(m_showMenu);
	toolbar->addWidget(m_showBtn);
	toolbar->addStretch();
	layout->addLayout(toolbar);

	m_tree = new QTreeWidget;
	m_tree->setHeaderLabels({"Time", "Action", "User", "Details"});
	m_tree->setColumnCount(4);
	m_tree->setRootIsDecorated(false);
	m_tree->setAlternatingRowColors(true);
	m_tree->setSortingEnabled(true);
	m_tree->sortByColumn(0, Qt::DescendingOrder);
	setDark(m_dark);
	layout->addWidget(m_tree);
}

void ActionWidget::addAction(const QString &action, const QString &detail,
                             const QString &timestamp, const QString &payload)
{
	if (!m_filterActions.contains(action)) {
		m_showMenu->addSeparator();
		auto *act = m_showMenu->addAction(action);
		act->setCheckable(true);
		act->setChecked(true);
		connect(act, &QAction::toggled, this, [this, action](bool on) {
			if (on)
				m_hiddenActions.remove(action);
			else
				m_hiddenActions.insert(action);
			applyFilters();
		});
		m_filterActions[action] = act;
	}

	auto *item = new SortableItem;

	QDateTime dt = QDateTime::fromString(timestamp, Qt::ISODate);
	if (!dt.isValid())
		dt = QDateTime::fromString(timestamp.left(15), "yyyyMMdd'T'HHmmss");
	item->setText(0, dt.isValid() ? dt.toString("MMM dd hh:mm:ss") : timestamp);
	item->setData(0, Qt::UserRole, dt.isValid() ? dt : QDateTime());

	item->setText(1, formatAction(action, detail));
	item->setData(1, ActionTypeRole, action);

	QStringList fields = payload.split('\t');
	QString user, details;
	parsePayload(action, fields, user, details);
	setItemText(item, 2, user);
	setItemText(item, 3, details);

	item->setHidden(m_hiddenActions.contains(action));

	m_tree->insertTopLevelItem(0, item);
	while (m_tree->topLevelItemCount() > MaxRows)
		delete m_tree->takeTopLevelItem(m_tree->topLevelItemCount() - 1);
}

void ActionWidget::applyFilters()
{
	for (int i = 0; i < m_tree->topLevelItemCount(); ++i) {
		auto *item = m_tree->topLevelItem(i);
		QString type = item->data(1, ActionTypeRole).toString();
		item->setHidden(m_hiddenActions.contains(type));
	}
}

void ActionWidget::setDark(bool dark)
{
	m_dark = dark;
	m_tree->setStyleSheet(dark
		? "QTreeWidget { background-color: #1e1e1e; color: #cccccc; alternate-background-color: #252525; }"
		: QString());
}
