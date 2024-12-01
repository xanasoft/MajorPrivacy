#include "pch.h"
#include "MajorPrivacy.h"
#include "../Core/PrivacyCore.h"
#include "../Core/Processes/ProcessList.h"
#include "../Core/Programs/ProgramManager.h"
#include "../../MiscHelpers/Common/SortFilterProxyModel.h"
#include "AccessPage.h"
#include "../Views/AccessTraceView.h"
#include "../Views/AccessRuleView.h"
#include "../Views/AccessView.h"
#include "../Views/AccessListView.h"
#include "../Views/HandleView.h"


CAccessPage::CAccessPage(QWidget* parent)
	: QWidget(parent)
{
	m_pMainLayout = new QVBoxLayout(this);
	m_pMainLayout->setContentsMargins(0, 0, 0, 0);

	//m_pToolBar = new QToolBar();
	//m_pMainLayout->addWidget(m_pToolBar);

	//m_pRuleTabs = new QTabWidget();

	m_pRuleView = new CAccessRuleView();
	//m_pRuleTabs->addTab(m_pAccessRuleView, tr("Access Rules"));


	m_pTabs = new QTabWidget();

	m_pHandleView = new CHandleView();
	m_pTabs->addTab(m_pHandleView, tr("Open Resources"));

	m_pAccessSplitter = new QSplitter(Qt::Horizontal);
	m_pTabs->addTab(m_pAccessSplitter, tr("Access Monitor"));

	m_pAccessView = new CAccessView();
	m_pAccessSplitter->addWidget(m_pAccessView);

	m_pAccessListView = new CAccessListView();
	m_pAccessSplitter->addWidget(m_pAccessListView);

	connect(m_pAccessView, &CAccessView::SelectionChanged, m_pAccessListView, &CAccessListView::OnSelectionChanged);

	m_pTraceView = new CAccessTraceView();
	m_pTabs->addTab(m_pTraceView, tr("Trace Log"));


	m_pVSplitter = new QSplitter(Qt::Vertical);
	m_pMainLayout->addWidget(m_pVSplitter);
	//m_pVSplitter->addWidget(m_pRuleTabs);
	m_pVSplitter->addWidget(m_pRuleView);
	m_pVSplitter->addWidget(m_pTabs);
}

CAccessPage::~CAccessPage()
{
	QString Name = this->objectName();
	if(Name.isEmpty())
		Name = "Access";
	theConf->SetValue(QString("MainWindow/" + Name + "Tab").toStdString().c_str(), m_pTabs->currentIndex());
}

void CAccessPage::LoadState()
{
	QString Name = this->objectName();
	if(Name.isEmpty())
		Name = "Access";
	m_pTabs->setCurrentIndex(theConf->GetInt(QString("MainWindow/" + Name + "Tab").toStdString().c_str(), 0));
}

void CAccessPage::SetMergePanels(bool bMerge)
{
	if (!m_pVSplitter == bMerge)
		return;

	if (bMerge)
	{
		m_pMainLayout->addWidget(m_pTabs);
		m_pTabs->insertTab(0, m_pRuleView, tr("Access Rules"));
		delete m_pVSplitter;
		m_pVSplitter = nullptr;
	}
	else
	{
		m_pVSplitter = new QSplitter(Qt::Vertical);
		m_pVSplitter->addWidget(m_pRuleView);
		m_pRuleView->setVisible(true);
		m_pVSplitter->addWidget(m_pTabs);
		m_pMainLayout->addWidget(m_pVSplitter);
	}

	LoadState();
}

void CAccessPage::Update()
{
	if (!isVisible())
		return;

	auto Current = theGUI->GetCurrentItems();

	if (m_pRuleView->isVisible())
	{
		if(Current.bAllPrograms)
			m_pRuleView->Sync(theCore->AccessManager()->GetAccessRules());
		else {
			QSet<QString> RuleIDs;
			foreach(CProgramItemPtr pItem, Current.Items)
				RuleIDs.unite(pItem->GetResRules());
			m_pRuleView->Sync(theCore->AccessManager()->GetAccessRules(RuleIDs));
		}
	}

	if(m_pHandleView->isVisible())
	{
		m_pHandleView->Sync(theGUI->GetCurrentProcesses());
	}

	if(m_pAccessView->isVisible())
	{
		m_pAccessView->Sync(Current.Programs, Current.ServicesEx | Current.ServicesIm);
	}

	if (m_pTraceView->isVisible())
	{
		m_pTraceView->Sync(ETraceLogs::eResLog, Current.Programs, Current.ServicesEx);
	}
}

void CAccessPage::Update(const QString& VolumeRoot, const QString& VolumeImage)
{
	if (!isVisible())
		return;

	auto Current = theGUI->GetCurrentItems();

	if (m_pRuleView->isVisible())
	{
		m_pRuleView->Sync(theCore->AccessManager()->GetAccessRules(), VolumeRoot, VolumeImage);
	}

	if(m_pHandleView->isVisible())
	{
		m_pHandleView->Sync(theCore->ProcessList()->List(), VolumeRoot);
	}

	if(m_pAccessView->isVisible())
	{
		m_pAccessView->Sync(theCore->ProgramManager()->GetPrograms(), theCore->ProgramManager()->GetServices(), VolumeRoot);
	}

	if (m_pTraceView->isVisible())
	{
		m_pTraceView->Sync(ETraceLogs::eResLog, theCore->ProgramManager()->GetPrograms(), theCore->ProgramManager()->GetServices(), VolumeRoot);
	}
}