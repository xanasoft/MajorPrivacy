#include "pch.h"
#include "MajorPrivacy.h"
#include "../Core/PrivacyCore.h"
#include "../Core/Processes/ProcessList.h"
#include "../../MiscHelpers/Common/SortFilterProxyModel.h"
#include "AccessPage.h"
#include "../Views/AccessTraceView.h"
#include "../Views/AccessRuleView.h"
#include "../Views/AccessView.h"
#include "../Views/AccessInfoView.h"
#include "../Views/HandleView.h"


CAccessPage::CAccessPage(QWidget* parent)
	: QWidget(parent)
{
	m_pMainLayout = new QVBoxLayout(this);
	m_pMainLayout->setContentsMargins(0, 0, 0, 0);

	//m_pToolBar = new QToolBar();
	//m_pMainLayout->addWidget(m_pToolBar);
	
	m_pVSplitter = new QSplitter(Qt::Vertical);
	m_pMainLayout->addWidget(m_pVSplitter);

	//m_pRuleTabs = new QTabWidget();
	//m_pVSplitter->addWidget(m_pRuleTabs);

	m_pRuleView = new CAccessRuleView();
	//m_pRuleTabs->addTab(m_pAccessRuleView, tr("Access Rules"));
	m_pVSplitter->addWidget(m_pRuleView);

	m_pTabs = new QTabWidget();
	m_pVSplitter->addWidget(m_pTabs);

	m_pHandleView = new CHandleView();
	m_pTabs->addTab(m_pHandleView, tr("Open Resources"));

	m_pAccessSplitter = new QSplitter(Qt::Horizontal);
	m_pTabs->addTab(m_pAccessSplitter, tr("Access Monitor"));

	m_pAccessView = new CAccessView();
	m_pAccessSplitter->addWidget(m_pAccessView);

	m_pAccessInfoView = new CAccessInfoView();
	m_pAccessSplitter->addWidget(m_pAccessInfoView);

	connect(m_pAccessView, &CAccessView::SelectionChanged, m_pAccessInfoView, &CAccessInfoView::OnSelectionChanged);

	m_pTraceView = new CAccessTraceView();
	m_pTabs->addTab(m_pTraceView, tr("Trace Log"));

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
		MergeTraceLogs(&m_Log, ETraceLogs::eResLog, Current.Programs, Current.ServicesEx);
		m_pTraceView->Sync(&m_Log);
	}
}