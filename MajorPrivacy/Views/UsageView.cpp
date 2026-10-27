#include "pch.h"
#include "UsageView.h"
#include "../MajorPrivacy.h"
#include "../Core/PrivacyCore.h"
#include "../Widgets/UsageGraph.h"
#include "../../MiscHelpers/Common/Common.h"
#include "../../MiscHelpers/Common/OtherFunctions.h"

#define USAGE_MAX_SLOTS			2000	// more points than pixels are not worth drawing
#define USAGE_FETCH_PERIOD		1000	// ms between fetches; only the open bucket travels when nothing else changed
#define USAGE_FULL_REBUILD		10000	// ms between full re-sums of the history, in between only the open slot is updated

//////////////////////////////////////////////////////////////////////////
// CElidedLabel
//
// A status label that gives way: it reports no minimum width, so when the row gets too
// narrow the text is elided before the buttons next to it are pushed out.
//

class CElidedLabel : public QLabel
{
public:
	CElidedLabel(QWidget* parent = nullptr) : QLabel(parent)
	{
		setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
	}

	void SetText(const QString& Text)
	{
		m_Text = Text;
		setToolTip(Text);
		Elide();
	}

	virtual QSize minimumSizeHint() const override
	{
		return QSize(0, QLabel::minimumSizeHint().height());
	}

protected:
	virtual void resizeEvent(QResizeEvent* pEvent) override
	{
		QLabel::resizeEvent(pEvent);
		Elide();
	}

	void Elide()
	{
		QMargins Margins = contentsMargins();
		int Width = width() - Margins.left() - Margins.right();
		QLabel::setText(fontMetrics().elidedText(m_Text, Qt::ElideRight, qMax(0, Width)));
	}

	QString m_Text;
};

//////////////////////////////////////////////////////////////////////////
// CUsageView

