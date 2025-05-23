#pragma once

#include "../../MiscHelpers/Common/PanelView.h"
#include "../../MiscHelpers/Common/TreeviewEx.h"
#include "../Models/VolumeModel.h"
#include "../Core/Programs/ProgramFile.h"
#include "../Core/Programs/WindowsService.h"

class CVolumeView : public CPanelViewEx<CVolumeModel>
{
	Q_OBJECT

public:
	CVolumeView(QWidget *parent = 0);
	virtual ~CVolumeView();

	void					Sync();
	
	QString					GetSelectedVolumePath();
	QString					GetSelectedVolumeImage();

protected:
	void					OnMenu(const QPoint& Point) override;
	void					OnDoubleClicked(const QModelIndex& Index) override;

private slots:
	//void					OnResetColumns();
	//void					OnColumnsChanged();

	void					OnMountVolume();
	void					OnUnmountVolume();
	void					OnUnmountAllVolumes();
	void					OnCreateVolume();
	void					OnChangeVolumePassword();
	void					OnRenameVolume();
	void					OnRemoveVolume();
	void					OnAddFolder();

	void					MountVolume(QString Path = QString());

private:

	QToolBar*				m_pToolBar;

	QAction*				m_pMountVolume;
	QAction*				m_pUnmountVolume;
	QAction*				m_pCreateVolume;
	QAction*				m_pChangeVolumePassword;
	QAction*				m_pRenameVolume;
	QAction*				m_pRemoveVolume;
	QAction*				m_pMountAndAddVolume;
	QAction*				m_pUnmountAllVolumes;
	QAction*				m_pAddFolder;
};
