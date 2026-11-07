#pragma once
#include <QtWidgets>
namespace BusinessUi {
inline QString stylesheet() {
  return QStringLiteral(R"(
QMainWindow,QWidget#workspaceRoot { background:#f3f6fb; color:#253247; }
QWidget { font-family:"Microsoft YaHei"; font-size:10pt; color:#253247; }
QLabel { background:transparent; }
QFrame#workspaceNav { background:#14233e; border:0; }
QFrame#workspaceNav QLabel { color:#b4c2d9; }
QLabel#brand { color:white; font-size:20pt; font-weight:700; }
QLabel#brandSubtitle { color:#8da5c9; font-size:9pt; }
QListWidget#workspaceMenu { background:transparent; border:0; outline:0; color:#bdcbe1; padding:4px; }
QListWidget#workspaceMenu::item { padding:13px 12px; margin:4px 0; border-radius:7px; }
QListWidget#workspaceMenu::item:selected { background:#2d5eea; color:white; }
QListWidget#workspaceMenu::item:hover { background:#213a60; }
QLabel#pageTitle { font-size:19pt; font-weight:700; color:#14233e; }
QLabel#pageSubtitle { color:#7c8a9d; font-size:9pt; }
QLabel#scopeBadge { background:#e6f7f2; color:#13836d; padding:7px 12px; border-radius:12px; }
QTabWidget::pane { border:0; background:transparent; }
QFrame#metricCard { background:white; border:1px solid #e3e9f2; border-radius:9px; }
QLabel#metricLabel { color:#748399; font-size:9pt; }
QLabel#metricValue { color:#183453; font-size:22pt; font-weight:700; }
QGroupBox { background:white; border:1px solid #e0e7f1; border-radius:8px; margin-top:12px; padding:15px 10px 10px; font-weight:600; }
QGroupBox::title { subcontrol-origin:margin; left:15px; padding:0 5px; color:#425571; }
QGroupBox#authGroupBox { margin:0; padding:18px; }
QLabel#loginHeading { color:#14233e; font-size:18pt; font-weight:700; }
QLabel#loginHint { color:#586a84; padding-bottom:8px; }
QPushButton { background:white; border:1px solid #d4deeb; border-radius:5px; padding:7px 11px; min-height:15px; }
QPushButton:hover { border-color:#6a8eff; background:#f0f5ff; }
QPushButton:pressed { background:#e3ecff; }
QPushButton[primary="true"] { color:white; background:#2d5eea; border-color:#2d5eea; font-weight:600; }
QPushButton[primary="true"]:hover { background:#214dd0; }
QPushButton:disabled { color:#9ca8ba; background:#edf1f6; border-color:#e0e6ee; }
QLineEdit,QDateEdit,QComboBox,QSpinBox { background:white; border:1px solid #d8e1ee; border-radius:5px; padding:6px 8px; selection-background-color:#dce7ff; }
QLineEdit:focus,QDateEdit:focus,QComboBox:focus { border-color:#527bf1; }
QTextEdit,QPlainTextEdit { background:white; border:1px solid #d8e1ee; border-radius:5px; padding:6px; }
QTableView#marketingReportTable,QTableWidget,QTreeWidget { background:white; alternate-background-color:#f6f9fe; border:1px solid #e1e8f2; gridline-color:#edf1f7; selection-background-color:#e4edff; selection-color:#234a91; outline:0; }
QTableView#marketingReportTable QHeaderView::section,QTableWidget QHeaderView::section,QTreeWidget QHeaderView::section { background:#edf2fa; color:#5b6c84; padding:9px 8px; border:0; border-bottom:1px solid #dfe6f1; font-weight:600; }
QTableView#marketingReportTable::item { padding:6px; border:0; }
QCalendarWidget { background:white; border:1px solid #d8e1ee; }
QCalendarWidget QToolButton { color:#253247; background:transparent; border:0; padding:4px; }
QCalendarWidget QToolButton:hover { background:#e8efff; border-radius:4px; }
QCalendarWidget QAbstractItemView { background:white; color:#253247; selection-background-color:#2d5eea; selection-color:white; padding:0px; outline:0; }
QCalendarWidget QHeaderView::section { background:#f3f6fb; color:#61728c; padding:3px; border:0; font-weight:600; }
QScrollBar:vertical { background:#f1f4f9; width:10px; margin:0; }
QScrollBar::handle:vertical { background:#c5d0e0; min-height:25px; border-radius:5px; }
QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical { height:0; }
QSplitter::handle { background:#e4eaf3; width:5px; height:5px; }
QProgressBar { background:#e9eff9; border:0; border-radius:4px; text-align:center; max-height:16px; }
QProgressBar::chunk { background:#4977ee; border-radius:4px; }
QMenuBar,QStatusBar { background:#f3f6fb; color:#61728c; }
QMenu { background:white; border:1px solid #d8e1ee; padding:4px; }
QMenu::item:selected { background:#e8efff; }
QToolTip { color:#253247; background:white; border:1px solid #d5deeb; padding:5px; }
)");
}
inline QFrame *metric(const QString &caption, QLabel *&value,
                      QWidget *parent = nullptr) {
  auto *card = new QFrame(parent);
  card->setObjectName("metricCard");
  auto *box = new QVBoxLayout(card);
  box->setContentsMargins(16, 12, 16, 12);
  box->setSpacing(3);
  auto *label = new QLabel(caption);
  label->setObjectName("metricLabel");
  box->addWidget(label);
  value = new QLabel("—");
  value->setObjectName("metricValue");
  box->addWidget(value);
  return card;
}
inline QWidget *shell(QTabWidget *pages, const QString &brand,
                      const QString &subtitle, const QStringList &names) {
  auto *root = new QWidget;
  root->setObjectName("workspaceRoot");
  auto *row = new QHBoxLayout(root);
  row->setContentsMargins(0, 0, 0, 0);
  row->setSpacing(0);
  auto *nav = new QFrame;
  nav->setObjectName("workspaceNav");
  nav->setFixedWidth(190);
  auto *side = new QVBoxLayout(nav);
  side->setContentsMargins(18, 26, 18, 20);
  side->setSpacing(10);
  auto *logo = new QLabel(brand);
  logo->setObjectName("brand");
  side->addWidget(logo);
  auto *small = new QLabel(subtitle);
  small->setObjectName("brandSubtitle");
  small->setWordWrap(true);
  side->addWidget(small);
  side->addSpacing(24);
  auto *menu = new QListWidget;
  menu->setObjectName("workspaceMenu");
  for (int i = 0; i < names.size(); ++i)
    menu->addItem(
        QString("%1    %2").arg(i + 1, 2, 10, QChar('0')).arg(names[i]));
  side->addWidget(menu, 1);
  side->addWidget(new QLabel("WORKSPACE  /  工作空间"));
  row->addWidget(nav);
  auto *content = new QWidget;
  auto *body = new QVBoxLayout(content);
  body->setContentsMargins(18, 18, 18, 12);
  body->setSpacing(12);
  auto *header = new QHBoxLayout;
  auto *titles = new QVBoxLayout;
  auto *title = new QLabel(names.value(0));
  title->setObjectName("pageTitle");
  auto *desc = new QLabel(subtitle);
  desc->setObjectName("pageSubtitle");
  titles->addWidget(title);
  titles->addWidget(desc);
  header->addLayout(titles);
  header->addStretch();
  auto *scope = new QLabel("本地工作区");
  scope->setObjectName("scopeBadge");
  header->addWidget(scope);
  body->addLayout(header);
  pages->tabBar()->hide();
  body->addWidget(pages, 1);
  row->addWidget(content, 1);
  QObject::connect(menu, &QListWidget::currentRowChanged, pages,
                   &QTabWidget::setCurrentIndex);
  QObject::connect(pages, &QTabWidget::currentChanged, title,
                   [menu, title, names](int i) {
                     QSignalBlocker block(menu);
                     menu->setCurrentRow(i);
                     title->setText(names.value(i));
                   });
  menu->setCurrentRow(0);
  return root;
}
} // namespace BusinessUi
