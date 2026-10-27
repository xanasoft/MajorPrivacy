#include "pch.h"
#include "UsageGraph.h"
#include "../../MiscHelpers/Common/Common.h"

#include <QWheelEvent>
#include <QStack>
#include <QMouseEvent>
#include <QPainter>
#include <cmath>

#include <qwt_plot.h>
#include <qwt_plot_canvas.h>
#include <qwt_plot_layout.h>
#include <qwt_plot_curve.h>
#include <qwt_plot_grid.h>
#include <qwt_plot_picker.h>
#include <qwt_plot_zoomer.h>
#include <qwt_plot_marker.h>
#include <qwt_picker_machine.h>
#include <qwt_legend.h>
#include <qwt_scale_draw.h>
#include <qwt_scale_engine.h>
#include <qwt_scale_map.h>
#include <qwt_interval.h>
#include <qwt_scale_widget.h>
#include <qwt_date_scale_draw.h>
#include <qwt_date_scale_engine.h>
#include <qwt_text.h>

#define USAGE_WHEEL_STEP		1.25	// y axis scaling per wheel notch
#define USAGE_HOVER_PIXELS		6		// how close (in pixels) the pointer has to be to a curve to be "on" it

//////////////////////////////////////////////////////////////////////////
// scale draws

class CBytesScaleDraw : public QwtScaleDraw
{
public:
	virtual QwtText label(double Value) const override
	{
		if (Value < 0) Value = 0;
		return FormatRate((quint64)(Value + 0.5), Value < 1024 ? 0 : 1);
	}
};

class CPercentScaleDraw : public QwtScaleDraw
{
public:
	virtual QwtText label(double Value) const override
	{
		return QString::number(Value, 'f', Value < 10 && Value != (int)Value ? 1 : 0) + "%";
	}
};

//////////////////////////////////////////////////////////////////////////
// CUsageZoomer
//
// Ported from Mops (GUI/MeasurementWidget.cpp): a rubber band zoomer with a history the
// toolbar buttons and the mouse gestures walk through. The bottom of the zoom stack is the
// full view, which CUsageGraph::UpdateBase replaces whenever the data or the time span changes.
//

class CUsageZoomer : public QwtPlotZoomer
{
public:
	CUsageZoomer(QWidget* pCanvas, CUsageGraph* pGraph)
		: QwtPlotZoomer(QwtAxis::XBottom, QwtAxis::YLeft, pCanvas, false), m_pGraph(pGraph)
	{
		setRubberBand(QwtPicker::RectRubberBand);
		setTrackerMode(QwtPicker::ActiveOnly);

		setMousePattern(QwtEventPattern::MouseSelect2, Qt::RightButton, Qt::ControlModifier);	// Ctrl+RightButton: back to the full view
		setMousePattern(QwtEventPattern::MouseSelect3, Qt::RightButton);						// RightButton: back one step
		setMousePattern(QwtEventPattern::MouseSelect6, Qt::RightButton, Qt::ShiftModifier);		// Shift+RightButton: forward one step
	}

	// Replace the bottom of the zoom stack, the full view, and keep the history above it.
	// setZoomStack only rescales when the current rectangle changed, so when the user is
	// zoomed in the axes stay where they are.
	void SetBase(const QRectF& Rect)
	{
		QStack<QRectF> Stack = zoomStack();
		if (Stack.isEmpty())
			Stack.push(Rect.normalized());
		else
			Stack[0] = Rect.normalized();
		setZoomStack(Stack, (int)zoomRectIndex());
	}

	// Replace the current rectangle without adding a history step (the wheel uses this).
	void SetCurrent(const QRectF& Rect)
	{
		QStack<QRectF> Stack = zoomStack();
		if (Stack.isEmpty())
			return;
		Stack[zoomRectIndex()] = Rect.normalized();
		setZoomStack(Stack, (int)zoomRectIndex());
	}

	void Reset(const QRectF& Rect)
	{
		QStack<QRectF> Stack;
		Stack.push(Rect.normalized());
		m_RightStack.clear();
		setZoomStack(Stack, 0);
		rescale(); // setZoomStack only rescales when the current rectangle changed, not when the axes moved
	}