CUsageView::CUsageView(EUsageMetric Metric, QWidget* parent)
	: QWidget(parent)
{
	m_Metric = Metric;
	m_Cores = qMax(1, QThread::idealThreadCount());

	m_pMainLayout = new QVBoxLayout(this);
	m_pMainLayout->setContentsMargins(0, 0, 0, 0);

	// status text on the left, buttons on the right; the text yields first when space is short
	QHBoxLayout* pTopLayout = new QHBoxLayout();
	pTopLayout->setContentsMargins(0, 0, 0, 0);
	pTopLayout->setSpacing(0);
	m_pMainLayout->addLayout(pTopLayout);

	m_pInfo = new CElidedLabel();
	m_pInfo->setContentsMargins(4, 0, 4, 0);
	pTopLayout->addWidget(m_pInfo, 1);

	m_pToolBar = new QToolBar();
	m_pToolBar->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
	pTopLayout->addWidget(m_pToolBar, 0);

	//
	// The graph shows volumes only, so it includes everything by default, unlike the
	// traffic and access lists which hide private items; this hides them on demand.
	//
	if (m_Metric != EUsageMetric::eCpu)
	{
		m_pBtnPrivate = new QToolButton();
		m_pBtnPrivate->setIcon(QIcon(":/Icons/Invisible.png"));
		m_pBtnPrivate->setCheckable(true);
		m_pBtnPrivate->setToolTip(tr("Hide Private Entries - leave out programs with a private trace preset"));
		m_pBtnPrivate->setFixedHeight(22);
		connect(m_pBtnPrivate, SIGNAL(clicked()), this, SLOT(OnRefresh()));
		m_pToolBar->addWidget(m_pBtnPrivate);
		m_pToolBar->addSeparator();
	}

	//
	// LAN traffic is of no interest to many: the two LAN lines, and their share of the
	// tooltips and the summary, can be switched off. Remembered across sessions.
	//
	if (m_Metric == EUsageMetric::eNet)
	{
		m_pBtnLan = new QToolButton();
		// a "disable" overlay over 60% of the icon, bottom right, like the other hide buttons (FwRuleView, LibraryView)
		m_pBtnLan->setIcon(QIcon(QPixmap::fromImage(ImageAddOverlay(QImage(":/Icons/Home.png"), ":/Icons/Disable.png", 30))));
		m_pBtnLan->setCheckable(true);
		m_pBtnLan->setToolTip(tr("Hide LAN Traffic - leave out the LAN upload and download lines"));
		m_pBtnLan->setFixedHeight(22);
		m_pBtnLan->setChecked(theConf->GetBool("Options/UsageHideLan", false));
		connect(m_pBtnLan, &QToolButton::toggled, this, [this](bool checked) {
			theConf->SetValue("Options/UsageHideLan", checked);
			for (int c = 2; c < 4; c++) // the two LAN lines
				m_pGraph->SetCurveVisible(c, !checked);
			m_pGraph->Replot();
			OnRefresh();
		});
		m_pToolBar->addWidget(m_pBtnLan);
		m_pToolBar->addSeparator();
	}

	// zoom buttons (as in Mops): they walk the zoom history the mouse gestures build
	m_pZoomOut = m_pToolBar->addAction(QString(), this, [this]() { m_pGraph->ZoomOut(); });
	m_pZoomOut->setToolTip(tr("Zoom out - back one step in the zoom history (right-click, double-click)"));
	m_pZoomIn = m_pToolBar->addAction(QString(), this, [this]() { m_pGraph->ZoomIn(); });
	m_pZoomIn->setToolTip(tr("Zoom in - forward one step in the zoom history (Shift+right-click, Ctrl+double-click)"));
	m_pZoomFit = m_pToolBar->addAction(QString(), this, [this]() { m_pGraph->ZoomFit(); });
	m_pZoomFit->setToolTip(tr("Fit all - show the whole time span and value range (Ctrl+right-click, Alt+double-click)\nDrag a rectangle to zoom in, the mouse wheel scales the value axis"));
	m_pToolBar->addSeparator();

	m_pBtnRefresh = new QToolButton();
	m_pBtnRefresh->setIcon(QIcon(":/Icons/Refresh.png"));
	m_pBtnRefresh->setToolTip(tr("Refresh"));
	m_pBtnRefresh->setFixedHeight(22);
	connect(m_pBtnRefresh, SIGNAL(clicked()), this, SLOT(OnRefresh()));
	m_pToolBar->addWidget(m_pBtnRefresh);

	m_pGraph = new CUsageGraph();
	m_pMainLayout->addWidget(m_pGraph);

	switch (m_Metric)
	{
	case EUsageMetric::eCpu:
		m_pGraph->SetYUnit(CUsageGraph::ePercent);
		m_pGraph->AddCurve(tr("CPU (user + kernel)"), QColor(0, 120, 215), true, 2);
		m_pGraph->AddCurve(tr("Kernel"), QColor(220, 60, 60), true, 1);
		m_pGraph->AddCurve(tr("GPU"), QColor(40, 160, 70), false, 2);
		m_pGraph->AddCurve(tr("NPU"), QColor(140, 90, 210), false, 2);
		m_CurveCount = 4;
		m_ValueCount = m_CurveCount;
		break;
	case EUsageMetric::eIo:
		m_pGraph->SetYUnit(CUsageGraph::eBytesPerSec);
		m_pGraph->AddCurve(tr("Read"), QColor(40, 160, 70), false, 2);
		m_pGraph->AddCurve(tr("Write"), QColor(230, 140, 0), false, 2);
		m_pGraph->AddCurve(tr("Other"), QColor(140, 90, 210), false, 1);
		m_CurveCount = 3;
		m_ValueCount = m_CurveCount;
		break;
	case EUsageMetric::eNet:
		m_pGraph->SetYUnit(CUsageGraph::eBytesPerSec);
		m_pGraph->AddCurve(tr("Internet Upload"), QColor(230, 100, 30), false, 2);
		m_pGraph->AddCurve(tr("Internet Download"), QColor(0, 140, 210), false, 2);
		m_pGraph->AddCurve(tr("LAN Upload"), QColor(200, 160, 30), false, 2);
		m_pGraph->AddCurve(tr("LAN Download"), QColor(40, 160, 70), false, 2);
		m_pGraph->AddCurve(tr("Open Connections"), QColor(150, 150, 150), false, 1, true); // right axis
		m_CurveCount = 5;
		m_ValueCount = 7; // 5: inbound, 6: outbound connection seconds, for the texts
		break;
	}

	connect(m_pGraph, SIGNAL(Hovered(qint64, const QPoint&, int)), this, SLOT(OnHovered(qint64, const QPoint&, int)));
	connect(m_pGraph, SIGNAL(HoverLeft()), this, SLOT(OnHoverLeft()));
	connect(m_pGraph, SIGNAL(ZoomChanged()), this, SLOT(OnZoomChanged()));

	UpdateZoomIcons();
	OnZoomChanged();
}

