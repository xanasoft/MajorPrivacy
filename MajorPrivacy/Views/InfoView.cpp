#include "pch.h"
#include "InfoView.h"
#include "../Core/PrivacyCore.h"
#include "../Core/Programs/ProgramItem.h"
#include "../Core/Programs/ProgramManager.h"
#include "../../MiscHelpers/Common/Common.h"
#include <QFileIconProvider>
#include <QDateTime>

CInfoView::CInfoView(QWidget *parent)
	:QWidget(parent)
{
	m_pMainLayout = new QVBoxLayout(this);
	m_pMainLayout->setContentsMargins(0, 0, 0, 0);

	// Create stacked widget to switch between single and multiple item views
	m_pStackedWidget = new QWidget();
	m_pStackedLayout = new QStackedLayout();
	m_pStackedWidget->setLayout(m_pStackedLayout);
	m_pStackedWidget->setMaximumHeight(150);
	m_pMainLayout->addWidget(m_pStackedWidget);

	// Single item view
	m_pOneItemWidget = new QWidget();
	m_pOneItemLayout = new QVBoxLayout();
	m_pOneItemLayout->setContentsMargins(0, 0, 0, 0);
	m_pOneItemWidget->setLayout(m_pOneItemLayout);
	m_pStackedLayout->addWidget(m_pOneItemWidget);

	// Details
	m_pInfoBox = new QGroupBox(tr("Program Information"));
	m_pInfoLayout = new QGridLayout(m_pInfoBox);
	m_pInfoLayout->setContentsMargins(9, 9, 9, 9);
	m_pInfoLayout->setSpacing(4);
	m_pOneItemLayout->addWidget(m_pInfoBox);


	int row = 0;

	m_pIcon = new QLabel();
	m_pIcon->setPixmap(QPixmap(":/exe32.png"));
	m_pInfoLayout->addWidget(m_pIcon, 0, 0, 2, 1);

	m_pProgramName = new QLabel();
	m_pProgramName->setTextInteractionFlags(Qt::TextSelectableByMouse);
	m_pProgramName->setSizePolicy(QSizePolicy::Ignored, m_pProgramName->sizePolicy().verticalPolicy());
	m_pInfoLayout->addWidget(m_pProgramName, row++, 1, 1, 2);

	m_pCompanyName = new QLabel();
	m_pCompanyName->setTextInteractionFlags(Qt::TextSelectableByMouse);
	m_pInfoLayout->addWidget(m_pCompanyName, row++, 1, 1, 2);

	m_pType = new QLabel();
	m_pInfoLayout->addWidget(m_pType, row, 0, 1, 2);
	m_pInfo = new QLabel();
	m_pInfo->setAlignment(Qt::AlignRight);
	m_pInfoLayout->addWidget(m_pInfo, row++, 2, 1, 1);
	m_pFilePath = new QLineEdit();
	m_pFilePath->setReadOnly(true);
	m_pInfoLayout->addWidget(m_pFilePath, row++, 0, 1, 3);

	m_pInfoLayout->addItem(new QSpacerItem(20, 30, QSizePolicy::Expanding, QSizePolicy::Expanding), row++, 1);

	
	m_pTabWidget = new QTabWidget();
	m_pMainLayout->addWidget(m_pTabWidget);

	// Details tab with scroll area
	m_pDetailsScroll = new QScrollArea();
	m_pDetailsScroll->setFrameShape(QFrame::NoFrame);
	m_pDetailsScroll->setWidgetResizable(true);
	//QPalette pal = m_pDetailsScroll->palette();
	//pal.setColor(QPalette::Window, Qt::transparent);
	//m_pDetailsScroll->setPalette(pal);

	m_pDetailsWidget = new QWidget();
	m_pDetailsLayout = new QGridLayout(m_pDetailsWidget);
	m_pDetailsLayout->setContentsMargins(9, 9, 9, 9);
	m_pDetailsLayout->setSpacing(6);

	m_pDetailsScroll->setWidget(m_pDetailsWidget);
	m_pTabWidget->addTab(m_pDetailsScroll, tr("Details"));

	QFont boldFont;
	boldFont.setBold(true);

	int detailRow = 0;

	// Process Information
	QLabel* pProcessHeader = new QLabel(tr("Process Information"));
	pProcessHeader->setFont(boldFont);
	m_pDetailsLayout->addWidget(pProcessHeader, detailRow++, 0, 1, 3);

	m_pDetailsLayout->addWidget(new QLabel(tr("Running Processes:")), detailRow, 0);
	m_pProcessCountValue = new QLabel();
	m_pProcessCountValue->setTextInteractionFlags(Qt::TextSelectableByMouse);
	m_pDetailsLayout->addWidget(m_pProcessCountValue, detailRow++, 1, 1, 2);

	m_pDetailsLayout->addWidget(new QLabel(tr("Last Execution:")), detailRow, 0);
	m_pLastExecutionValue = new QLabel();
	m_pLastExecutionValue->setTextInteractionFlags(Qt::TextSelectableByMouse);
	m_pDetailsLayout->addWidget(m_pLastExecutionValue, detailRow++, 1, 1, 2);

	// Rules Information
	QLabel* pRulesHeader = new QLabel(tr("Rules"));
	pRulesHeader->setFont(boldFont);
	m_pDetailsLayout->addWidget(pRulesHeader, detailRow++, 0, 1, 3);

	m_pDetailsLayout->addWidget(new QLabel(tr("Program Rules:")), detailRow, 0);
	m_pProgramRulesValue = new QLabel();
	m_pProgramRulesValue->setTextInteractionFlags(Qt::TextSelectableByMouse);
	m_pDetailsLayout->addWidget(m_pProgramRulesValue, detailRow++, 1, 1, 2);

	m_pDetailsLayout->addWidget(new QLabel(tr("Access Rules:")), detailRow, 0);
	m_pAccessRulesValue = new QLabel();
	m_pAccessRulesValue->setTextInteractionFlags(Qt::TextSelectableByMouse);
	m_pDetailsLayout->addWidget(m_pAccessRulesValue, detailRow++, 1, 1, 2);

	m_pDetailsLayout->addWidget(new QLabel(tr("Firewall Rules:")), detailRow, 0);
	m_pFwRulesValue = new QLabel();
	m_pFwRulesValue->setTextInteractionFlags(Qt::TextSelectableByMouse);
	m_pDetailsLayout->addWidget(m_pFwRulesValue, detailRow++, 1, 1, 2);

	// File Access Information
	QLabel* pFileHeader = new QLabel(tr("File Access"));
	pFileHeader->setFont(boldFont);
	m_pDetailsLayout->addWidget(pFileHeader, detailRow++, 0, 1, 3);

	m_pDetailsLayout->addWidget(new QLabel(tr("Accessed Files:")), detailRow, 0);
	m_pOpenFilesValue = new QLabel();
	m_pOpenFilesValue->setTextInteractionFlags(Qt::TextSelectableByMouse);
	m_pDetailsLayout->addWidget(m_pOpenFilesValue, detailRow++, 1, 1, 2);

	// Network Information
	QLabel* pNetHeader = new QLabel(tr("Network Activity"));
	pNetHeader->setFont(boldFont);
	m_pDetailsLayout->addWidget(pNetHeader, detailRow++, 0, 1, 3);

	m_pDetailsLayout->addWidget(new QLabel(tr("Open Sockets:")), detailRow, 0);
	m_pOpenSocketsValue = new QLabel();
	m_pOpenSocketsValue->setTextInteractionFlags(Qt::TextSelectableByMouse);
	m_pDetailsLayout->addWidget(m_pOpenSocketsValue, detailRow++, 1, 1, 2);

	m_pDetailsLayout->addWidget(new QLabel(tr("Last Net Activity:")), detailRow, 0);
	m_pLastNetActivityValue = new QLabel();
	m_pLastNetActivityValue->setTextInteractionFlags(Qt::TextSelectableByMouse);
	m_pDetailsLayout->addWidget(m_pLastNetActivityValue, detailRow++, 1, 1, 2);

	m_pDetailsLayout->addWidget(new QLabel(tr("Upload Rate:")), detailRow, 0);
	m_pUploadValue = new QLabel();
	m_pUploadValue->setTextInteractionFlags(Qt::TextSelectableByMouse);
	m_pDetailsLayout->addWidget(m_pUploadValue, detailRow, 1);

	m_pDetailsLayout->addWidget(new QLabel(tr("Download Rate:")), detailRow, 2);
	m_pDownloadValue = new QLabel();
	m_pDownloadValue->setTextInteractionFlags(Qt::TextSelectableByMouse);
	m_pDetailsLayout->addWidget(m_pDownloadValue, detailRow++, 3);

	m_pDetailsLayout->addWidget(new QLabel(tr("Total Uploaded:")), detailRow, 0);
	m_pUploadedValue = new QLabel();
	m_pUploadedValue->setTextInteractionFlags(Qt::TextSelectableByMouse);
	m_pDetailsLayout->addWidget(m_pUploadedValue, detailRow, 1);

	m_pDetailsLayout->addWidget(new QLabel(tr("Total Downloaded:")), detailRow, 2);
	m_pDownloadedValue = new QLabel();
	m_pDownloadedValue->setTextInteractionFlags(Qt::TextSelectableByMouse);
	m_pDetailsLayout->addWidget(m_pDownloadedValue, detailRow++, 3);

	// Other Information
	QLabel* pOtherHeader = new QLabel(tr("Other"));
	pOtherHeader->setFont(boldFont);
	m_pDetailsLayout->addWidget(pOtherHeader, detailRow++, 0, 1, 3);

	m_pDetailsLayout->addWidget(new QLabel(tr("Log Size:")), detailRow, 0);
	m_pLogSizeValue = new QLabel();
	m_pLogSizeValue->setTextInteractionFlags(Qt::TextSelectableByMouse);
	m_pDetailsLayout->addWidget(m_pLogSizeValue, detailRow++, 1, 1, 2);

	// Add spacer to push content to top
	m_pDetailsLayout->addItem(new QSpacerItem(20, 20, QSizePolicy::Minimum, QSizePolicy::Expanding), detailRow, 0);

	// Set column stretches
	m_pDetailsLayout->setColumnStretch(1, 1);
	m_pDetailsLayout->setColumnStretch(3, 1);

	// Extended info tree tab
	m_pExtendedInfo = new CPanelWidgetEx();
	m_pExtendedInfo->GetTree()->setAlternatingRowColors(theConf->GetBool("Options/AltRowColors", false));
	m_pExtendedInfo->GetTree()->setHeaderLabels(tr("Extended Info").split("|"));
	m_pTabWidget->addTab(m_pExtendedInfo, tr("Extended"));

	QByteArray Columns = theConf->GetBlob("MainWindow/ProgramInfoView_Columns");
	if (Columns.isEmpty()) {
		m_pExtendedInfo->GetTree()->setColumnWidth(0, 300);
	} else
		m_pExtendedInfo->GetTree()->header()->restoreState(Columns);

	// Multiple items view
	m_pMultiItemWidget = new QWidget();
	m_pMultiItemLayout = new QVBoxLayout();
	m_pMultiItemLayout->setContentsMargins(0, 0, 0, 0);
	m_pMultiItemWidget->setLayout(m_pMultiItemLayout);
	m_pStackedLayout->addWidget(m_pMultiItemWidget);

	m_pProgramList = new CPanelWidgetEx();
	m_pProgramList->GetTree()->setAlternatingRowColors(theConf->GetBool("Options/AltRowColors", false));
	m_pProgramList->GetTree()->setHeaderLabels(tr("Name|Type|Processes|Path").split("|"));
	m_pProgramList->GetTree()->setSelectionMode(QAbstractItemView::SingleSelection);
	m_pProgramList->GetTree()->setSortingEnabled(false);
	connect(m_pProgramList->GetTree(), SIGNAL(currentItemChanged(QTreeWidgetItem*,QTreeWidgetItem*)), this, SLOT(OnCurrentChanged(QTreeWidgetItem*,QTreeWidgetItem*)));
	m_pMultiItemLayout->addWidget(m_pProgramList);

	QByteArray ListColumns = theConf->GetBlob("MainWindow/ProgramList_Columns");
	if (ListColumns.isEmpty()) {
		m_pProgramList->GetTree()->setColumnWidth(0, 200);
		m_pProgramList->GetTree()->setColumnWidth(1, 100);
		m_pProgramList->GetTree()->setColumnWidth(2, 80);
	} else
		m_pProgramList->GetTree()->header()->restoreState(ListColumns);
}

