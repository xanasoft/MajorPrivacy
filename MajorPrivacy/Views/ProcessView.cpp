#include "pch.h"
#include "ProcessView.h"
#include "../Core/PrivacyCore.h"
#include "../MiscHelpers/Common/CustomStyles.h"

enum class EProcessScope
{
	eAll = 0,
	eUser,
	eSystem
};

CProcessView::CProcessView(QWidget *parent)
	:CPanelViewEx(parent)
{
	m_pTreeView->setColumnReset(2);
	QStyle* pStyle = QStyleFactory::create("windows");
	m_pTreeView->setStyle(pStyle);
	m_pTreeView->setItemDelegate(new CTreeItemDelegate());
	m_pTreeView->setAlternatingRowColors(theConf->GetBool("Options/AltRowColors", false));
	//connect(m_pTreeView, SIGNAL(ResetColumns()), this, SLOT(OnResetColumns()));
	//connect(m_pTreeView, SIGNAL(ColumnChanged(int, bool)), this, SLOT(OnColumnsChanged()));

	QByteArray Columns = theConf->GetBlob("MainWindow/ProcessView_Columns");
	if (Columns.isEmpty()) {
		m_pTreeView->setColumnWidth(0, 300);
	} else
		m_pTreeView->restoreState(Columns);

	m_pMainLayout->setSpacing(1);

	m_pToolBar = new QToolBar();
	m_pMainLayout->insertWidget(0, m_pToolBar);

	m_pCmbScope = new QComboBox();
	m_pCmbScope->addItem(QIcon(":/Icons/Process.png"), tr("All"), (qint32)EProcessScope::eAll);
	m_pCmbScope->addItem(QIcon(":/Icons/UserApp.png"), tr("User"), (qint32)EProcessScope::eUser);
	m_pCmbScope->addItem(QIcon(":/Icons/System.png"), tr("System"), (qint32)EProcessScope::eSystem);
	m_pToolBar->addWidget(m_pCmbScope);

	m_pToolBar->addSeparator();
	m_pBtnTree = new QToolButton();
	m_pBtnTree->setIcon(QIcon(":/Icons/Tree.png"));
	m_pBtnTree->setCheckable(true);
	m_pBtnTree->setToolTip(tr("Show Tree"));
	m_pBtnTree->setMaximumHeight(22);
	m_pItemModel->SetTree(theConf->GetBool("Options/UseProcessTree", false));
	m_pBtnTree->setChecked(m_pItemModel->IsTree());
	connect(m_pBtnTree, &QToolButton::toggled, this, [&](bool checked) {
		theConf->SetValue("Options/UseProcessTree", checked);
		m_pItemModel->SetTree(checked);
		if(checked)
			m_pTreeView->expandAll();
		});
	m_pToolBar->addWidget(m_pBtnTree);

	QWidget* pSpacer = new QWidget();
	pSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	m_pToolBar->addWidget(pSpacer);

	QAbstractButton* pBtnSearch = m_pFinder->GetToggleButton();
	pBtnSearch->setIcon(QIcon(":/Icons/Search.png"));
	pBtnSearch->setMaximumHeight(22);
	m_pToolBar->addWidget(pBtnSearch);

	AddPanelItemsToMenu();
}

CProcessView::~CProcessView()
{
	theConf->SetBlob("MainWindow/ProcessView_Columns", m_pTreeView->saveState());
}

void CProcessView::Sync(QHash<quint64, CProcessPtr> ProcessMap)
{
	EProcessScope Scope = (EProcessScope)m_pCmbScope->currentData().toInt();

	if (Scope != EProcessScope::eAll) 
	{
		for (auto I = ProcessMap.begin(); I != ProcessMap.end();)
		{
			QString UserSid = I.value()->GetUserSid();
			bool bIsSystem = UserSid == "S-1-5-18"	// SYSTEM
				|| UserSid == "S-1-5-19"	// NT-AUTORITAET\Lokaler Dienst
				|| UserSid == "S-1-5-20";	// NT-AUTORITAET\Netzwerkdienst

			if ((Scope == EProcessScope::eSystem) != bIsSystem)
				I = ProcessMap.erase(I);
			else
				++I;
		}
	}

	QSet<quint64> AddedProcs = m_pItemModel->Sync(ProcessMap);

	if (m_pItemModel->IsTree()) {
		QTimer::singleShot(10, this, [this, AddedProcs]() {
			foreach(const QVariant ID, AddedProcs) {
				QModelIndex ModelIndex = m_pItemModel->FindIndex(ID);	
				m_pTreeView->expand(m_pSortProxy->mapFromSource(ModelIndex));
			}
			});
	}
}

/*void CProcessView::OnDoubleClicked(const QModelIndex& Index)
{

}*/

void CProcessView::OnMenu(const QPoint& Point)
{
	CPanelView::OnMenu(Point);
}
