#pragma once

#include "../../MiscHelpers/Common/PanelView.h"
#include "../../MiscHelpers/Common/TreeWidgetEx.h"
#include "../Core/Programs/ProgramItem.h"
#include "LibraryView.h"

class CLibraryInfoView : public QWidget
{
	Q_OBJECT

public:
	CLibraryInfoView(QWidget *parent = 0);
	virtual ~CLibraryInfoView();

	void Sync();

	enum EColumns
	{
		eName = 0,
		eSigner,
		eValid,
		eSHA1,
		eSHAX,
		eCount
	};

protected:

private slots:

private:
	QGridLayout*			m_pMainLayout;
	CPanelWidgetEx*			m_pInfo;
};