CInfoView::~CInfoView()
{
	theConf->SetBlob("MainWindow/ProgramInfoView_Columns", m_pExtendedInfo->GetTree()->header()->saveState());
	theConf->SetBlob("MainWindow/ProgramList_Columns", m_pProgramList->GetTree()->header()->saveState());
}

void CInfoView::Sync(const QList<CProgramItemPtr>& Items)
{
	if (m_Items != Items)
	{
		m_Items = Items;

		if (m_Items.count() <= 1)
		{
			// Switch to single item view
			m_pStackedLayout->setCurrentWidget(m_pOneItemWidget);

			if (m_Items.count() == 1)
			{
				ShowProgram(m_Items.first());
				UpdateDetailsTab(m_Items);
			}
			else
			{
				Clear();
			}
		}
		else
		{
			// Switch to multiple items view
			m_pStackedLayout->setCurrentWidget(m_pMultiItemWidget);

			// Update Details tab with aggregated stats for all items
			UpdateDetailsTab(m_Items);

			// Populate the program list
			m_pProgramList->GetTree()->clear();
			foreach(const CProgramItemPtr& pItem, m_Items)
			{
				QTreeWidgetItem* pTreeItem = new QTreeWidgetItem();
				pTreeItem->setText(0, pItem->GetNameEx());
				pTreeItem->setText(1, pItem->GetTypeStr());
				pTreeItem->setText(2, QString::number(pItem->GetStats()->ProcessCount));
				pTreeItem->setText(3, pItem->GetPath());
				pTreeItem->setData(0, Qt::UserRole, QVariant::fromValue(pItem));

				QIcon icon = pItem->GetIcon();
				if (!icon.isNull())
					pTreeItem->setIcon(0, icon);

				m_pProgramList->GetTree()->addTopLevelItem(pTreeItem);
			}

			// Select the first item by default
			if (m_pProgramList->GetTree()->topLevelItemCount() > 0)
			{
				m_pProgramList->GetTree()->setCurrentItem(m_pProgramList->GetTree()->topLevelItem(0));
			}
		}
	}
}

