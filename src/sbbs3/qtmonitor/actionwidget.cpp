#include "actionwidget.h"
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

ActionWidget::ActionWidget(bool dark, QWidget *parent)
	: QWidget(parent), m_dark(dark)
{
	auto *layout = new QVBoxLayout(this);
	layout->setContentsMargins(2, 2, 2, 2);

	m_tree = new QTreeWidget;
	m_tree->setHeaderLabels({"Time", "Action", "Detail", "Info"});
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
	auto *item = new SortableItem;

	QDateTime dt = QDateTime::fromString(timestamp, Qt::ISODate);
	if (!dt.isValid())
		dt = QDateTime::fromString(timestamp.left(15), "yyyyMMdd'T'HHmmss");
	item->setText(0, dt.isValid() ? dt.toString("MMM dd hh:mm:ss") : timestamp);
	item->setData(0, Qt::UserRole, dt.isValid() ? dt : QDateTime());

	item->setText(1, action);
	item->setText(2, detail);
	item->setText(3, payload);

	m_tree->insertTopLevelItem(0, item);
	while (m_tree->topLevelItemCount() > MaxRows)
		delete m_tree->takeTopLevelItem(m_tree->topLevelItemCount() - 1);
}

void ActionWidget::setDark(bool dark)
{
	m_dark = dark;
	m_tree->setStyleSheet(dark
		? "QTreeWidget { background-color: #1e1e1e; color: #cccccc; alternate-background-color: #252525; }"
		: QString());
}
