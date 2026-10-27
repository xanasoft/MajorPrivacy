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
#include "../Views/UsageView.h"


CAccessPage::CAccessPage(bool bEmbedded, QWidget* parent)
	: QWidget(parent)
{
	m_bEmbedded = bEmbedded;

	m_pMainLayout = new QVBoxLayout(this);
	m_pMainLayout->setContentsMargins(0, 0, 0, 0);

	//m_pToolBar = new QToolBar();
	//m_pMainLayout->addWidget(m_pToolBar);


	m_pRuleTabs = new QTabWidget();

	m_pRuleView = new CAccessRuleView();
	m_pRuleTabs->addTab(m_pRuleView,  QIcon(":/Icons/Rules.png"), tr("Access Rules"));


	m_pTabs = new QTabWidget();

	m_pHandleView = new CHandleView();
	m_pTabs->addTab(m_pHandleView, QIcon(":/Icons/Files.png"), tr("Open Resources"));

	m_pAccessSplitter = new QSplitter(Qt::Horizontal);
	m_pTabs->addTab(m_pAccessSplitter, QIcon(":/Icons/Ampel.png"), tr("Access Monitor"));

	m_pAccessView = new CAccessView();
	m_pAccessSplitter->addWidget(m_pAccessView);
	m_pAccessSplitter->setCollapsible(0, false);

	m_pAccessListView = new CAccessListView();
	m_pAccessSplitter->addWidget(m_pAccessListView);

	connect(m_pAccessView, &CAccessView::SelectionChanged, this, [&] {
		if(m_pAccessSplitter->sizes().at(1) > 10)
			m_pAccessListView->OnSelectionChanged(m_pAccessView->GetSelectedItemsWithChildren());
	});

	m_pTraceView = new CAccessTraceView();
	m_pTabs->addTab(m_pTraceView, QIcon(":/Icons/SetLogging.png"), tr("Trace Log"));

	if (!bEmbedded) {
		m_pIoView = new CUsageView(EUsageMetric::eIo);
		m_pTabs->addTab(m_pIoView, QIcon(":/Icons/Disk.png"), tr("I/O Usage"));
	}


	m_pVSplitter = new QSplitter(Qt::Vertical);
	m_pMainLayout->addWidget(m_pVSplitter);
	m_pVSplitter->addWidget(m_pRuleTabs);
	m_pVSplitter->setCollapsible(0, false);
	m_pVSplitter->addWidget(m_pTabs);
	m_pVSplitter->setCollapsible(1, false);
}

CAccessPage::~CAccessPage()
{
	theConf->SetValue("MainWindow/" + QString(m_bEmbedded ? "VolumeAccess" : "AccessPanel"), m_pTabs->currentIndex());
}

void CAccessPage::LoadState()
{
	m_pTabs->setCurrentIndex(theConf->GetInt(QString("MainWindow/" + QString(m_bEmbedded ? "AccessPanel" : "AccessTab")).toStdString().c_str(), 0));
}

void CAccessPage::SetMergePanels(bool bMerge)
{
	if (!m_pVSplitter == bMerge)
		return;

	if (bMerge)
	{
		m_pMainLayout->addWidget(m_pTabs);
		m_pTabs->insertTab(0, m_pRuleView, QIcon(":/Icons/Rules.png"), tr("Access Rules"));
		delete m_pVSplitter;
		m_pVSplitter = nullptr;
	}
	else
	{
		m_pVSplitter = new QSplitter(Qt::Vertical);
		m_pRuleTabs = new QTabWidget();
		m_pRuleTabs->addTab(m_pRuleView,  QIcon(":/Icons/Rules.png"), tr("Access Rules"));
		m_pVSplitter->addWidget(m_pRuleTabs);
		m_pRuleView->setVisible(true);
		m_pVSplitter->setCollapsible(0, false);
		m_pVSplitter->addWidget(m_pTabs);
		m_pVSplitter->setCollapsible(1, false);
		m_pMainLayout->addWidget(m_pVSplitter);
	}

	LoadState();
}