CUsageView::~CUsageView()
{
}

bool CUsageView::HideLan() const
{
	return m_pBtnLan && m_pBtnLan->isChecked();
}

void CUsageView::changeEvent(QEvent* pEvent)
{
	if (pEvent->type() == QEvent::PaletteChange || pEvent->type() == QEvent::StyleChange)
		UpdateZoomIcons();
	QWidget::changeEvent(pEvent);
}

void CUsageView::UpdateZoomIcons()
{
	QColor Ink = palette().color(QPalette::ButtonText);
	m_pZoomOut->setIcon(CUsageGraph::MakeZoomIcon(CUsageGraph::eZoomOut, Ink));
	m_pZoomIn->setIcon(CUsageGraph::MakeZoomIcon(CUsageGraph::eZoomIn, Ink));
	m_pZoomFit->setIcon(CUsageGraph::MakeZoomIcon(CUsageGraph::eZoomFit, Ink));
}

void CUsageView::OnZoomChanged()
{
	m_pZoomOut->setEnabled(m_pGraph->CanZoomOut());
	m_pZoomIn->setEnabled(m_pGraph->CanZoomIn());
	m_pZoomFit->setEnabled(m_pGraph->IsZoomed());
}

void CUsageView::Sync(const QSet<CProgramFilePtr>& Programs, const QSet<CWindowsServicePtr>& Services)
{
	//
	// Every sample lives in exactly one item (see CProcessList::UpdateUsage), so the
	// selection is simply summed.
	//
	bool bHidePrivate = m_pBtnPrivate && m_pBtnPrivate->isChecked();
	auto IsPrivate = [&](const CProgramItemPtr& pItem) {
		if (!bHidePrivate)
			return false;
		ETracePreset Preset = m_Metric == EUsageMetric::eNet ? pItem->GetNetTrace() : pItem->GetResTrace();
		return Preset == ETracePreset::ePrivate;
	};

	QSet<CProgramItemPtr> Items;
	foreach(const CProgramFilePtr& pProgram, Programs) {
		if (!IsPrivate(pProgram))
			Items.insert(pProgram);
	}
	foreach(const CWindowsServicePtr& pService, Services) {
		if (!IsPrivate(pService))
			Items.insert(pService);
	}

	quint64 Range = theGUI->GetGraphLimit();
	if (Range == 0) { // "Any time" - everything the service keeps
		qint64 Retention = theCore->GetConfigInt64("Service/UsageRetentionMinutes", 60 * 24 * 30);
		if (Retention <= 0)
			Retention = 60 * 24 * 30;
		Range = (quint64)Retention * 60 * 1000;
	}
	quint64 LiveWindow = (quint64)theCore->GetConfigInt64("Service/UsageSecondsWindow", 300) * 1000;
	EUsageTier Tier = Range <= LiveWindow ? EUsageTier::eSeconds : EUsageTier::eMinutes;

	if (Items != m_CurItems || Range != m_Range || Tier != m_Tier) {
		m_CurItems = Items;
		m_Items = Items.values();
		m_Range = Range;
		m_Tier = Tier;
		m_LastFetch = 0; // fetch right away
		m_LastFull = 0; // and re-sum everything
		m_pGraph->ZoomFit(); // a zoom into the old selection or span means nothing for the new one
	}

	quint64 uNow = (quint64)QDateTime::currentMSecsSinceEpoch();
	if (m_LastFetch && uNow - m_LastFetch < USAGE_FETCH_PERIOD)
		return;
	m_LastFetch = uNow;

	STATUS Status = theCore->UpdateUsageStats(m_Tier, m_Items);
	m_LastError = Status.IsError() ? (quint32)Status.GetStatus() : 0;

	Rebuild();
}

void CUsageView::OnRefresh()
{
	m_LastFetch = 0;
	m_LastFull = 0;
}

void CUsageView::Clear()
{
	m_CurItems.clear();
	m_Items.clear();
	m_LastFetch = 0;
	m_LastFull = 0;
	m_Slots = 0;
	m_Sums.clear();
	m_IconCache.clear();
	for (int i = 0; i < m_CurveCount; i++)
		m_pGraph->SetSamples(i, QVector<QPointF>());
	m_pGraph->ZoomFit();
	m_pGraph->Replot();
	m_pInfo->SetText(QString());
}

