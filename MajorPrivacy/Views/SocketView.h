#pragma once

#include "../../MiscHelpers/Common/PanelView.h"
#include "../../MiscHelpers/Common/TreeviewEx.h"
#include "../Models/SocketModel.h"
#include "../Core/Programs/ProgramFile.h"
#include "../Core/Programs/WindowsService.h"

class CSocketView : public CPanelViewEx<CSocketModel>
{
	Q_OBJECT

public:
	CSocketView(QWidget *parent = 0);
	virtual ~CSocketView();

	void					Sync(const QMap<quint64, CProcessPtr>& Processes, const QSet<CWindowsServicePtr>& ServicesEx);
	
protected:
	virtual void			OnMenu(const QPoint& Point) override;
	void					OnDoubleClicked(const QModelIndex& Index) override;

private slots:
	//void					OnResetColumns();
	//void					OnColumnsChanged();
private:

};