void CInfoView::ShowProgram(const CProgramItemPtr& pItem)
{
	Clear();
	if (!pItem)
		return;

	// main details (info box at top)
	QIcon icon = pItem->GetIcon();
	if (icon.isNull())
		icon = QPixmap(":/exe32.png");
	m_pIcon->setPixmap(icon.pixmap(32,32));

	m_pProgramName->setText(pItem->GetNameEx());

	m_pFilePath->setText(pItem->GetPath());

	m_pCompanyName->setText(pItem->GetPublisher());

	m_pType->setText(pItem->GetTypeStr());

	//m_pInfo->setText(); // todo

	// extended info tab
	ShowCurrent(pItem);
}

void CInfoView::ShowCurrent(const CProgramItemPtr& pItem)
{
	ClearExtended();
	if(!pItem)
		return;

	auto pRoot = theCore->ProgramManager()->GetRoot();

	QTreeWidgetItem* pName = new QTreeWidgetItem();
	pName->setText(0, tr("Name: %1 (%2)").arg(pItem->GetName()).arg(pItem->GetUID()));
	m_pExtendedInfo->GetTree()->addTopLevelItem(pName);

	QTreeWidgetItem* pDosPath = new QTreeWidgetItem();
	pDosPath->setText(0, tr("Path: %1").arg(pItem->GetPath()));
	m_pExtendedInfo->GetTree()->addTopLevelItem(pDosPath);

	auto Groups = pItem->GetGroups();

	if (Groups.count() > 0) {
		QTreeWidgetItem* pGroups = new QTreeWidgetItem();
		pGroups->setText(0, tr("Groups (%1)").arg(Groups.count()));
		m_pExtendedInfo->GetTree()->addTopLevelItem(pGroups);

		foreach(auto Group, Groups)
		{
			CProgramSetPtr pSet = Group.lock().objectCast<CProgramSet>();
			if(!pSet) continue;
			QTreeWidgetItem* pGroup = new QTreeWidgetItem();
			if(pRoot == pSet)
				pGroup->setText(0, tr("Root (%1)").arg(pSet->GetUID()));
			else
				pGroup->setText(0, tr("%1 (%2)").arg(pSet->GetName()).arg(pSet->GetUID()));
			pGroups->addChild(pGroup);
		}

		m_pExtendedInfo->GetTree()->expandAll();
	}
}

