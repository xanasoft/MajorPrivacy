#include "pch.h"
#include "TweakView.h"
#include "../Core/PrivacyCore.h"
#include "../Core/Tweaks/TweakManager.h"
#include "MajorPrivacy.h"
#include "../MiscHelpers/Common/CheckableMessageBox.h"


CTweakView::CTweakView(QWidget *parent)
	:CPanelViewEx<CTweakModel>(parent)
{
	m_pTreeView->setColumnReset(2);
	m_pTreeView->setUniformRowHeights(false);
	QStyle* pStyle = QStyleFactory::create("windows");
	m_pTreeView->setStyle(pStyle);
	m_pTreeView->setItemDelegate(new CTreeItemDelegate2());
	m_pTreeView->setIconSize(QSize(32, 32));
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

	m_pBtnRefresh = new QToolButton();
	m_pBtnRefresh->setIcon(QIcon(":/Icons/Recover.png"));
	m_pBtnRefresh->setToolTip(tr("Refresh tweak states"));
	m_pBtnRefresh->setFixedHeight(22);
	m_pBtnRefresh->setShortcut(QKeySequence::fromString("F5"));
	connect(m_pBtnRefresh, SIGNAL(clicked()), this, SLOT(OnRefresh()));
	m_pToolBar->addWidget(m_pBtnRefresh);

	m_pBtnHidden = new QToolButton();
	m_pBtnHidden->setIcon(QIcon(":/Icons/Disable.png"));
	m_pBtnHidden->setCheckable(true);
	m_pBtnHidden->setToolTip(tr("Show not available tweaks"));
	m_pBtnHidden->setMaximumHeight(22);
	m_pToolBar->addWidget(m_pBtnHidden);

	m_pToolBar->addSeparator();

	m_pBtnApprove = new QToolButton();
	m_pBtnApprove->setIcon(QIcon(":/Icons/Approve2.png"));
	m_pBtnApprove->setToolTip(tr("Approve Current State"));
	m_pBtnApprove->setMaximumHeight(22);
	connect(m_pBtnApprove, SIGNAL(clicked()), this, SLOT(OnApprove()));
	m_pToolBar->addWidget(m_pBtnApprove);

	m_pBtnRestore = new QToolButton();
	m_pBtnRestore->setIcon(QIcon(":/Icons/Refresh.png"));
	m_pBtnRestore->setToolTip(tr("Restore Approved State"));
	m_pBtnRestore->setMaximumHeight(22);
	connect(m_pBtnRestore, SIGNAL(clicked()), this, SLOT(OnRestore()));
	m_pToolBar->addWidget(m_pBtnRestore);


	m_pToolBar->addSeparator();
	m_pBtnExpand = new QToolButton();
	m_pBtnExpand->setIcon(QIcon(":/Icons/Expand.png"));
	m_pBtnExpand->setCheckable(true);
	m_pBtnExpand->setToolTip(tr("Auto Expand"));
	m_pBtnExpand->setMaximumHeight(22);
	connect(m_pBtnExpand, &QToolButton::toggled, this, [&](bool checked) {
		if(checked)
			m_pTreeView->expandAll();
		else
			m_pTreeView->collapseAll();
		});
	m_pToolBar->addWidget(m_pBtnExpand);

	QWidget* pSpacer = new QWidget();
	pSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	m_pToolBar->addWidget(pSpacer);

	QAbstractButton* pBtnSearch = m_pFinder->GetToggleButton();
	pBtnSearch->setIcon(QIcon(":/Icons/Search.png"));
	pBtnSearch->setMaximumHeight(22);
	m_pToolBar->addWidget(pBtnSearch);

	AddPanelItemsToMenu();

	connect(theCore->TweakManager(), SIGNAL(TweaksChanged()), this, SLOT(OnTweaksChanged()));
}

CTweakView::~CTweakView()
{
	theConf->SetBlob("MainWindow/TweakView_Columns", m_pTreeView->saveState());
}

void CTweakView::Sync(const CTweakPtr& pRoot)
{
	m_pRoot = pRoot;

	QList<QModelIndex> Added = m_pItemModel->Sync(pRoot, m_pBtnHidden->isChecked());

	QTimer::singleShot(10, this, [this, Added]() {
		foreach(const QModelIndex& Index, Added) {
			if(m_pItemModel->GetItem(Index)->GetType() == ETweakType::eGroup)
				m_pTreeView->expand(m_pSortProxy->mapFromSource(Index));
		}
	});
}