	// setZoomStack does not rescale when the current rectangle is unchanged but the axes moved
	void Rescale() { rescale(); }

	//
	// The right axis: QwtPlotZoomer drives one y axis. Each history step remembers the right
	// axis range that went with its rectangle (none for the full view, which auto fits), so
	// walking the history restores both axes. Sets the scale only, the caller replots.
	//
	void UpdateRight()
	{
		if (!plot()->isAxisVisible(QwtAxis::YRight))
			return;
		int Index = (int)zoomRectIndex();
		if (Index < m_RightStack.count() && m_RightStack[Index].isValid())
			plot()->setAxisScale(QwtAxis::YRight, m_RightStack[Index].minValue(), m_RightStack[Index].maxValue());
		else
			plot()->setAxisScale(QwtAxis::YRight, 0.0, m_pGraph->AutoYMax(QwtAxis::YRight));
	}

	// a zoom rectangle (in left axis coordinates) zooms the right axis by the same pixel span
	virtual void zoom(const QRectF& Rect) override
	{
		QwtInterval Right;
		if (plot()->isAxisVisible(QwtAxis::YRight)) {
			QRectF Norm = Rect.normalized();
			QwtScaleMap Left = plot()->canvasMap(QwtAxis::YLeft);
			QwtScaleMap Map = plot()->canvasMap(QwtAxis::YRight);
			Right = QwtInterval(Map.invTransform(Left.transform(Norm.top())), Map.invTransform(Left.transform(Norm.bottom()))).normalized();
		}

		// placed before the base pushes, rescale() looks it up by the new index; taken back if nothing was pushed
		int Before = (int)zoomRectIndex();
		m_RightStack.resize(Before + 1);
		m_RightStack.append(Right);
		QwtPlotZoomer::zoom(Rect);
		if ((int)zoomRectIndex() == Before)
			m_RightStack.resize(Before + 1);
	}

	virtual void zoom(int Offset) override { QwtPlotZoomer::zoom(Offset); }

protected:
	virtual void rescale() override
	{
		UpdateRight();			// before the base, its replot then draws both axes
		QwtPlotZoomer::rescale();
	}

	virtual QwtText trackerTextF(const QPointF& Pos) const override
	{
		return QwtText(m_pGraph->FormatTime(Pos.x()) + "  " + m_pGraph->FormatValue(Pos.y()));
	}

	virtual bool eventFilter(QObject* pObject, QEvent* pEvent) override
	{
		if (pObject && pObject == parentWidget() && pEvent->type() == QEvent::MouseButtonDblClick)
		{
			QMouseEvent* pMouse = (QMouseEvent*)pEvent;
			if (pMouse->button() == Qt::LeftButton)
			{
				if (pMouse->modifiers() == Qt::ControlModifier)
					zoom(1);
				else if (pMouse->modifiers() == Qt::AltModifier)
					m_pGraph->ZoomFit();
				else
					zoom(-1);
				return false;
			}
		}
		return QwtPlotZoomer::eventFilter(pObject, pEvent);
	}

	CUsageGraph* m_pGraph;
	QVector<QwtInterval> m_RightStack;	// right axis range per zoom step, invalid = auto fit
};

//////////////////////////////////////////////////////////////////////////
// CUsageGraph

