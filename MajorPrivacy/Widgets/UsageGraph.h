#pragma once
#include <QWidget>

class QwtPlot;
class QwtPlotCurve;
class QwtPlotGrid;
class QwtPlotPicker;
class QwtPlotMarker;
class CUsageZoomer;

//
// A time series plot (Qwt) with a date axis, a set of curves, a hover picker and a zoomer.
// The widget only draws; what the curves mean is up to the owner (see CUsageView).
//
// Zooming: left-drag selects a rectangle, right-click goes back one step, Ctrl+right-click back
// to the full view, Shift+right-click forward again; double-click gestures do the same
// (plain = back, Ctrl = forward, Alt = fit all). The mouse wheel scales the y axis only,
// the time span stays as it is.
//
// Curves added with bRightAxis go on a second y axis on the right, a plain count scale that
// fits its curves by default. A zoom rectangle zooms both axes together (the right one by the
// same pixel span), the wheel only scales the left axis, and the history and fit restore both.
//

class CUsageGraph : public QWidget
{
	Q_OBJECT
public:
	enum EYUnit
	{
		eCount = 0,
		ePercent,
		eBytesPerSec
	};

	enum EZoomIcon
	{
		eZoomIn = 0,
		eZoomOut,
		eZoomFit
	};

	CUsageGraph(QWidget* parent = nullptr);
	virtual ~CUsageGraph();

	int						AddCurve(const QString& Title, const QColor& Color, bool bFill = false, int Width = 1, bool bRightAxis = false);
	void					SetYUnit(EYUnit Unit);

	// x values are milliseconds since epoch
	void					SetTimeRange(qint64 FromMs, qint64 ToMs);
	void					SetSamples(int Curve, const QVector<QPointF>& Points);
	void					SetCurveVisible(int Curve, bool bVisible); // hidden curves leave the legend as well
	void					SetMarker(qint64 TimeMs); // < 0 hides the marker
	void					Replot();

	// zoom history, see CUsageZoomer
	bool					CanZoomIn() const;
	bool					CanZoomOut() const;
	bool					IsZoomed() const;	// anything but the full view, including a wheel-scaled y axis
	void					ZoomIn();			// forward one step in the zoom history
	void					ZoomOut();			// back one step in the zoom history
	void					ZoomFit();			// drop the zoom history and the wheel scaling, show everything

	QString					FormatTime(double TimeMs) const;
	QString					FormatValue(double Value) const;

	// 16x16 icons for a zoom toolbar, drawn in the given color
	static QIcon			MakeZoomIcon(EZoomIcon Kind, const QColor& Color);

signals:
	void					Hovered(qint64 TimeMs, const QPoint& GlobalPos, int Curve); // Curve: the one under the pointer, -1 for none
	void					HoverLeft();
	void					ZoomChanged();

protected:
	friend class CUsageZoomer;

	virtual bool			eventFilter(QObject* pObject, QEvent* pEvent) override;
	virtual void			changeEvent(QEvent* pEvent) override;

	void					ApplyPalette();
	void					UpdateBase();
	int						CurveAt(const QPointF& Pos) const;
	void					ScaleY(double Factor);
	double					AutoYMax(int Axis = 0) const;	// QwtAxis::YLeft = 0, QwtAxis::YRight = 1

private slots:
	void					OnMoved(const QPointF& Pos);
	void					OnReplot();

private:
	QwtPlot*				m_pPlot = nullptr;
	QwtPlotGrid*			m_pGrid = nullptr;
	QwtPlotPicker*			m_pPicker = nullptr;
	QwtPlotMarker*			m_pMarker = nullptr;
	CUsageZoomer*			m_pZoomer = nullptr;
	QList<QwtPlotCurve*>	m_Curves;
	EYUnit					m_YUnit = eCount;
	QColor					m_RightColor;		// the right axis takes the color of its (first) curve, invalid = no right axis
	bool					m_bReplotPending = false;

	// the full view: the owner's time span and the data's y extent, unless the wheel set one
	qint64					m_FromMs = 0;
	qint64					m_ToMs = 0;
	double					m_YMaxOverride = 0;	// 0 = automatic
};
