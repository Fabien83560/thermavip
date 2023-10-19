#ifndef VIP_DOUBLE_SLIDER_H
#define VIP_DOUBLE_SLIDER_H

#include "VipAxisBase.h"

/// \addtogroup Plotting
/// @{

class VipSliderGrip;

/// Class representing an axis item with a slider grip.
class VIP_PLOTTING_EXPORT VipDoubleSlider : public VipAxisBase
{
	Q_OBJECT
	Q_PROPERTY(bool singleStepEnabled READ singleStepEnabled WRITE setSingleStepEnabled)
	Q_PROPERTY(bool scaleVisible READ scaleVisible WRITE setScaleVisible)
	Q_PROPERTY(bool sliderEnabled READ isSliderEnabled WRITE setSliderEnabled)
	Q_PROPERTY(bool mouseClickEnabled READ isMouseClickEnabled WRITE setMouseClickEnabled)
	Q_PROPERTY(double value READ value WRITE setValue)
	Q_PROPERTY(double lineWidth READ lineWidth WRITE setLineWidth)
public:
	VipDoubleSlider(Alignment pos, QGraphicsItem* parent = 0);
	virtual ~VipDoubleSlider();

	/// @brief Set alignment (left, right, top, bottom)
	virtual void setAlignment(Alignment align);

	/// @brief Set the distance between the slider bar (area drawn close to the axis backbone on which the grip is positioned)
	/// and the axis backbone.
	void setSliderWidth(double width);
	double sliderWidth() const;
	QRectF sliderRect() const;

	/// @brief Returns the grip object
	VipSliderGrip* grip() const;

	/// @brief Returns the current value
	double value() const;

	/// @brief Enable/disable single step
	void setSingleStepEnabled(bool);
	bool singleStepEnabled() const;

	/// @brief Set the single step value and an associated reference.
	/// If reference is set to Vip::InvalidValue, the reference will be internally set to the axis minimum value.
	void setSingleStep(double, double reference = Vip::InvalidValue);
	double singleStep() const;

	/// @brief Enable/disable slider bar drawing
	bool isSliderEnabled() const;

	/// @brief Set the bow style to draw the slider bar
	void setLineBoxStyle(const VipBoxStyle&);
	const VipBoxStyle& lineBoxStyle() const;
	VipBoxStyle& lineBoxStyle();

	/// @brief Get/Set the slider bar width (area drawn close to the axis backbone on which the grip is positioned)
	double lineWidth() const;

	bool scaleVisible() const;

	bool isMouseClickEnabled() const;

	void divideAxisScale(vip_double min, vip_double max, vip_double stepSize = 0.);

	virtual void draw(QPainter* painter, QWidget* widget = 0);
	virtual double extentForLength(double length) const;

public Q_SLOTS:

	void setSliderEnabled(bool on);
	void setLineWidth(double);
	void setScaleVisible(bool);
	void setMouseClickEnabled(bool);
	/// @brief Set the grip value
	void setValue(double);

Q_SIGNALS:
	/// @brief Emitted when the grip value changed
	void valueChanged(double);

protected:
	void drawSlider(QPainter* painter, const QRectF& rect) const;
	virtual void itemGeometryChanged(const QRectF&);
	virtual double additionalSpace() const;
	virtual void mousePressEvent(QGraphicsSceneMouseEvent* event);

private Q_SLOTS:

	void scaleDivHasChanged();
	void gripValueChanged(double);

private:
	QRectF sliderRect(const QRectF& rect) const;

	double closestValue(double);

	class PrivateData;
	PrivateData* d_data;
};

/// Widget representing a vertical or horizontal slider with an axis.
class VIP_PLOTTING_EXPORT VipDoubleSliderWidget : public VipScaleWidget
{
	Q_OBJECT
	Q_PROPERTY(bool singleStepEnabled READ singleStepEnabled WRITE setSingleStepEnabled)
	Q_PROPERTY(double value READ value WRITE setValue)
public:
	VipDoubleSliderWidget(VipBorderItem::Alignment align, QWidget* parent = NULL);

	/// @brief Set axis alignment
	void setAlignment(VipBorderItem::Alignment align);
	VipBorderItem::Alignment alignment() const;

	/// @brief Set the axis and slider range
	void setRange(double min, double max, double stepSize = 0);

	double minValue() const;
	double maxValue() const;
	double value() const;

	void setSingleStepEnabled(bool);
	bool singleStepEnabled() const;

	void setSingleStep(double, double reference = Vip::InvalidValue);
	double singleStep() const;

	VipDoubleSlider* slider() const;

protected:
	virtual void onResize();
	virtual void showEvent(QShowEvent*);

public Q_SLOTS:

	void setValue(double);

private Q_SLOTS:

	void handleValueChanged(double);

Q_SIGNALS:

	void valueChanged(double);
};

/// @}
// end Plotting

#endif