void CInfoView::UpdateDetailsTab(const QList<CProgramItemPtr>& Items)
{
	if (Items.isEmpty())
	{
		ClearStatistics();
		return;
	}

	// Aggregate statistics from all items
	int totalProcessCount = 0;
	quint64 latestExecution = 0;
	int totalProgRuleCount = 0;
	int totalProgRuleTotal = 0;
	int totalResRuleCount = 0;
	int totalResRuleTotal = 0;
	int totalFwRuleCount = 0;
	int totalFwRuleTotal = 0;
	quint32 totalAccessCount = 0;
	int totalSocketCount = 0;
	quint64 latestNetActivity = 0;
	quint64 totalUpload = 0;
	quint64 totalDownload = 0;
	quint64 totalUploaded = 0;
	quint64 totalDownloaded = 0;
	quint64 totalLogSize = 0;

	foreach(const CProgramItemPtr& pItem, Items)
	{
		const SProgramStats* pStats = pItem->GetStats();
		if (pStats)
		{
			totalProcessCount += pStats->ProcessCount;
			if (pStats->LastExecution > latestExecution)
				latestExecution = pStats->LastExecution;

			totalProgRuleCount += pStats->ProgRuleCount;
			totalProgRuleTotal += pStats->ProgRuleTotal;
			totalResRuleCount += pStats->ResRuleCount;
			totalResRuleTotal += pStats->ResRuleTotal;
			totalFwRuleCount += pStats->FwRuleCount;
			totalFwRuleTotal += pStats->FwRuleTotal;

			totalAccessCount += pStats->AccessCount;
			totalSocketCount += pStats->SocketCount;

			if (pStats->LastNetActivity > latestNetActivity)
				latestNetActivity = pStats->LastNetActivity;

			totalUpload += pStats->Upload;
			totalDownload += pStats->Download;
			totalUploaded += pStats->Uploaded;
			totalDownloaded += pStats->Downloaded;
		}

		totalLogSize += pItem->GetLogMemUsage();
	}

	// Display aggregated statistics

	// Process Information
	m_pProcessCountValue->setText(QString::number(totalProcessCount));
	if (latestExecution > 0)
		m_pLastExecutionValue->setText(QDateTime::fromMSecsSinceEpoch(latestExecution).toString("dd.MM.yyyy hh:mm:ss"));
	else
		m_pLastExecutionValue->setText(tr("Never"));

	// Rules
	if (totalProgRuleCount != totalProgRuleTotal)
		m_pProgramRulesValue->setText(tr("%1 / %2 (enabled / total)").arg(totalProgRuleCount).arg(totalProgRuleTotal));
	else
		m_pProgramRulesValue->setText(QString::number(totalProgRuleTotal));

	if (totalResRuleCount != totalResRuleTotal)
		m_pAccessRulesValue->setText(tr("%1 / %2 (enabled / total)").arg(totalResRuleCount).arg(totalResRuleTotal));
	else
		m_pAccessRulesValue->setText(QString::number(totalResRuleTotal));

	if (totalFwRuleCount != totalFwRuleTotal)
		m_pFwRulesValue->setText(tr("%1 / %2 (enabled / total)").arg(totalFwRuleCount).arg(totalFwRuleTotal));
	else
		m_pFwRulesValue->setText(QString::number(totalFwRuleTotal));

	// File Access
	m_pOpenFilesValue->setText(QString::number(totalAccessCount));

	// Network
	m_pOpenSocketsValue->setText(QString::number(totalSocketCount));

	if (latestNetActivity > 0)
		m_pLastNetActivityValue->setText(QDateTime::fromMSecsSinceEpoch(latestNetActivity).toString("dd.MM.yyyy hh:mm:ss"));
	else
		m_pLastNetActivityValue->setText(tr("Never"));

	m_pUploadValue->setText(FormatSize(totalUpload) + "/s");
	m_pDownloadValue->setText(FormatSize(totalDownload) + "/s");
	m_pUploadedValue->setText(FormatSize(totalUploaded));
	m_pDownloadedValue->setText(FormatSize(totalDownloaded));

	// Other
	m_pLogSizeValue->setText(FormatSize(totalLogSize));
}

