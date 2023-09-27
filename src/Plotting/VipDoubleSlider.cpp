#include "VipDoubleSlider.h"
#include "VipSliderGrip.h"
#include "VipPainter.h"
#include "VipPlotItem.h"



class DoubleSliderGrip : public VipSliderGrip
{
public:

	DoubleSliderGrip(VipDoubleSlider * parent)
	:VipSliderGrip(parent)
	{
		this->setValue(0);
	}

	VipDoubleSlider * axis() const
	{
		return const_cast<VipDoubleSlider*>(static_cast<const VipDoubleSlider*>(scale()));
	}

protected:

	virtual double handleDistance() const
	{
		if(axis()->orientation() == Qt::Vertical)
		{
			return qAbs(static_cast<const VipDoubleSlider*>(this->axis())->sliderRect().center().x() - this->axis()->constScaleDraw()->pos().x() );
		}
		else
		{
			return qAbs(static_cast<const VipDoubleSlider*>(this->axis())->sliderRect().center().y() - this->axis()->constScaleDraw()->pos().y() );
		}
	}
};


class VipDoubleSlider::PrivateData
{
public:

	bool isEnabled;
	double width;
	VipBoxStyle lineBoxStyle;
	double lineWidth;
	bool singleStepEnabled;
	double singleStep;
	double value;
	VipSliderGrip * grip;

	PrivateData()
	:isEnabled(true),width(15),lineWidth(10),singleStepEnabled(false),singleStep(1),grip(NULL)
	{
		VipAdaptativeGradient grad(QGradientStops()<<QGradientStop(0,QColor(0xBDBDBD))<<QGradientStop(1,QColor(0xDBDBDB)),Qt::Vertical);
		lineBoxStyle.setBorderRadius(2);
		lineBoxStyle.setRoundedCorners(Vip::AllCorners);
		lineBoxStyle.setAdaptativeGradientBrush(grad);
		lineBoxStyle.setBorderPen(QPen(Qt::NoPen));
	}
};

VipDoubleSlider::VipDoubleSlider(Alignment pos, QGraphicsItem * parent )
:VipAxisBase(pos,parent)
{
	d_data = new PrivateData();
	d_data->grip = new DoubleSliderGrip(this);

	VipInterval interval  = this->scaleDiv().bounds();
	d_data->grip->setValue(interval.minValue());

	this->scaleDraw()->setTicksPosition(VipScaleDraw::TicksInside);
	this->setRenderHints(QPainter::TextAntialiasing | QPainter::HighQualityAntialiasing);
	this->setBorderDist(5,5);
	this->setUseBorderDistHintForLayout(true);

	connect(d_data->grip,SIGNAL(valueChanged(double)),this,SLOT(gripValueChanged(double)),Qt::DirectConnection);
	connect(this,SIGNAL(scaleDivChanged(bool)),this,SLOT(scaleDivHasChanged()));
}

VipDoubleSlider::~VipDoubleSlider()
{
	delete d_data;
}

void VipDoubleSlider::setAlignment( Alignment align)
{
	d_data->grip->setImage(QImage());
	VipAxisBase::setAlignment(align);
}

void VipDoubleSlider::setValue(double v)
{
	d_data->grip->setValue(v);
}

void VipDoubleSlider::scaleDivHasChanged()
{
	//smart grip update
	VipInterval interval = scaleDiv().bounds().normalized();
	if(grip()->value() < interval.minValue())
		grip()->setValue(interval.minValue());
	else if(grip()->value() > interval.maxValue())
		grip()->setValue(interval.maxValue());
}

double VipDoubleSlider::additionalSpace() const
{
	if ( d_data->isEnabled )
	{
		return d_data->width;
	}
	else
		return 0.;
}

void VipDoubleSlider::setSingleStepEnabled(bool enable)
{
	d_data->grip->setSingleStepEnabled(enable);
}

bool VipDoubleSlider::singleStepEnabled() const
{
	return d_data->grip->singleStepEnabled();
}

