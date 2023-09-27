#include "VipPie.h"


VipPie::VipPie(vip_double start_angle,vip_double end_angle,vip_double min_radius,vip_double max_radius,vip_double offset_to_center)
:d_start_angle(start_angle),d_end_angle(end_angle),d_min_radius(min_radius),d_max_radius(max_radius), d_offset_to_center(offset_to_center)
{}

VipPie::VipPie(const VipPolarCoordinate & top_left, const VipPolarCoordinate & bottom_right,vip_double offset_to_center)
:d_start_angle(bottom_right.angle()),d_end_angle(top_left.angle()),d_min_radius(bottom_right.radius()),d_max_radius(top_left.radius()), d_offset_to_center(offset_to_center)
{}

VipPie::VipPie(const QRectF & r,vip_double offset_to_center)
:d_start_angle(r.left()),d_end_angle(r.right()),d_min_radius(r.top()),d_max_radius(r.bottom()) , d_offset_to_center(offset_to_center)
{}

bool VipPie::isEmpty() const
{
	return d_start_angle==0 && d_end_angle==0 && d_min_radius==0 && d_max_radius==0;
}

VipPie& VipPie::setAngleRange(vip_double start, vip_double end)
{
	d_start_angle = start;
	d_end_angle = end;
	return *this;
}

VipPie & VipPie::setStartAngle(vip_double start_angle)
{
	d_start_angle = start_angle;
	return *this;
}

vip_double VipPie::startAngle() const
{
	return d_start_angle;
}

VipPie & VipPie::setEndAngle(vip_double end_angle)
{
	d_end_angle = end_angle;
	return *this;
}

vip_double VipPie::endAngle() const
{
	return d_end_angle;
}

vip_double VipPie::sweepLength() const
{
	return d_end_angle - d_start_angle;
}

vip_double VipPie::meanAngle() const
{
	return (d_end_angle + d_start_angle)/2.0;
}

void VipPie::setRadiusRange(vip_double start, vip_double end)
{
	d_min_radius = start;
	d_max_radius = end;
}

VipPie & VipPie::setMinRadius(vip_double min_radius)
{
	d_min_radius = min_radius;
	return *this;
}

vip_double VipPie::minRadius() const
{
	return d_min_radius;
}

VipPie & VipPie::setMaxRadius(vip_double max_radius)
{
	d_max_radius = max_radius;
	return *this;
}

vip_double VipPie::maxRadius() const
{
	return d_max_radius;
}

vip_double VipPie::radiusExtent() const
{
	return d_max_radius - d_min_radius;
}

vip_double VipPie::meanRadius() const
{
	return (d_max_radius + d_min_radius)/2.0;
}

VipPie & VipPie::setOffsetToCenter(vip_double offset_to_center)
{
	d_offset_to_center = offset_to_center;
	return *this;
}

vip_double VipPie::offsetToCenter() const
{
	return d_offset_to_center;
}

VipPolarCoordinate VipPie::topLeft() const
{
	return VipPolarCoordinate(d_max_radius,d_end_angle);
}
VipPolarCoordinate VipPie::bottomRight() const
{
	return VipPolarCoordinate(d_min_radius,d_start_angle);
}

QRectF VipPie::rect() const
{
	return QRectF(QPointF(d_end_angle,d_max_radius),QPointF(d_start_angle,d_min_radius)).normalized();
}

VipPie VipPie::normalized() const
{
	VipPie res(*this);
	if(res.d_start_angle > res.d_end_angle)
		qSwap(res.d_start_angle,res.d_end_angle);
	if(res.d_min_radius > res.d_max_radius)
		qSwap(res.d_min_radius,res.d_max_radius);
	return res;
}

VipPie & VipPie::setMeanAngle(vip_double mean_angle)
{
	vip_double offset = mean_angle - meanAngle();
	d_start_angle += offset;
	d_end_angle += offset;
	return *this;
}

VipPie & VipPie::setMeanRadius(vip_double mean_radius)
{
	vip_double offset = mean_radius - meanRadius();
	d_min_radius += offset;
	d_max_radius += offset;
	return *this;
}



//make the the types declared with Q_DECLARE_METATYPE are registered
static int reg1 = qMetaTypeId<VipPie>();
static int reg2 = qMetaTypeId<VipPolarCoordinate>();