CUsageGraph::CUsageGraph(QWidget* parent)
	: QWidget(parent)
{
	QVBoxLayout* pLayout = new QVBoxLayout(this);
	pLayout->setContentsMargins(0, 0, 0, 0);

	m_pPlot = new QwtPlot(this);
	pLayout->addWidget(m_pPlot);

	QwtPlotCanvas* pCanvas = new QwtPlotCanvas();
	pCanvas->setFrameStyle(QFrame::NoFrame);
	pCanvas->setLineWidth(0);
	m_pPlot->setCanvas(pCanvas);
	pCanvas->setMouseTracking(true);
	pCanvas->installEventFilter(this);

	m_pPlot->plotLayout()->setAlignCanvasToScales(true);

	QFont SmallFont = font();
	SmallFont.setPointSizeF(SmallFont.pointSizeF() * 0.85);
	m_pPlot->setAxisFont(QwtAxis::XBottom, SmallFont);
	m_pPlot->setAxisFont(QwtAxis::YLeft, SmallFont);

	// the x axis shows local time
	QwtDateScaleDraw* pDateDraw = new QwtDateScaleDraw(Qt::LocalTime);
	pDateDraw->setDateFormat(QwtDate::Millisecond, "hh:mm:ss");
	pDateDraw->setDateFormat(QwtDate::Second, "hh:mm:ss");
	pDateDraw->setDateFormat(QwtDate::Minute, "hh:mm");
	pDateDraw->setDateFormat(QwtDate::Hour, "hh:mm");
	pDateDraw->setDateFormat(QwtDate::Day, "dd.MM hh:mm");
	pDateDraw->setDateFormat(QwtDate::Week, "dd.MM");
	pDateDraw->setDateFormat(QwtDate::Month, "dd.MM.yyyy");
	m_pPlot->setAxisScaleDraw(QwtAxis::XBottom, pDateDraw);
	m_pPlot->setAxisScaleEngine(QwtAxis::XBottom, new QwtDateScaleEngine(Qt::LocalTime));

	m_pGrid = new QwtPlotGrid();
	m_pGrid->enableXMin(false);
	m_pGrid->enableYMin(false);
	m_pGrid->attach(m_pPlot);

	m_pMarker = new QwtPlotMarker();
	m_pMarker->setLineStyle(QwtPlotMarker::VLine);
	m_pMarker->setVisible(false);
	m_pMarker->attach(m_pPlot);

	m_pPicker = new QwtPlotPicker(QwtAxis::XBottom, QwtAxis::YLeft, QwtPicker::NoRubberBand, QwtPicker::AlwaysOff, pCanvas);
	m_pPicker->setStateMachine(new QwtPickerTrackerMachine());
	connect(m_pPicker, qOverload<const QPointF&>(&QwtPlotPicker::moved), this, &CUsageGraph::OnMoved);

	m_pZoomer = new CUsageZoomer(pCanvas, this);
	connect(m_pZoomer, &QwtPlotZoomer::zoomed, this, [this](const QRectF&) { emit ZoomChanged(); });

	QwtLegend* pLegend = new QwtLegend();
	m_pPlot->insertLegend(pLegend, QwtPlot::BottomLegend);

	SetYUnit(eCount);
	ApplyPalette();
}

CUsageGraph::~CUsageGraph()
{
}

int CUsageGraph::AddCurve(const QString& Title, const QColor& Color, bool bFill, int Width, bool bRightAxis)
{
	QwtPlotCurve* pCurve = new QwtPlotCurve(Title);
	pCurve->setRenderHint(QwtPlotItem::RenderAntialiased);
	pCurve->setPen(QPen(Color, Width));
	if (bRightAxis)
	{
		pCurve->setYAxis(QwtAxis::YRight);
		if (!m_RightColor.isValid())
		{
			// a plain count scale, colored like its curve so it is clear which line it belongs to
			m_RightColor = Color;
			QwtLinearScaleEngine* pEngine = new QwtLinearScaleEngine();
			pEngine->setAttribute(QwtScaleEngine::IncludeReference, true);
			pEngine->setAttribute(QwtScaleEngine::Floating, false);
			pEngine->setReference(0.0);
			m_pPlot->setAxisScaleEngine(QwtAxis::YRight, pEngine);
			m_pPlot->setAxisScaleDraw(QwtAxis::YRight, new QwtScaleDraw());
			m_pPlot->setAxisScale(QwtAxis::YRight, 0.0, 1.0);
			QFont SmallFont = font();
			SmallFont.setPointSizeF(SmallFont.pointSizeF() * 0.85);
			m_pPlot->setAxisFont(QwtAxis::YRight, SmallFont);
			m_pPlot->setAxisVisible(QwtAxis::YRight, true);
			ApplyPalette();
		}
	}
	if (bFill) {
		QColor Fill = Color;
		Fill.setAlpha(60);
		pCurve->setBrush(Fill);
		pCurve->setBaseline(0.0);
	}
	pCurve->setLegendAttribute(QwtPlotCurve::LegendShowLine);
	pCurve->attach(m_pPlot);
	m_Curves.append(pCurve);
	return m_Curves.count() - 1;
}

