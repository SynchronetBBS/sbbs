#ifndef MAXCONCURRENTWIDGET_H
#define MAXCONCURRENTWIDGET_H

#include <QWidget>
#include <QTreeWidget>
#include <QToolButton>
#include <QHash>

class MaxConcurrentWidget : public QWidget
{
	Q_OBJECT

public:
	explicit MaxConcurrentWidget(bool dark = true, QWidget *parent = nullptr);

	void updateEntry(const QString &server, const QString &ip,
	                 const QString &action, int strikes = 0);
	void setDark(bool dark);

signals:
	void countChanged(int count);

private:
	QTreeWidget *m_tree;
	QHash<QString, QTreeWidgetItem *> m_entries;
	bool m_dark;
};

#endif
