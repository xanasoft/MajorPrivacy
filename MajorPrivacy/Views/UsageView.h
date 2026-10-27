#pragma once
#include <QWidget>
#include "../Core/Programs/ProgramFile.h"
#include "../Core/Programs/WindowsService.h"

class CUsageGraph;
class CElidedLabel;

enum class EUsageMetric
{
	eCpu = 0,	// user + kernel time, kernel time, GPU time, NPU time
	eIo,		// read, write, other bytes
	eNet		// internet upload, internet download, LAN upload, LAN download bytes; open connections on the right axis
};

//
// One usage graph tab: sums the usage history of the selected items over the time range
// of the toolbar combo and plots it as rates; hovering shows the top contributors.
//

class CUsageView : public QWidget
{
	Q_OBJECT
public:
	CUsageView(EUsageMetric Metric, QWidget* parent = nullptr);
	virtual ~CUsageView();

	void					Sync(const QSet<CProgramFilePtr>& Programs, const QSet<CWindowsServicePtr>& Services);
	void					Clear();

protected:
	virtual void			changeEvent(QEvent* pEvent) override;

private slots:
	void					OnHovered(qint64 TimeMs, const QPoint& GlobalPos, int Curve);
	void					OnHoverLeft();
	void					OnRefresh();
	void					OnZoomChanged();

private:
	void					Rebuild();
	void					UpdateZoomIcons();
	double					RawValue(const SUsageSample& Sample, int Curve) const;
	double					ToRate(double Raw, quint32 Seconds, int Curve) const;
	QString					FormatRate(double Rate) const;
	QString					FormatBreakdown(const SUsageSample& Sample, quint32 Seconds) const;
	QString					FormatBreakdown(const QVector<double>& Raw, quint32 Seconds) const;
	quint32					SlotSeconds(int Slot) const;
	double					RankValue(const SUsageSample& Sample, int Focus) const;
	QString					RankName(int Focus) const;
	bool					HideLan() const; // traffic graph: the LAN lines are switched off
	bool					IsCountCurve(int Curve) const { return m_Metric == EUsageMetric::eNet && Curve == 4; }
	QString					FormatCount(double Count) const;
	QString					IconHtml(const CProgramItemPtr& pItem);

	EUsageMetric			m_Metric;
	int						m_CurveCount = 0;
	int						m_ValueCount = 0;	// summed values: the curves, then values that only show in the text (traffic: connections in, out)
	int						m_Cores = 1;

	QVBoxLayout*			m_pMainLayout = nullptr;
	QToolBar*				m_pToolBar = nullptr;
	CElidedLabel*			m_pInfo = nullptr;
	QAction*				m_pZoomOut = nullptr;
	QAction*				m_pZoomIn = nullptr;
	QAction*				m_pZoomFit = nullptr;
	QToolButton*			m_pBtnRefresh = nullptr;
	QToolButton*			m_pBtnPrivate = nullptr;	// I/O and traffic only: leaves out programs with a private trace preset
	QToolButton*			m_pBtnLan = nullptr;		// traffic only: hides the LAN upload/download lines, remembered in the settings
	CUsageGraph*			m_pGraph = nullptr;

	QSet<CProgramItemPtr>	m_CurItems;
	QList<CProgramItemPtr>	m_Items;

	quint64					m_Range = 0;		// ms, 0 = everything the service keeps
	EUsageTier				m_Tier = EUsageTier::eMinutes;
	quint64					m_LastFetch = 0;	// ms since epoch
	quint32					m_LastError = 0;	// NTSTATUS of the last failed fetch, shown in the info line

	// the slots currently plotted
	qint64					m_From = 0;			// unix seconds
	qint64					m_To = 0;			// end of the slot that is being filled
	qint64					m_Now = 0;			// time of the last rebuild
	qint64					m_LastFull = 0;		// time of the last full rebuild
	quint32					m_SlotLen = 60;		// seconds per plotted slot, a multiple of the bucket interval
	int						m_Slots = 0;
	bool					m_bNoData = true;
	QVector<QVector<double>> m_Sums;			// [curve][slot] raw sums

	QHash<quint64, QString>	m_IconCache;		// item UID -> <img> tag with the icon as a data URL, for the tooltip
};
