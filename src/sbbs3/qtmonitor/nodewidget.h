#ifndef NODEWIDGET_H
#define NODEWIDGET_H

#include <QWidget>
#include <QTreeWidget>
#include <QVariantMap>

class NodeWidget : public QWidget
{
	Q_OBJECT

public:
	explicit NodeWidget(bool dark = true, QWidget *parent = nullptr);
	void setDark(bool dark);

public slots:
	void updateNode(int nodeNum, const QVariantMap &data);
	void updateNodeVerbose(int nodeNum, const QString &description);

signals:
	void nodeAction(int nodeNum, const QString &action);
	void nodeStatus(int nodeNum, int status);
	void nodeMessage(int nodeNum);

private:
	void emitAction(const QString &actionId);
	QTreeWidgetItem *ensureItem(int nodeNum);
	QString statusName(const QString &code) const;
	QString connectionName(int val) const;
	QString actionName(int val) const;

	QTreeWidget *m_tree;
	QHash<int, QTreeWidgetItem *> m_nodes;
	bool m_dark;
};

#endif