void CAccessPage::Update()
{
	if (!isVisible() || m_bEmbedded)
		return;
	UpdateEnabled();

	auto Current = theGUI->GetCurrentItems();

	//
	// Explicitly selected items are always shown. The implicit members of a group or of
	// "All Programs" with a private trace preset only when the view's own "show private"
	// button, or the global one, is on. The usage graphs get everything, see CUsageView.
	//
	bool bShowPrivate = theGUI->GetShowPrivate();
	auto Programs = [&](bool bPrivate) {
		QSet<CProgramFilePtr> Set = Current.ProgramsEx;
		for (auto& pProgram : Current.ProgramsIm) {
			if (bShowPrivate || bPrivate || pProgram->GetResTrace() != ETracePreset::ePrivate)
				Set.insert(pProgram);
		}
		return Set;
	};
	auto Services = [&](bool bPrivate) {
		QSet<CWindowsServicePtr> Set = Current.ServicesEx;
		for (auto& pService : Current.ServicesIm) {
			if (bShowPrivate || bPrivate || pService->GetResTrace() != ETracePreset::ePrivate)
				Set.insert(pService);
		}
		return Set;
	};

	if (m_pRuleView->isVisible())
	{
		if (Current.bAllProgramsOnly) {
			//
			// The all programs item stands for the rules that apply to every program,
			// not for the union of what the individual programs have.
			//
			QSet<QFlexGuid> RuleIDs;
			CProgramSetPtr pAll = theCore->ProgramManager()->GetAll();
			if (pAll) RuleIDs = pAll->GetResRules();
			m_pRuleView->Sync(theCore->AccessManager()->GetAccessRules(RuleIDs));
		}
		else if(Current.bAllPrograms)
			m_pRuleView->Sync(theCore->AccessManager()->GetAccessRules());
		else {
			QSet<QFlexGuid> RuleIDs;
			foreach(CProgramItemPtr pItem, Current.Items)
				RuleIDs.unite(pItem->GetResRules());
			m_pRuleView->Sync(theCore->AccessManager()->GetAccessRules(RuleIDs));
		}
	}

	if (m_pHandleView->isVisible())
	{
		m_pHandleView->Sync(theGUI->GetCurrentProcesses());
	}

	if (m_pAccessView->isVisible())
	{
		bool bPrivate = m_pAccessView->ShowPrivate();
		m_pAccessView->Sync(Programs(bPrivate), Services(bPrivate));
	}

	if (m_pTraceView->isVisible())
	{
		m_pTraceView->Sync(ETraceLogs::eResLog, Programs(m_pTraceView->ShowPrivate()), Current.ServicesEx);
	}

	if (m_pIoView && m_pIoView->isVisible())
	{
		m_pIoView->Sync(Current.ProgramsEx | Current.ProgramsIm, Current.ServicesEx | Current.ServicesIm);
	}
}

void CAccessPage::Update(const QString& VolumeRoot, const QString& VolumeImage, const QString& MountPoint)
{
	if (!isVisible() || !m_bEmbedded)
		return;
	UpdateEnabled();

	auto Current = theGUI->GetCurrentItems();

	if (m_pRuleView->isVisible())
	{
		m_pRuleView->Sync(theCore->AccessManager()->GetAccessRules(), VolumeRoot, VolumeImage, MountPoint);
	}

	if (m_pHandleView->isVisible())
	{
		m_pHandleView->Sync(theCore->ProcessList()->List(), VolumeRoot);
	}

	if (m_pAccessView->isVisible())
	{
		m_pAccessView->Sync(theCore->ProgramManager()->GetPrograms(), theCore->ProgramManager()->GetServices(), VolumeRoot);
	}

	if (m_pTraceView->isVisible())
	{
		m_pTraceView->Sync(ETraceLogs::eResLog, theCore->ProgramManager()->GetPrograms(), theCore->ProgramManager()->GetServices(), VolumeRoot);
	}
}

void CAccessPage::Clear()
{
	if (m_pIoView) m_pIoView->Clear();

	m_pRuleView->Clear();

	m_pHandleView->Clear();

	m_pAccessView->Clear();

	m_pTraceView->Clear();

	m_pAccessListView->Clear();
}

void CAccessPage::UpdateEnabled()
{
	m_pHandleView->setEnabled(theCore->GetConfigBool("Service/EnumAllOpenFiles", false));
	m_pAccessView->setEnabled(theCore->GetConfigBool("Service/ResTrace", true));
	m_pTraceView->setEnabled(theCore->GetConfigBool("Service/ResLog", false));
	if (m_pIoView) m_pIoView->setEnabled(theCore->GetConfigBool("Service/UsageStats", true));
}