void CUsageGraph::SetYUnit(EYUnit Unit)
{
	m_YUnit = Unit;

	QwtLinearScaleEngine* pEngine = new QwtLinearScaleEngine();
	pEngine->setAttribute(QwtScaleEngine::IncludeReference, true);
	pEngine->setAttribute(QwtScaleEngine::Floating, false);
	pEngine->setReference(0.0);
	m_pPlot->setAxisScaleEngine(QwtAxis::YLeft, pEngine);

	switch (Unit)
	{
	case ePercent:
		m_pPlot->setAxisScaleDraw(QwtAxis::YLeft, new CPercentScaleDraw());
		break;
	case eBytesPerSec:
		m_pPlot->setAxisScaleDraw(QwtAxis::YLeft, new CBytesScaleDraw());
		break;
	default:
		m_pPlot->setAxisScaleDraw(QwtAxis::YLeft, new QwtScaleDraw());
		break;
	}
	m_pPlot->setAxisScale(QwtAxis::YLeft, 0.0, Unit == ePercent ? 100.0 : 1.0);

	QFont SmallFont = font();
	SmallFont.setPointSizeF(SmallFont.pointSizeF() * 0.85);
	m_pPlot->setAxisFont(QwtAxis::YLeft, SmallFont);

	m_YMaxOverride = 0;
	UpdateBase();
}

void CUsageGraph::SetTimeRange(qint64 FromMs, qint64 ToMs)
{
	m_FromMs = FromMs;
	m_ToMs = ToMs;
	// the base is updated with the next replot, when the samples are in as well
}

void CUsageGraph::SetSamples(int Curve, const QVector<QPointF>& Points)
{
	if (Curve < 0 || Curve >= m_Curves.count())
		return;
	m_Curves[Curve]->setSamples(Points);
}

void CUsageGraph::SetCurveVisible(int Curve, bool bVisible)
{
	if (Curve < 0 || Curve >= m_Curves.count())
		return;
	QwtPlotCurve* pCurve = m_Curves[Curve];
	if (pCurve->isVisible() == bVisible && pCurve->testItemAttribute(QwtPlotItem::Legend) == bVisible)
		return;
	pCurve->setItemAttribute(QwtPlotItem::Legend, bVisible);
	pCurve->setVisible(bVisible);
	if (bVisible) // the legend only picks up an item again when it is told about it
		pCurve->legendChanged();
}

void CUsageGraph::SetMarker(qint64 TimeMs)
{
	if (TimeMs < 0) {
		if (!m_pMarker->isVisible())
			return;
		m_pMarker->setVisible(false);
	}
	else {
		m_pMarker->setXValue((double)TimeMs);
		m_pMarker->setVisible(true);
	}
	Replot();
}

void CUsageGraph::Replot()
{
	// several SetSamples calls in a row end up as one replot
	if (m_bReplotPending)
		return;
	m_bReplotPending = true;
	QTimer::singleShot(0, this, SLOT(OnReplot()));
}

void CUsageGraph::OnReplot()
{
	m_bReplotPending = false;
	UpdateBase();
	m_pZoomer->UpdateRight(); // the full view follows the data on the right as well
	m_pPlot->replot();
}

//////////////////////////////////////////////////////////////////////////
// zoom

double CUsageGraph::AutoYMax(int Axis) const
{
	if (Axis == QwtAxis::YLeft && m_YUnit == ePercent)
		return 100.0;

	double DataMax = 0;
	foreach(QwtPlotCurve* pCurve, m_Curves) {
		if (pCurve->yAxis() == Axis && pCurve->dataSize() > 0)
			DataMax = qMax(DataMax, pCurve->maxYValue());
	}

	// let the scale engine round the data maximum up to a tick, as auto scaling would
	double y1 = 0, y2 = qMax(DataMax, 1.0), Step = 0;
	if (const QwtScaleEngine* pEngine = m_pPlot->axisScaleEngine(Axis))
		pEngine->autoScale(8, y1, y2, Step);
	return y2 > 0 ? y2 : 1.0;
}

