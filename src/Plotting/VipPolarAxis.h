#ifndef VIS_POLAR_AXIS_H
#define VIS_POLAR_AXIS_H

#include "VipAbstractScale.h"
#include "VipScaleDraw.h"

/// \addtogroup Plotting
/// @{


class VIP_PLOTTING_EXPORT VipAbstractPolarScale : public VipAbstractScale
{
	Q_OBJECT

public:

	VipAbstractPolarScale(QGraphicsItem * parent = NULL);

	void setOuterRect(const QRectF &);
	QRectF outerRect() const;

	VipBoxStyle & axisBoxStyle();
	const VipBoxStyle & axisBoxStyle() const;
	void setAxisBoxStyle(const VipBoxStyle &);

	virtual QRectF axisRect() const = 0;

	virtual void setCenter(const QPointF& center) = 0;
	virtual QPointF center() const = 0;

private:

	QRectF d_outerRect;
	VipBoxStyle d_style;
};


class VIP_PLOTTING_EXPORT VipPolarAxis : public VipAbstractPolarScale
{
	Q_OBJECT

protected:

	virtual void setScaleDraw( VipPolarScaleDraw * );
	virtual bool hasState(const QByteArray& state, bool enable) const;

public:

	VipPolarAxis(QGraphicsItem * parent = NULL);
	~VipPolarAxis();

	virtual QPainterPath shape() const;

	virtual void	draw ( QPainter * painter, QWidget * widget = 0 ) ;

	virtual void computeGeometry(bool compute_intersection_geometry = true);

	virtual const VipPolarScaleDraw * constScaleDraw() const;
	virtual VipPolarScaleDraw * scaleDraw();

	virtual QRectF axisRect() const;

	void setAdditionalRadius(double radius);
	double additionalRadius() const;

	void setCenterProximity(int );
	int centerProximity() const;

	virtual void getBorderDistHint( double &start, double &end ) const;
	double minRadius() const;
	double maxRadius() const;
	double radiusExtent() const {return maxRadius() - minRadius();}

	void setCenter(const QPointF & center);
	QPointF center() const;

	void setRadius(double radius);
	void setMaxRadius(double max_radius);
	void setMinRadius(double min_radius);
	double radius() const;

	void setStartAngle(double start);
	double startAngle() const;

	void setEndAngle(double end);
	double endAngle() const;

	double sweepLength() const;

	virtual void layoutScale();

private:

	void getBorderRadius(double & min_radius, double & max_radius) const;
	void computeScaleDrawRadiusAndCenter();

	class PrivateData;
	PrivateData * d_data;
};




class VIP_PLOTTING_EXPORT VipRadialAxis : public VipAbstractPolarScale
{
	Q_OBJECT

protected:

	virtual void setScaleDraw( VipRadialScaleDraw * );
	bool hasState(const QByteArray& state, bool enable) const;

public:

	VipRadialAxis(QGraphicsItem * parent = NULL);
	~VipRadialAxis();

	virtual QPainterPath shape() const;

	virtual void	draw ( QPainter * painter, QWidget * widget = 0 ) ;

	virtual void computeGeometry(bool compute_intersection_geometry = true);

	virtual const VipRadialScaleDraw * constScaleDraw() const;
	virtual VipRadialScaleDraw * scaleDraw();

	virtual void getBorderDistHint( double &start, double &end ) const;

	void setCenter(const QPointF & center);
	QPointF center() const;

	void setRadiusRange(double start_radius, double end_radius);
	void setStartRadius(double start_radius, VipPolarAxis * axis = NULL );
	void setEndRadius(double end_radius, VipPolarAxis * axis = NULL);
	double startRadius() const;
	double endRadius() const;

	void setAngle(double start, VipPolarAxis * axis = NULL, Vip::ValueType type = Vip::Relative);
	double angle() const;

	virtual void layoutScale();
	virtual QRectF axisRect() const;

private:

	class PrivateData;
	PrivateData * d_data;
};


/// @}
//end Plotting

#endif