double CUsageView::RawValue(const SUsageSample& Sample, int Curve) const
{
	switch (m_Metric)
	{
	case EUsageMetric::eCpu:
		switch (Curve) {
		case 0:		return (double)Sample.CpuKernel + (double)Sample.CpuUser;
		case 1:		return (double)Sample.CpuKernel;
		case 2:		return (double)Sample.GpuTime;
		default:	return (double)Sample.NpuTime;
		}
	case EUsageMetric::eIo:
		return Curve == 0 ? (double)Sample.IoRead : Curve == 1 ? (double)Sample.IoWrite : (double)Sample.IoOther;
	case EUsageMetric::eNet:
		// NetUpload/NetDownload hold all traffic, the LAN fields the part of it that stayed local
		switch (Curve) {
		case 0:		return Sample.NetUpload > Sample.LanUpload ? (double)(Sample.NetUpload - Sample.LanUpload) : 0.0;
		case 1:		return Sample.NetDownload > Sample.LanDownload ? (double)(Sample.NetDownload - Sample.LanDownload) : 0.0;
		case 2:		return (double)Sample.LanUpload;
		case 3:		return (double)Sample.LanDownload;
		case 4:		return (double)Sample.ConnInbound + (double)Sample.ConnOutbound; // connection seconds, see UsageStats.h
		case 5:		return (double)Sample.ConnInbound;
		default:	return (double)Sample.ConnOutbound;
		}
	}
	return 0;
}

QString CUsageView::FormatCount(double Count) const
{
	return QString::number(Count, 'f', Count > 0 && Count < 10 ? 1 : 0);
}

double CUsageView::ToRate(double Raw, quint32 Seconds, int Curve) const
{
	if (Seconds == 0)
		return 0;
	if (m_Metric == EUsageMetric::eCpu) {
		if (Curve < 2) // ms of CPU time -> percent of all cores
			return Raw * 100.0 / ((double)Seconds * 1000.0 * m_Cores);
		return Raw * 100.0 / ((double)Seconds * 1000.0); // GPU/NPU ms are already normalized to one node
	}
	return Raw / (double)Seconds; // bytes/s
}

QString CUsageView::FormatRate(double Rate) const
{
	if (m_Metric == EUsageMetric::eCpu)
		return QString::number(Rate, 'f', Rate < 10 ? 2 : 1) + "%";
	return ::FormatRate((quint64)(Rate + 0.5));
}

QString CUsageView::FormatBreakdown(const SUsageSample& Sample, quint32 Seconds) const
{
	QVector<double> Raw(m_ValueCount);
	for (int c = 0; c < m_ValueCount; c++)
		Raw[c] = RawValue(Sample, c);
	return FormatBreakdown(Raw, Seconds);
}

QString CUsageView::FormatBreakdown(const QVector<double>& Raw, quint32 Seconds) const
{
	switch (m_Metric)
	{
	case EUsageMetric::eCpu:
	{
		QString Str = tr("%1 (kernel %2)").arg(FormatRate(ToRate(Raw[0], Seconds, 0))).arg(FormatRate(ToRate(Raw[1], Seconds, 1)));
		if (Raw[2] > 0) Str += tr(", GPU %1").arg(FormatRate(ToRate(Raw[2], Seconds, 2)));
		if (Raw[3] > 0) Str += tr(", NPU %1").arg(FormatRate(ToRate(Raw[3], Seconds, 3)));
		return Str;
	}
	case EUsageMetric::eIo:
		return tr("read %1, write %2, other %3").arg(FormatRate(ToRate(Raw[0], Seconds, 0))).arg(FormatRate(ToRate(Raw[1], Seconds, 1))).arg(FormatRate(ToRate(Raw[2], Seconds, 2)));
	case EUsageMetric::eNet:
	{
		QString Str = tr("up %1, down %2").arg(FormatRate(ToRate(Raw[0], Seconds, 0))).arg(FormatRate(ToRate(Raw[1], Seconds, 1)));
		if (!HideLan() && (Raw[2] > 0 || Raw[3] > 0))
			Str += tr(", LAN up %1, down %2").arg(FormatRate(ToRate(Raw[2], Seconds, 2))).arg(FormatRate(ToRate(Raw[3], Seconds, 3)));
		if (Raw.count() > 6 && Raw[4] > 0) // average open connections over the span
			Str += tr(", %1 connections (in %2, out %3)").arg(FormatCount(ToRate(Raw[4], Seconds, 4))).arg(FormatCount(ToRate(Raw[5], Seconds, 5))).arg(FormatCount(ToRate(Raw[6], Seconds, 6)));
		return Str;
	}
	}
	return QString();
}