void VipDoubleSlider::setSingleStep(double singleStep, double reference)
{
	d_data->grip->setSingleStep(singleStep,reference);
}

double VipDoubleSlider::singleStep() const
{
	return d_data->grip->singleStep();
}

void VipDoubleSlider::setLineBoxStyle(const VipBoxStyle & style)
{
	d_data->lineBoxStyle = style;
	update();
}
const VipBoxStyle & VipDoubleSlider::lineBoxStyle() const
{
	return d_data->lineBoxStyle;
}

VipBoxStyle & VipDoubleSlider::lineBoxStyle()
{
	return d_data->lineBoxStyle;
}

void VipDoubleSlider::setLineWidth(double w)
{
	d_data->lineWidth = w;
	update();
}

double VipDoubleSlider::lineWidth() const
{
	return d_data->lineWidth;
}

void VipDoubleSlider::setScaleVisible(bool visible)
{
	scaleDraw()->setComponents(visible * VipScaleDraw::AllComponents);
	update();
}

bool VipDoubleSlider::scaleVisible() const
{
	return constScaleDraw()->components() == VipScaleDraw::AllComponents;
}

void VipDoubleSlider::divideAxisScale(vip_double min, vip_double max, vip_double stepSize)
{
	this->scaleEngine()->autoScale(this->maxMajor(),min,max,stepSize);
	this->setScaleDiv(this->scaleEngine()->divideScale(min,max,this->maxMajor(),this->maxMinor(),stepSize));
}


void VipDoubleSlider::drawSlider( QPainter *painter, const QRectF& rect ) const
{
    double half_width = d_data->lineWidth/2.0;
    QRectF line_rect;
    if(orientation() == Qt::Vertical)
    {
    	line_rect = QRectF(QPointF(rect.center().x()-half_width,rect.top()) , QPointF(rect.center().x()+half_width,rect.bottom()));
    }
    else
    {
    	line_rect = QRectF(QPointF(rect.left(),rect.center().y()-half_width) , QPointF(rect.right(),rect.center().y()+half_width));
    }

    painter->setRenderHint(QPainter::Antialiasing);
    d_data->lineBoxStyle.computeRect(line_rect);
    d_data->lineBoxStyle.draw(painter);
}

double VipDoubleSlider::extentForLength(double length) const
{
	//return d_data->length;
	return  VipAxisBase::extentForLength( length);
}

void VipDoubleSlider::itemGeometryChanged(const QRectF & r)
{
	VipAxisBase::itemGeometryChanged(r);

	//reset grip positions
	d_data->grip->setValue(d_data->grip->value());
}

void VipDoubleSlider::draw ( QPainter * painter, QWidget * widget  )
{
	VipAxisBase::draw(painter,widget);

	if ( d_data->isEnabled && d_data->width > 0 )
	{
		drawSlider( painter, sliderRect() );
	}
}



QRectF VipDoubleSlider::sliderRect() const
{
	return sliderRect(this->boundingRectNoCorners());
}

QRectF VipDoubleSlider::sliderRect( const QRectF& rect ) const
{
    QRectF cr = rect;
    double margin = this->margin();
    //double borderDist[2];
    // borderDist[0] = this->startBorderDist();
    // borderDist[1] = this->endBorderDist();

    if ( constScaleDraw()->orientation() == Qt::Horizontal )
    {
        cr.setLeft( this->constScaleDraw()->pos().x() );////cr.left() + borderDist[0] );
        cr.setWidth( this->constScaleDraw()->length() );//cr.width() - borderDist[1] + 1 );
    }
    else
    {
        cr.setTop( this->constScaleDraw()->pos().y() );//cr.top() + borderDist[0] );
        cr.setHeight( this->constScaleDraw()->length() );//cr.height() - borderDist[1] + 1 );
    }

    switch ( this->alignment() )
    {
        case Left:
        {
            cr.setLeft( cr.right() - margin
                - d_data->width );
            cr.setWidth( d_data->width );
            break;
        }

        case Right:
        {
            cr.setLeft( cr.left() + margin );
            cr.setWidth( d_data->width );
            break;
        }

        case Bottom:
        {
            cr.setTop( cr.top() + margin );
            cr.setHeight( d_data->width );
            break;
        }

        case Top:
        {
            cr.setTop( cr.bottom() - margin
                - d_data->width );
            cr.setHeight( d_data->width );
            break;
        }
    }

    return cr;
}


