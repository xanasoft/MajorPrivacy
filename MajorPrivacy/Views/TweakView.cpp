#include "pch.h"
#include "TweakView.h"
#include "../Core/PrivacyCore.h"
#include "../Core/Tweaks/TweakManager.h"
#include "../MiscHelpers/Common/CustomStyles.h"
#include "MajorPrivacy.h"

CTweakView::CTweakView(QWidget *parent)
	:CPanelViewEx<CTweakModel>(parent)
{
	m_pTreeView->setColumnReset(2);
	QStyle* pStyle = QStyleFactory::create("windows");
	m_pTreeView->setStyle(pStyle);
	m_pTreeView->setItemDelegate(new CTreeItemDelegate());
	//connect(m_pTreeView, SIGNAL(ResetColumns()), this, SLOT(OnResetColumns()));
	//connect(m_pTreeView, SIGNAL(ColumnChanged(int, bool)), this, SLOT(OnColumnsChanged()));

	connect(m_pItemModel, SIGNAL(CheckChanged(const QModelIndex&, bool)), this, SLOT(OnCheckChanged(const QModelIndex&, bool)));

	QByteArray Columns = theConf->GetBlob("MainWindow/TweakView_Columns");
	if (Columns.isEmpty()) {
		m_pTreeView->setColumnWidth(0, 300);
	} else
		m_pTreeView->restoreState(Columns);

	m_pMainLayout->setSpacing(1);

	m_pToolBar = new QToolBar();
	m_pMainLayout->insertWidget(0, m_pToolBar);

	QWidget* pSpacer = new QWidget();
	pSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	m_pToolBar->addWidget(pSpacer);

	QAbstractButton* pBtnSearch = m_pFinder->GetToggleButton();
	pBtnSearch->setIcon(QIcon(":/Icons/Search.png"));
	pBtnSearch->setMaximumHeight(22);
	m_pToolBar->addWidget(pBtnSearch);

	AddPanelItemsToMenu();
}

CTweakView::~CTweakView()
{
	theConf->SetBlob("MainWindow/TweakView_Columns", m_pTreeView->saveState());
}

void CTweakView::Sync(const CTweakPtr& pRoot)
{
	QList<QModelIndex> Added = m_pItemModel->Sync(pRoot);

	QTimer::singleShot(10, this, [this, Added]() {
		foreach(const QModelIndex& Index, Added) {
			if(m_pItemModel->GetItem(Index)->GetType() == ETweakType::eGroup)
				m_pTreeView->expand(m_pSortProxy->mapFromSource(Index));
		}
	});
}

void CTweakView::OnDoubleClicked(const QModelIndex& Index)
{
}

void CTweakView::OnCheckChanged(const QModelIndex& Index, bool State)
{
	STATUS Status;
	CTweakPtr pTweak = m_pItemModel->GetItem(Index);
	if(State)
		Status = theCore->TweakManager()->ApplyTweak(pTweak);
	else
		Status = theCore->TweakManager()->UndoTweak(pTweak);
	theGUI->CheckResults(QList<STATUS>() << Status, this);
}

void CTweakView::OnMenu(const QPoint& Point)
{
	CPanelView::OnMenu(Point);
}