void CUsageGraph::UpdateBase()
{
	//
	// The full view is the owner's time span by the data's y extent (or the y extent the
	// wheel set). It is the bottom of the zoom stack; what the user zoomed on top of it stays.
	//
	if (m_ToMs <= m_FromMs)
		return;

	double YMax = m_YMaxOverride > 0 ? m_YMaxOverride : AutoYMax();
	QRectF Base((double)m_FromMs, 0.0, (double)(m_ToMs - m_FromMs), YMax);

	const QStack<QRectF>& Stack = m_pZoomer->zoomStack();
	if (!Stack.isEmpty() && Stack[0] == Base)
		return;

	bool bWasZoomed = IsZoomed();
	m_pZoomer->SetBase(Base);
	if (bWasZoomed != IsZoomed())
		emit ZoomChanged();
}

void CUsageGraph::ScaleY(double Factor)
{
	const QStack<QRectF>& Stack = m_pZoomer->zoomStack();
	if (Stack.isEmpty() || Factor <= 0)
		return;

	QRectF Rect = m_pZoomer->zoomRect();
	double Height = Rect.height() * Factor;

	// stay within a sensible range of the data
	double Auto = AutoYMax();
	double MinHeight = m_YUnit == ePercent ? 0.1 : 1.0;
	double MaxHeight = m_YUnit == ePercent ? 100.0 : Auto * 100.0;
	Height = qBound(MinHeight, Height, qMax(MinHeight, MaxHeight));
	if (Height == Rect.height())
		return;

	if (m_pZoomer->zoomRectIndex() == 0)
	{
		// the full view: remember the y extent so the next data update keeps it
		m_YMaxOverride = Height;
		if (m_YUnit == ePercent ? Height >= 100.0 : Height >= Auto)
			m_YMaxOverride = 0; // back to automatic
		UpdateBase();
		m_pZoomer->Rescale();
	}
	else
	{
		// a zoomed view: scale the current rectangle from its bottom, within the full view
		Rect.setHeight(Height);
		Rect = Rect.intersected(Stack[0]);
		if (Rect.isEmpty())
			return;
		m_pZoomer->SetCurrent(Rect);
	}
	emit ZoomChanged();
}

bool CUsageGraph::CanZoomIn() const
{
	return m_pZoomer->zoomRectIndex() + 1 < (uint)m_pZoomer->zoomStack().count();
}

bool CUsageGraph::CanZoomOut() const
{
	return m_pZoomer->zoomRectIndex() > 0;
}

bool CUsageGraph::IsZoomed() const
{
	return m_pZoomer->zoomRectIndex() > 0 || m_YMaxOverride > 0;
}

void CUsageGraph::ZoomIn()
{
	m_pZoomer->zoom(1);
}

void CUsageGraph::ZoomOut()
{
	m_pZoomer->zoom(-1);
}

void CUsageGraph::ZoomFit()
{
	m_YMaxOverride = 0;
	if (m_ToMs <= m_FromMs)
		return;
	m_pZoomer->Reset(QRectF((double)m_FromMs, 0.0, (double)(m_ToMs - m_FromMs), AutoYMax()));
	emit ZoomChanged();
}

//////////////////////////////////////////////////////////////////////////
// formatting

QString CUsageGraph::FormatTime(double TimeMs) const
{
	return QDateTime::fromMSecsSinceEpoch((qint64)TimeMs).toString("dd.MM hh:mm:ss");
}

QString CUsageGraph::FormatValue(double Value) const
{
	if (Value < 0) Value = 0;
	switch (m_YUnit)
	{
	case ePercent:		return QString::number(Value, 'f', Value < 10 ? 2 : 1) + "%";
	case eBytesPerSec:	return FormatRate((quint64)(Value + 0.5));
	default:			return QString::number(Value, 'f', 0);
	}
}

