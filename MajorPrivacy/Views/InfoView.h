#pragma once

#include "../../MiscHelpers/Common/PanelView.h"
#include "../../MiscHelpers/Common/TreeWidgetEx.h"
#include "../Core/Programs/ProgramItem.h"

class CInfoView : public QWidget
{
	Q_OBJECT

public:
	CInfoView(QWidget *parent = 0);
	virtual ~CInfoView();

	void Sync(const QList<CProgramItemPtr>& Items);
	void Clear();

protected:
	void ClearStatistics();
	void ClearExtended();
	void ShowProgram(const CProgramItemPtr& pItem);
	void ShowCurrent(const CProgramItemPtr& pItem);
	void UpdateDetailsTab(const QList<CProgramItemPtr>& Items);

private slots:
	void OnCurrentChanged(QTreeWidgetItem* current, QTreeWidgetItem* previous);

private:
	QVBoxLayout*			m_pMainLayout;

	// Stacked widget for single vs multiple items
	QWidget*				m_pStackedWidget;
	QStackedLayout*			m_pStackedLayout;

	// Single item view
	QWidget*				m_pOneItemWidget;
	QVBoxLayout*			m_pOneItemLayout;

	QGroupBox*				m_pInfoBox;
	QGridLayout*			m_pInfoLayout;

	QLabel*					m_pIcon;
	QLabel*					m_pProgramName;
	QLabel*					m_pCompanyName;
	QLabel*					m_pType;
	QLabel*					m_pInfo;
	QLineEdit*				m_pFilePath;

	// Multiple items view
	QWidget*				m_pMultiItemWidget;
	QVBoxLayout*			m_pMultiItemLayout;
	CPanelWidgetEx*			m_pProgramList;

	QTabWidget*				m_pTabWidget;
	QScrollArea*			m_pDetailsScroll;
	QWidget*				m_pDetailsWidget;
	QGridLayout*			m_pDetailsLayout;

	// Statistics fields
	QLabel*					m_pProcessCountValue;
	QLabel*					m_pLastExecutionValue;
	QLabel*					m_pProgramRulesValue;
	QLabel*					m_pAccessRulesValue;
	QLabel*					m_pFwRulesValue;
	QLabel*					m_pOpenFilesValue;
	QLabel*					m_pOpenSocketsValue;
	QLabel*					m_pLastNetActivityValue;
	QLabel*					m_pUploadValue;
	QLabel*					m_pDownloadValue;
	QLabel*					m_pUploadedValue;
	QLabel*					m_pDownloadedValue;
	QLabel*					m_pLogSizeValue;

	CPanelWidgetEx*			m_pExtendedInfo;

	// Current items
	QList<CProgramItemPtr>	m_Items;
};
