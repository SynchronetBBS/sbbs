#include "nodewidget.h"
#include "textutil.h"
#include <QVBoxLayout>
#include <QToolBar>
#include <QMenu>

static const QHash<QString, QString> StatusNames = {
	{"0", "WFC"}, {"1", "Logon"}, {"2", "New User"}, {"3", "In Use"},
	{"4", "Quiet"}, {"5", "Offline"}, {"6", "Netting"}, {"7", "Event Wait"},
	{"8", "Event Run"}, {"9", "Event Limbo"}, {"10", "Logout"},
};

static const QHash<int, QString> ConnectionNames = {
	{65535, "Telnet"}, {65534, "RLogin"}, {65533, "SSH"},
	{65532, "Raw"}, {65531, "SFTP"}, {0, "Local"},
};

static const QStringList ActionNames = {
	"Main", "Reading Msgs", "Reading Mail", "Sending Mail", "Reading G-Files",
	"Reading Sent Mail", "Posting Msg", "Auto-Msg", "External", "Defaults",
	"Xfer Prompt", "Downloading", "Uploading", "Bidi Xfer", "Listing Files",
	"Logging On", "Local Chat", "Multi-Chat", "Guru Chat", "Chat Section",
	"Sysop", "QWK Xfer", "Private Chat", "Paging", "Seq Dev", "Custom",
};

NodeWidget::NodeWidget(bool dark, QWidget *parent)
	: QWidget(parent), m_dark(dark)
{
	auto *layout = new QVBoxLayout(this);
	layout->setContentsMargins(2, 2, 2, 2);

	auto *toolbar = new QToolBar;
	for (auto &[name, id] : std::initializer_list<std::pair<const char *, const char *>>{
		{"Lock", "lock"}, {"Down", "down"}, {"Interrupt", "interrupt"},
		{"Rerun", "rerun"}, {"Clear Errors", "clear_errors"},
	}) {
		auto *act = toolbar->addAction(name);
		connect(act, &QAction::triggered, this, [this, s = QString(id)] { emitAction(s); });
	}
	toolbar->addSeparator();
	auto *msgAct = toolbar->addAction("Send Message");
	connect(msgAct, &QAction::triggered, this, [this] {
		for (auto *item : m_tree->selectedItems())
			emit nodeMessage(item->data(0, Qt::DisplayRole).toInt());
	});
	layout->addWidget(toolbar);

	m_tree = new QTreeWidget;
	m_tree->setHeaderLabels({"Node", "Status", "User", "Connection", "Action", "Errors", "Description"});
	m_tree->setColumnCount(7);
	m_tree->setRootIsDecorated(false);
	m_tree->setAlternatingRowColors(true);
	m_tree->setSortingEnabled(true);
	m_tree->sortByColumn(0, Qt::AscendingOrder);
	m_tree->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(m_tree, &QTreeWidget::customContextMenuRequested, this, [this](const QPoint &pos) {
		QMenu menu(this);
		for (auto &[name, id] : std::initializer_list<std::pair<const char *, const char *>>{
			{"Lock", "lock"}, {"Down", "down"}, {"Interrupt", "interrupt"},
			{"Rerun", "rerun"}, {"Clear Errors", "clear_errors"},
		}) {
			auto *act = menu.addAction(name);
			connect(act, &QAction::triggered, this, [this, s = QString(id)] { emitAction(s); });
		}
		menu.addSeparator();
		auto *msgAct = menu.addAction("Send Message");
		connect(msgAct, &QAction::triggered, this, [this] {
			for (auto *item : m_tree->selectedItems())
				emit nodeMessage(item->data(0, Qt::DisplayRole).toInt());
		});
		menu.exec(m_tree->viewport()->mapToGlobal(pos));
	});
	setDark(m_dark);
	layout->addWidget(m_tree);
}

void NodeWidget::emitAction(const QString &actionId)
{
	for (auto *item : m_tree->selectedItems())
		emit nodeAction(item->data(0, Qt::DisplayRole).toInt(), actionId);
}

QTreeWidgetItem *NodeWidget::ensureItem(int nodeNum)
{
	auto it = m_nodes.find(nodeNum);
	if (it != m_nodes.end())
		return *it;
	auto *item = new QTreeWidgetItem;
	item->setData(0, Qt::DisplayRole, nodeNum);
	m_tree->addTopLevelItem(item);
	m_nodes[nodeNum] = item;
	return item;
}

void NodeWidget::updateNode(int nodeNum, const QVariantMap &data)
{
	auto *item = ensureItem(nodeNum);
	QString statusCode = data.value("status").toString();
	QString statusText = statusName(statusCode);
	item->setText(1, statusText);

	int userNum = data.value("user", "0").toString().toInt();
	if (userNum)
		item->setData(2, Qt::DisplayRole, userNum);
	else
		item->setData(2, Qt::DisplayRole, QVariant());

	int connVal = data.value("connection", "0").toString().toInt();
	item->setText(3, connectionName(connVal));

	int actionVal = data.value("action", "0").toString().toInt();
	item->setText(4, actionName(actionVal));
	item->setText(5, data.value("errors", "0").toString());

	int ncols = m_tree->columnCount();
	if (statusText == "In Use" || statusText == "Quiet" || statusText == "Logon" || statusText == "New User") {
		QColor c = m_dark ? QColor("#44FF44") : QColor("darkGreen");
		for (int i = 0; i < ncols; ++i) item->setForeground(i, c);
	} else if (statusText == "WFC") {
		QColor c = m_dark ? QColor("#888888") : QColor("gray");
		for (int i = 0; i < ncols; ++i) item->setForeground(i, c);
	}
	QString errors = data.value("errors", "0").toString();
	if (errors != "0")
		item->setForeground(5, m_dark ? QColor("#FF4444") : QColor("red"));
}

void NodeWidget::updateNodeVerbose(int nodeNum, const QString &description)
{
	setItemText(ensureItem(nodeNum), 6, description);
}

QString NodeWidget::statusName(const QString &code) const
{
	return StatusNames.value(code, code);
}

QString NodeWidget::connectionName(int val) const
{
	return ConnectionNames.value(val, QString::number(val));
}

QString NodeWidget::actionName(int val) const
{
	return (val >= 0 && val < ActionNames.size()) ? ActionNames[val] : QString::number(val);
}

void NodeWidget::setDark(bool dark)
{
	m_dark = dark;
	m_tree->setStyleSheet(dark
		? "QTreeWidget { background-color: #1e1e1e; color: #cccccc; alternate-background-color: #252525; }"
		: QString());
}
