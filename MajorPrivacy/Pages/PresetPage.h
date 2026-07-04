#pragma once
#include <QWidget>
#include "../../MiscHelpers/Common/TreeViewEx.h"

class CPresetView;
class CPresetProperties;

class CPresetPage : public QWidget
{
	Q_OBJECT
public:
	CPresetPage(QWidget* parent);
	~CPresetPage();

	void	Update();
	void	Clear();

private slots:
	void	OnCurrentChanged(const QString& presetGuid);
	void	OnPresetModified(bool bModified);

private:
	QVBoxLayout*			m_pMainLayout;

	QSplitter*				m_pHSplitter;
	CPresetView*			m_pPresetView;
	CPresetProperties*		m_pPresetProperties;

	QString					m_CurrentPresetGuid;
};