double CUsageView::RankValue(const SUsageSample& Sample, int Focus) const
{
	//
	// What the tooltip ranks the programs by: the curve under the pointer, else CPU time
	// for the CPU graph and the sum of all curves for the I/O and traffic graphs.
	//
	if (Focus >= 0 && Focus < m_CurveCount)
		return RawValue(Sample, Focus);
	if (m_Metric == EUsageMetric::eCpu)
		return RawValue(Sample, 0);
	double Sum = 0;
	for (int c = 0; c < m_CurveCount; c++) {
		if (m_Metric == EUsageMetric::eNet && c >= 2 && c < 4 && HideLan())
			continue; // hidden lines do not count
		if (IsCountCurve(c))
			continue; // connections are not bytes
		Sum += RawValue(Sample, c);
	}
	return Sum;
}

QString CUsageView::RankName(int Focus) const
{
	switch (m_Metric)
	{
	case EUsageMetric::eCpu:
		switch (Focus) {
		case 1:		return tr("kernel time");
		case 2:		return tr("GPU");
		case 3:		return tr("NPU");
		default:	return tr("CPU");
		}
	case EUsageMetric::eIo:
		switch (Focus) {
		case 0:		return tr("read");
		case 1:		return tr("write");
		case 2:		return tr("other");
		default:	return tr("total I/O");
		}
	case EUsageMetric::eNet:
		switch (Focus) {
		case 0:		return tr("internet upload");
		case 1:		return tr("internet download");
		case 2:		return tr("LAN upload");
		case 3:		return tr("LAN download");
		case 4:		return tr("open connections");
		default:	return tr("total traffic");
		}
	}
	return QString();
}

//
// The tooltip label word wraps rich text at a width of its own choosing (QLabel's golden
// ratio heuristic), and Qt's table layout does not honor <nobr>, so lines broke at the slash
// in "KB/s". Text without any break opportunity cannot wrap: spaces, hyphens and slashes
// are replaced by their non breaking forms.
//
static QString NoWrap(const QString& Text)
{
	QString Str = Text.toHtmlEscaped();
	Str.replace(" ", "&nbsp;");
	Str.replace("-", "&#8209;");	// non breaking hyphen
	Str.replace("/", "/&#8288;");	// word joiner after the slash
	return Str;
}

QString CUsageView::IconHtml(const CProgramItemPtr& pItem)
{
	//
	// QToolTip renders rich text, and Qt's text document loads images from data URLs, so
	// the icon travels inside the tooltip string. Encoded once per item and kept.
	//
	quint64 UID = pItem->GetUID();
	auto I = m_IconCache.find(UID);
	if (I != m_IconCache.end())
		return I.value();

	QIcon Icon = pItem->GetIcon();
	if (Icon.isNull())
		Icon = pItem->DefaultIcon();
	QPixmap Pixmap = Icon.pixmap(16, 16);
	if (Pixmap.isNull())
		return QString(); // not cached, the icon may still be loading

	QByteArray Png;
	QBuffer Buffer(&Png);
	Buffer.open(QIODevice::WriteOnly);
	Pixmap.toImage().save(&Buffer, "PNG");

	QString Html = "<img src=\"data:image/png;base64," + QString::fromLatin1(Png.toBase64()) + "\" width=\"16\" height=\"16\">";
	if (m_IconCache.count() > 1000)
		m_IconCache.clear();
	m_IconCache.insert(UID, Html);
	return Html;
}

quint32 CUsageView::SlotSeconds(int Slot) const
{
	//
	// The last slot is still being filled: its rate is over the seconds elapsed so far,
	// not over the whole slot, so the end of the plot moves with every sample.
	//
	if (Slot + 1 < m_Slots)
		return m_SlotLen;
	qint64 SlotStart = m_From + (qint64)Slot * m_SlotLen;
	return (quint32)qBound<qint64>(1, m_Now - SlotStart + 1, m_SlotLen);
}