void VipDoubleSlider::setSliderEnabled( bool on )
{
    if ( on != d_data->isEnabled )
    {
        d_data->isEnabled = on;
        layoutScale();
    }
}


bool VipDoubleSlider::isSliderEnabled() const
{
    return d_data->isEnabled;
}

void VipDoubleSlider::setSliderWidth( double width )
{
    if ( width != d_data->width )
    {
        d_data->width = width;
        if ( isSliderEnabled() )
            layoutScale();
    }
}


double VipDoubleSlider::sliderWidth() const
{
    return d_data->width;
}

VipSliderGrip * VipDoubleSlider::grip() const
{
	return const_cast<VipSliderGrip*>(d_data->grip);
}

double VipDoubleSlider::value() const
{
	return d_data->grip->value();
}

void VipDoubleSlider::gripValueChanged(double value)
{
	Q_UNUSED(value)
	Q_EMIT valueChanged(d_data->grip->value());
}






VipDoubleSliderWidget::VipDoubleSliderWidget(VipBorderItem::Alignment align, QWidget * parent)
:VipScaleWidget(new VipDoubleSlider(align),parent)
{
	setAlignment(align);
	slider()->setMinBorderDist(10,10);
	slider()->setSliderWidth(10);
	slider()->setLineWidth(5);
	slider()->setMargin(5);
	connect(slider(),SIGNAL(valueChanged(double)),this,SLOT(handleValueChanged(double)),Qt::DirectConnection);
}

void VipDoubleSliderWidget::setAlignment(VipBorderItem::Alignment align)
{
	slider()->setAlignment(align);
	onResize();
}

void VipDoubleSliderWidget::onResize()
{
	if(slider()->orientation() == Qt::Vertical)
		this->setMinimumWidth(slider()->extentForLength(0));
	else
		this->setMinimumHeight(slider()->extentForLength(0));
}

void VipDoubleSliderWidget::showEvent(QShowEvent*)
{
	slider()->grip()->updatePosition();
}

VipBorderItem::Alignment VipDoubleSliderWidget::alignment() const
{
	return slider()->alignment();
}

void VipDoubleSliderWidget::setRange(double min, double max, double stepSize)
{
	slider()->setScale(min,max,stepSize);
	slider()->grip()->setValue(min);
}

//void VipDoubleSliderWidget::divideRange(double min, double max, double stepSize)
// {
// slider()->divideAxisScale(min,max,stepSize);
// }

double VipDoubleSliderWidget::minValue() const
{
	return slider()->scaleDiv().bounds().minValue();
}

double VipDoubleSliderWidget::maxValue() const
{
	return slider()->scaleDiv().bounds().minValue();
}

void VipDoubleSliderWidget::setSingleStepEnabled(bool enable)
{
	slider()->setSingleStepEnabled(enable);
}

bool VipDoubleSliderWidget::singleStepEnabled() const
{
	return slider()->singleStepEnabled();
}

void VipDoubleSliderWidget::setSingleStep(double singleStep, double reference)
{
	slider()->setSingleStep(singleStep,reference);
}

double VipDoubleSliderWidget::singleStep() const
{
	return slider()->singleStep();
}

VipDoubleSlider * VipDoubleSliderWidget::slider() const
{
	return static_cast<VipDoubleSlider*>(scale());
}

void VipDoubleSliderWidget::setValue(double v)
{
	slider()->setValue(v);
}

void VipDoubleSliderWidget::handleValueChanged(double v)
{
	Q_EMIT valueChanged(v);
}