/*void CTweakView::OnDoubleClicked(const QModelIndex& Index)
{
}*/

void CTweakView::OnCheckChanged(const QModelIndex& Index, bool State)
{
	CTweakPtr pTweak = m_pItemModel->GetItem(Index);
	if(!pTweak)
		return;

	STATUS Status;
	if (State) 
	{
		if (pTweak->GetHint() == ETweakHint::eBreaking) {

			if (theConf->GetBool("Options/WarnBreakingTweaks", true)) {
				bool State = false;
				if (CCheckableMessageBox::question(this, "MajorPrivacy", tr("This tweak is marked as breaking, hence it may break something. Are you sure you want to apply it?"), 
				 tr("Don't show this message again."), &State, QDialogButtonBox::Ok | QDialogButtonBox::Cancel, QDialogButtonBox::Cancel, QMessageBox::Warning) != QDialogButtonBox::Ok)
					return;
				if (State)
					theConf->SetValue("Options/WarnBreakingTweaks", false);
			}
		}

		Status = theCore->TweakManager()->ApplyTweak(pTweak);
	}
	else
		Status = theCore->TweakManager()->UndoTweak(pTweak);

	if (Status) 
		pTweak->SetStatus(State ? ETweakStatus::eSet : ETweakStatus::eNotSet);
	theGUI->CheckResults(QList<STATUS>() << Status, this);
}

void CTweakView::OnMenu(const QPoint& Point)
{
	CPanelView::OnMenu(Point);
}

QList<CTweakPtr> FlattenTweaks(const QSharedPointer<CAbstractTweak>& pRoot, bool bOnlyTweaks = false)
{
	QList<CTweakPtr> List;
	QList<CTweakPtr> Queue;

	if (pRoot)
		Queue.append(pRoot);

	while (!Queue.isEmpty())
	{
		CTweakPtr current = Queue.takeFirst(); 

		auto tweakList = qSharedPointerDynamicCast<CTweakList>(current);
		if (tweakList)
		{
			QMap<QString, CTweakPtr> children = tweakList->GetList();
			for (auto it = children.begin(); it != children.end(); ++it)
			{
				if (it.value())
					Queue.append(it.value());
			}
		}

		if (!bOnlyTweaks || qSharedPointerDynamicCast<CTweak>(current))
			List.append(current);
	}

	return List;
}

void CTweakView::OnRefresh()
{
	theCore->TweakManager()->Update();
}

void CTweakView::OnTweaksChanged()
{
	if (!m_uRefreshPending)
	{
		m_uRefreshPending = GetCurTick();
		QTimer::singleShot(100, this, [this]() {
			m_uRefreshPending = 0;
			theCore->TweakManager()->Update();
		});
	}
}

void CTweakView::OnApprove()
{
	if (QMessageBox::question(this, tr("MajorPrivacy"), tr("Are you sure you want to approve the current state of all tweaks?"), QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
		return;

	QList<CTweakPtr> List = FlattenTweaks(m_pRoot, true);

	foreach(const auto& pTweak, List)
		theCore->ApproveTweak(pTweak->GetId());
}

void CTweakView::OnRestore()
{
	QList<CTweakPtr> TweaksToUndo;
	QList<CTweakPtr> TweaksToApply;

	QList<CTweakPtr> List = FlattenTweaks(m_pRoot, true);
	for (const auto& pTweak : List)
	{
		if (pTweak->GetStatus() == ETweakStatus::eApplied)
			TweaksToUndo.append(pTweak);
		else if (pTweak->GetStatus() == ETweakStatus::eMissing)
			TweaksToApply.append(pTweak);
	}

	QString Message = tr("Are you sure you want to restore the approved state of all tweaks?\n\nThe following operations will be performed:\n");
	if (!TweaksToUndo.isEmpty())
	{
		Message += tr("Undo tweaks:\n");
		for (const auto& pTweak : TweaksToUndo)
			Message += "  - " + pTweak->GetName() + "\n";
		Message += "\n";
	}
	if (!TweaksToApply.isEmpty())
	{
		Message += tr("Apply tweaks:\n");
		for (const auto& pTweak : TweaksToApply)
			Message += "  - " + pTweak->GetName() + "\n";
		Message += "\n";
	}

	if (QMessageBox::question(this, tr("MajorPrivacy"), Message, QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
		return;

	for (const auto& pTweak : TweaksToUndo)
		theCore->UndoTweak(pTweak->GetId());
	for (const auto& pTweak : TweaksToApply)
		theCore->ApplyTweak(pTweak->GetId());
}