void CUsageView::Rebuild()
{
	quint32 Interval = UsageTierInterval(m_Tier);
	qint64 NowSec = QDateTime::currentSecsSinceEpoch();

	// the plot ends with the bucket that is currently being filled
	qint64 To = (NowSec / Interval + 1) * Interval;
	qint64 RangeSec = qMax<qint64>((qint64)(m_Range / 1000), Interval);
	qint64 RangeStart = qMax<qint64>(To - RangeSec, 0);

	// the plot starts with the first bucket inside the range, not with the range itself:
	// the span before the first sample has no data and would only be a flat line
	qint64 First = To;
	foreach(const CProgramItemPtr& pItem, m_Items)
	{
		SUsageHistory::STier Tier = pItem->GetUsage(m_Tier);
		int i = Tier.LowerBound((quint32)RangeStart);
		if (i < Tier.Buckets.count() && (qint64)Tier.Buckets[i].Time < First)
			First = Tier.Buckets[i].Time;
	}
	bool bNoData = First >= To;
	if (bNoData) // nothing at all, show the most recent minute(s) empty
		First = To - qMin<qint64>(RangeSec, 60 * Interval);
	qint64 Span = To - First;

	// several buckets per plotted slot when the span is long, the stored data keeps its resolution
	quint32 SlotLen = Interval;
	while (Span / SlotLen > USAGE_MAX_SLOTS)
		SlotLen *= 2;

	int Slots = (int)((Span + SlotLen - 1) / SlotLen);
	qint64 From = To - (qint64)Slots * SlotLen;

	//
	// Closed buckets never change, so between full re-sums (every few seconds, or when the
	// slot grid moved) only the slot being filled is summed again. That keeps the per second
	// refresh cheap for a month of history of all programs.
	//
	bool bFull = From != m_From || To != m_To || SlotLen != m_SlotLen || Slots != m_Slots
		|| m_Sums.count() != m_ValueCount || (NowSec - m_LastFull) * 1000 >= USAGE_FULL_REBUILD;

	m_From = From;
	m_To = To;
	m_Now = NowSec;
	m_SlotLen = SlotLen;
	m_Slots = Slots;
	m_bNoData = bNoData;

	if (bFull)
	{
		m_LastFull = NowSec;
		m_Sums.resize(m_ValueCount);
		for (int c = 0; c < m_ValueCount; c++) {
			m_Sums[c].resize(Slots);
			m_Sums[c].fill(0.0);
		}
	}

	qint64 SumFrom = bFull ? From : From + (qint64)(Slots - 1) * SlotLen;
	if (!bFull) {
		for (int c = 0; c < m_ValueCount; c++)
			m_Sums[c][Slots - 1] = 0.0;
	}

	foreach(const CProgramItemPtr& pItem, m_Items)
	{
		SUsageHistory::STier Tier = pItem->GetUsage(m_Tier); // a shallow copy, QVector is implicitly shared
		for (int i = Tier.LowerBound((quint32)SumFrom); i < Tier.Buckets.count(); i++)
		{
			const SUsageBucket& Bucket = Tier.Buckets[i];
			if ((qint64)Bucket.Time >= To)
				break;
			int Slot = (int)(((qint64)Bucket.Time - From) / SlotLen);
			if (Slot < 0 || Slot >= Slots)
				continue;
			for (int c = 0; c < m_ValueCount; c++)
				m_Sums[c][Slot] += RawValue(Bucket.Data, c);
		}
	}

	QVector<double> Total(m_ValueCount, 0.0);
	for (int c = m_CurveCount; c < m_ValueCount; c++) {
		for (int s = 0; s < Slots; s++)
			Total[c] += m_Sums[c][s];
	}
	for (int c = 0; c < m_CurveCount; c++)
	{
		QVector<QPointF> Points;
		Points.reserve(Slots + 1);
		double Last = 0;
		for (int s = 0; s < Slots; s++) {
			Total[c] += m_Sums[c][s];
			Last = ToRate(m_Sums[c][s], SlotSeconds(s), c);
			Points.append(QPointF((double)(From + (qint64)s * SlotLen) * 1000.0, Last));
		}
		// the open slot's value reaches up to now, so the line ends at the right edge
		if (Slots > 0)
			Points.append(QPointF((double)(NowSec + 1) * 1000.0, Last));
		m_pGraph->SetSamples(c, Points);

		// the GPU and NPU curves only show up on machines that have one and when it was used
		if (m_Metric == EUsageMetric::eCpu && c >= 2)
			m_pGraph->SetCurveVisible(c, Total[c] > 0);
		else if (m_Metric == EUsageMetric::eNet && c >= 2 && c < 4)
			m_pGraph->SetCurveVisible(c, !HideLan());
	}

	m_pGraph->SetTimeRange(From * 1000, (NowSec + 1) * 1000);
	m_pGraph->Replot();

	QString Info = tr("%1 item(s), %2 per point").arg(m_Items.count()).arg(SlotLen == 1 ? tr("1 second") : SlotLen < 3600 ? tr("%1 minute(s)").arg(SlotLen / 60) : tr("%1 hour(s)").arg(SlotLen / 3600));
	if (m_LastError == 0xC00000A3) // STATUS_DEVICE_NOT_READY: the service is still loading the stored history
		Info += tr(", the service is still loading the usage history");
	else if (m_LastError)
		Info += tr(", fetching the usage data failed: 0x%1").arg(m_LastError, 8, 16, QChar('0'));
	else if (bNoData)
		Info += tr(", no usage data for these items in this time range");
	else
		Info += tr(", average over the range: %1").arg(FormatBreakdown(Total, (quint32)qMax<qint64>(NowSec + 1 - From, 1)));
	m_pInfo->SetText(Info);
}