void CInfoView::OnCurrentChanged(QTreeWidgetItem* current, QTreeWidgetItem* previous)
{
	Q_UNUSED(previous);

	CProgramItemPtr pItem;
	if(current) pItem = current->data(0, Qt::UserRole).value<CProgramItemPtr>();
	ShowCurrent(pItem);
}

void CInfoView::Clear()
{
	m_pIcon->setPixmap(QPixmap(":/exe32.png").scaled(32, 32, Qt::KeepAspectRatio, Qt::SmoothTransformation));
	m_pProgramName->clear();
	m_pCompanyName->clear();
	m_pType->clear();
	m_pInfo->clear();
	m_pFilePath->clear();

	ClearStatistics();
	ClearExtended();
}

void CInfoView::ClearStatistics()
{
	// Clear statistics
	m_pProcessCountValue->clear();
	m_pLastExecutionValue->clear();
	m_pProgramRulesValue->clear();
	m_pAccessRulesValue->clear();
	m_pFwRulesValue->clear();
	m_pOpenFilesValue->clear();
	m_pOpenSocketsValue->clear();
	m_pLastNetActivityValue->clear();
	m_pUploadValue->clear();
	m_pDownloadValue->clear();
	m_pUploadedValue->clear();
	m_pDownloadedValue->clear();
	m_pLogSizeValue->clear();
}

void CInfoView::ClearExtended()
{
	m_pExtendedInfo->GetTree()->clear();
}