QIcon CUsageGraph::MakeZoomIcon(EZoomIcon Kind, const QColor& Ink)
{
	//
	// The resources hold no zoom icons, so the zoom toolbar draws its own (from Mops): a
	// magnifier with +/-, and corner brackets around a trace for fit all.
	//
	QPixmap Pixmap(16, 16);
	Pixmap.fill(Qt::transparent);
	QPainter Painter(&Pixmap);
	Painter.setRenderHint(QPainter::Antialiasing);

	if (Kind == eZoomFit)
	{
		Painter.setPen(QPen(Ink, 1.5, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));
		const double a = 1.5, b = 14.5, l = 4;
		Painter.drawPolyline(QPolygonF() << QPointF(a, a + l) << QPointF(a, a) << QPointF(a + l, a));
		Painter.drawPolyline(QPolygonF() << QPointF(b - l, a) << QPointF(b, a) << QPointF(b, a + l));
		Painter.drawPolyline(QPolygonF() << QPointF(b, b - l) << QPointF(b, b) << QPointF(b - l, b));
		Painter.drawPolyline(QPolygonF() << QPointF(a + l, b) << QPointF(a, b) << QPointF(a, b - l));
		Painter.setPen(QPen(Ink, 1.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
		Painter.drawPolyline(QPolygonF() << QPointF(4, 9) << QPointF(6, 9) << QPointF(7.5, 5) << QPointF(9, 11) << QPointF(10, 9) << QPointF(12, 9));
	}
	else
	{
		Painter.setPen(QPen(Ink, 1.5));
		Painter.drawEllipse(QRectF(1.5, 1.5, 9, 9));
		Painter.setPen(QPen(Ink, 2.5, Qt::SolidLine, Qt::RoundCap));
		Painter.drawLine(QPointF(9.5, 9.5), QPointF(14, 14));
		Painter.setPen(QPen(Ink, 1.4, Qt::SolidLine, Qt::FlatCap));
		Painter.drawLine(QPointF(3.5, 6), QPointF(8.5, 6));
		if (Kind == eZoomIn)
			Painter.drawLine(QPointF(6, 3.5), QPointF(6, 8.5));
	}
	return QIcon(Pixmap);
}

//////////////////////////////////////////////////////////////////////////
// events

// the value of a curve at x, linearly interpolated between its samples
static bool CurveValueAt(const QwtPlotCurve* pCurve, double x, double& y)
{
	size_t Count = pCurve->dataSize();
	if (Count == 0)
		return false;

	// the first sample with an x at or after the position
	size_t Low = 0, High = Count;
	while (Low < High) {
		size_t Mid = Low + (High - Low) / 2;
		if (pCurve->sample(Mid).x() < x)
			Low = Mid + 1;
		else
			High = Mid;
	}
	if (Low >= Count) // behind the last sample
		return false;

	QPointF B = pCurve->sample(Low);
	if (Low == 0) {
		if (x < B.x())
			return false; // before the first sample
		y = B.y();
		return true;
	}
	QPointF A = pCurve->sample(Low - 1);
	double t = B.x() > A.x() ? (x - A.x()) / (B.x() - A.x()) : 0.0;
	y = A.y() + t * (B.y() - A.y());
	return true;
}

int CUsageGraph::CurveAt(const QPointF& Pos) const
{
	//
	// Which curve is under the pointer. The vertical distance at the pointer's x alone is not
	// enough: with a point every pixel or two the traffic lines zigzag through a whole band and
	// a spike crosses everything on its way up, so some line always happened to pass within a
	// few pixels of the pointer even when it plainly sat on another one. So a curve has to be
	// close at the pointer's x to qualify, and among those the one that stays closest over a
	// small window left and right of it wins: the line the pointer follows, not the one that
	// crosses it. The position comes in left axis coordinates, every curve is measured in
	// pixels through its own axis map, so right axis curves work the same.
	//
	static const double Offsets[] = { -8.0, -4.0, 0.0, 4.0, 8.0 }; // pixels
	const int Center = 2;

	QwtScaleMap xMap = m_pPlot->canvasMap(QwtAxis::XBottom);
	double PixelX = xMap.transform(Pos.x());
	double PixelY = m_pPlot->canvasMap(QwtAxis::YLeft).transform(Pos.y());

	int Best = -1;
	double BestScore = 0;
	for (int c = 0; c < m_Curves.count(); c++)
	{
		const QwtPlotCurve* pCurve = m_Curves[c];
		if (!pCurve->isVisible())
			continue;
		QwtScaleMap yMap = m_pPlot->canvasMap(pCurve->yAxis());

		double Score = 0;
		int Count = 0;
		bool bQualifies = false;
		for (int i = 0; i < (int)(sizeof(Offsets) / sizeof(Offsets[0])); i++)
		{
			double y;
			if (!CurveValueAt(pCurve, xMap.invTransform(PixelX + Offsets[i]), y))
				continue;
			double Dist = qAbs(yMap.transform(y) - PixelY);
			if (i == Center)
				bQualifies = Dist <= USAGE_HOVER_PIXELS;
			Score += Dist;
			Count++;
		}
		if (!bQualifies || Count == 0)
			continue;
		Score /= Count;
		if (Best < 0 || Score < BestScore) {
			BestScore = Score;
			Best = c;
		}
	}
	return Best;
}

void CUsageGraph::OnMoved(const QPointF& Pos)
{
	if (m_pZoomer->isActive()) // no tooltip while a zoom rectangle is being dragged
		return;
	emit Hovered((qint64)Pos.x(), QCursor::pos(), CurveAt(Pos));
}

bool CUsageGraph::eventFilter(QObject* pObject, QEvent* pEvent)
{
	if (pObject == m_pPlot->canvas())
	{
		if (pEvent->type() == QEvent::Leave)
			emit HoverLeft();
		else if (pEvent->type() == QEvent::Wheel)
		{
			// the wheel scales the y axis only, the time span is pinned to the toolbar combo
			QWheelEvent* pWheel = (QWheelEvent*)pEvent;
			int Delta = pWheel->angleDelta().y();
			if (Delta != 0) {
				emit HoverLeft();
				ScaleY(pow(USAGE_WHEEL_STEP, -Delta / 120.0)); // wheel up zooms in
			}
			return true;
		}
	}
	return QWidget::eventFilter(pObject, pEvent);
}

void CUsageGraph::changeEvent(QEvent* pEvent)
{
	if (pEvent->type() == QEvent::PaletteChange || pEvent->type() == QEvent::StyleChange)
		ApplyPalette();
	QWidget::changeEvent(pEvent);
}

void CUsageGraph::ApplyPalette()
{
	//
	// Colors come from the widget palette, so the plot follows the light and dark theme
	// without knowing about either.
	//
	QPalette Pal = palette();
	QColor Text = Pal.color(QPalette::Text);
	QColor Base = Pal.color(QPalette::Base);

	m_pPlot->setCanvasBackground(Base);
	m_pPlot->setAutoFillBackground(false);

	QColor GridColor = Text;
	GridColor.setAlpha(50);
	m_pGrid->setMajorPen(QPen(GridColor, 0, Qt::DotLine));

	QColor MarkerColor = Text;
	MarkerColor.setAlpha(160);
	m_pMarker->setLinePen(QPen(MarkerColor, 0, Qt::DashLine));

	m_pZoomer->setRubberBandPen(QPen(Pal.color(QPalette::Highlight), 1, Qt::DashLine));
	m_pZoomer->setTrackerPen(QPen(Text));

	for (int i = (int)QwtAxis::YLeft; i < QwtAxis::AxisPositions; i++) {
		QwtScaleWidget* pAxis = m_pPlot->axisWidget((QwtAxis::Position)i);
		if (!pAxis) continue;
		QColor AxisText = i == QwtAxis::YRight && m_RightColor.isValid() ? m_RightColor : Text;
		QPalette AxisPal = pAxis->palette();
		AxisPal.setColor(QPalette::WindowText, AxisText);
		AxisPal.setColor(QPalette::Text, AxisText);
		pAxis->setPalette(AxisPal);
	}

	Replot();
}