void CUsageView::OnHovered(qint64 TimeMs, const QPoint& GlobalPos, int Curve)
{
	qint64 Sec = TimeMs / 1000;
	if (m_Slots == 0 || Sec < m_From || Sec >= m_To) {
		OnHoverLeft();
		return;
	}

	int Slot = (int)((Sec - m_From) / m_SlotLen);
	quint32 SlotStart = (quint32)(m_From + (qint64)Slot * m_SlotLen);
	quint32 SlotEnd = SlotStart + m_SlotLen;
	quint32 Seconds = SlotSeconds(Slot);

	m_pGraph->SetMarker((qint64)SlotStart * 1000);

	QString Time = QDateTime::fromSecsSinceEpoch(SlotStart).toString("dd.MM.yyyy hh:mm:ss");
	if (m_SlotLen > 1)
		Time += " - " + QDateTime::fromSecsSinceEpoch(Seconds < m_SlotLen ? SlotStart + Seconds : SlotEnd).toString("hh:mm:ss");

	SUsageSample SlotTotal;
	struct SEntry {
		double Total = 0;
		QString Name;
		CProgramItemPtr pItem;
		SUsageSample Sum;
	};
	QVector<SEntry> Entries;
	Entries.reserve(m_Items.count());

	foreach(const CProgramItemPtr& pItem, m_Items)
	{
		SUsageHistory::STier Tier = pItem->GetUsage(m_Tier);
		SEntry Entry;
		for (int i = Tier.LowerBound(SlotStart); i < Tier.Buckets.count() && Tier.Buckets[i].Time < SlotEnd; i++)
			Entry.Sum.Add(Tier.Buckets[i].Data);
		Entry.Total = RankValue(Entry.Sum, Curve);
		if (Entry.Total <= 0)
			continue;
		Entry.Name = pItem->GetNameEx();
		Entry.pItem = pItem;
		SlotTotal.Add(Entry.Sum);
		Entries.append(Entry);
	}

	std::sort(Entries.begin(), Entries.end(), [](const SEntry& a, const SEntry& b) { return a.Total > b.Total; });

	// rich text, so the program icons can be shown next to the names
	QString Html = "<html><body><b>" + NoWrap(Time) + "</b><br>";
	Html += NoWrap(tr("Total: %1").arg(FormatBreakdown(SlotTotal, Seconds)));

	if (!Entries.isEmpty()) {
		Html += "<br><br>" + NoWrap(tr("Top %1 of %2 by %3:").arg(qMin(10, Entries.count())).arg(Entries.count()).arg(RankName(Curve)));
		Html += "<table cellspacing=\"0\" cellpadding=\"1\">";
		for (int i = 0; i < Entries.count() && i < 10; i++) {
			Html += "<tr><td align=\"right\">" + QString::number(i + 1) + ".</td>";
			Html += "<td>" + IconHtml(Entries[i].pItem) + "</td>";
			Html += "<td>" + NoWrap(Entries[i].Name) + "</td>";
			Html += "<td>&nbsp;" + NoWrap(FormatBreakdown(Entries[i].Sum, Seconds)) + "</td></tr>";
		}
		Html += "</table>";
	}
	Html += "</body></html>";

	QToolTip::showText(GlobalPos, Html, m_pGraph);
}

void CUsageView::OnHoverLeft()
{
	m_pGraph->SetMarker(-1);
	QToolTip::hideText();
}
