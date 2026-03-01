#pragma once

#include "../../MiscHelpers/Common/PanelView.h"
#include "../../MiscHelpers/Common/TreeviewEx.h"
#include "../Models/VolumeModel.h"
#include "../Core/Programs/ProgramFile.h"
#include "../Core/Programs/WindowsService.h"
#include <functional>

class CVolumeView : public CPanelViewEx<CVolumeModel>
{
	Q_OBJECT

public:
	CVolumeView(QWidget *parent = 0);
	virtual ~CVolumeView();

	void					Sync();
	void					Clear();
	
	QString					GetSelectedVolumePath();
	QString					GetSelectedVolumeImage();

protected:
	void					OnMenu(const QPoint& Point) override;
	void					OnDoubleClicked(const QModelIndex& Index) override;

	void					OpenVolumeDialog(const CVolumePtr& pVolume);

private slots:
	//void					OnResetColumns();
	//void					OnColumnsChanged();

	void					OnMountVolume();
	void					OnUnmountVolume();
	void					OnUnmountAllVolumes();
	void					OnCreateVolume();
	void					OnChangeVolumePassword();
	void					OnBackupVolumeHeader();
	void					OnRestoreVolumeHeader();
	void					OnExpandVolume();
	void					OnChangeVolumeConfig();
	void					OnAddVolumeEnclave();
	void					OnRenameVolume();
	void					OnRemoveVolume();
	void					OnAddFolder();
	void					OnCleanupVolumes();

	void					MountVolume(QString Path = QString(), CVolumePtr pVolume = CVolumePtr());

	void					OnVolumeOperationFinished();

private:
	void					RunVolumeOperation(const QString& progressText, std::function<STATUS()> operation, std::function<void()> onSuccess = nullptr, const QList<CVolumePtr>& Volumes = QList<CVolumePtr>());

	QToolBar*				m_pToolBar;

	QAction*				m_pMountVolume;
	QAction*				m_pUnmountVolume;
	QAction*				m_pCreateVolume;
	QAction*				m_pChangeVolumePassword;
	QAction*				m_pBackupVolumeHeader;
	QAction*				m_pRestoreVolumeHeader;
	QAction*				m_pExpandVolume;
	QAction*				m_pChangeVolumeConfig;
	QAction*				m_pAddVolumeEnclave;
	QAction*				m_pRenameVolume;
	QAction*				m_pRemoveVolume;
	QAction*				m_pMountAndAddVolume;
	QAction*				m_pAddFolder;
	QAction*				m_pUnmountAllVolumes;
	QAction*				m_pCleanupVolumes;
};
