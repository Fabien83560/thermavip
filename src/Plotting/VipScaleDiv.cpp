

#include "VipScaleDiv.h"
#include <qalgorithms.h>
#include <qmath.h>

/// Construct a division without ticks
///
/// \param lowerBound First boundary
/// \param upperBound Second boundary
///
/// \note lowerBound might be greater than upperBound for inverted scales
VipScaleDiv::VipScaleDiv( vip_double lowerBound, vip_double upperBound ):
    d_lowerBound( lowerBound ),
    d_upperBound( upperBound ),
    d_epsilon(0)
{
	computeEpsilon();
}

/// Construct a scale division
///
/// \param lowerBound First boundary
/// \param upperBound Second boundary
/// \param ticks List of major, medium and minor ticks
///
/// \note lowerBound might be greater than upperBound for inverted scales
VipScaleDiv::VipScaleDiv( vip_double lowerBound, vip_double upperBound,
        TickList ticks[NTickTypes] ):
    d_lowerBound( lowerBound ),
    d_upperBound( upperBound ),
    d_epsilon(0)
{
    for ( int i = 0; i < NTickTypes; i++ )
        d_ticks[i] = ticks[i];

    computeEpsilon();
}

VipScaleDiv::VipScaleDiv( const VipInterval & vipBounds,
          TickList ticks[NTickTypes] )
	:d_lowerBound( vipBounds.minValue() ),
    d_upperBound( vipBounds.maxValue() ),
    d_epsilon(0)
{
    for ( int i = 0; i < NTickTypes; i++ )
        d_ticks[i] = ticks[i];

    computeEpsilon();
}

/// Construct a scale division
///
/// \param lowerBound First boundary
/// \param upperBound Second boundary
/// \param minorTicks List of minor ticks
/// \param mediumTicks List medium ticks
/// \param majorTicks List of major ticks
///
/// \note lowerBound might be greater than upperBound for inverted scales
VipScaleDiv::VipScaleDiv( vip_double lowerBound, vip_double upperBound,
        const TickList &minorTicks,
        const TickList &mediumTicks,
        const TickList &majorTicks ):
    d_lowerBound( lowerBound ),
    d_upperBound( upperBound ),
    d_epsilon(0)
{
    d_ticks[ MinorTick ] = minorTicks;
    d_ticks[ MediumTick ] = mediumTicks;
    d_ticks[ MajorTick ] = majorTicks;

    computeEpsilon();
}

/// Change the interval
///
/// \param lowerBound First boundary
/// \param upperBound Second boundary
///
/// \note lowerBound might be greater than upperBound for inverted scales
void VipScaleDiv::setInterval( vip_double lowerBound, vip_double upperBound )
{
    d_lowerBound = lowerBound;
    d_upperBound = upperBound;
    computeEpsilon();
}

void VipScaleDiv::setInterval(const VipInterval & vipBounds)
{
    d_lowerBound = vipBounds.minValue();
    d_upperBound = vipBounds.maxValue();
    computeEpsilon();
}

/// Set the first boundary
///
/// \param lowerBound First boundary
/// \sa lowerBiound(), setUpperBound()
void VipScaleDiv::setLowerBound( vip_double lowerBound  )
{
    d_lowerBound = lowerBound;
    computeEpsilon();
}

/// Set the second boundary
///
/// \param upperBound Second boundary
/// \sa upperBound(), setLowerBound()
void VipScaleDiv::setUpperBound( vip_double upperBound  )
{
    d_upperBound = upperBound;
    computeEpsilon();
}


/// \brief Equality operator
/// \return true if this instance is equal to other
bool VipScaleDiv::operator==( const VipScaleDiv &other ) const
{
    if ( d_lowerBound != other.d_lowerBound ||
        d_upperBound != other.d_upperBound )
    {
        return false;
    }

    for ( int i = 0; i < NTickTypes; i++ )
    {
        if ( d_ticks[i] != other.d_ticks[i] )
            return false;
    }

    return true;
}

/// \brief Inequality
/// \return true if this instance is not equal to other
bool VipScaleDiv::operator!=( const VipScaleDiv &other ) const
{
    return ( !( *this == other ) );
}

/// Invert the scale division
/// \sa inverted()
void VipScaleDiv::invert()
{
    qSwap( d_lowerBound, d_upperBound );

    for ( int i = 0; i < NTickTypes; i++ )
    {
        TickList& ticks = d_ticks[i];

        const int size = ticks.count();
        const int size2 = size / 2;

        for ( int j = 0; j < size2; j++ )
            qSwap( ticks[j], ticks[size - 1 - j] );
    }
}

/// \return A scale division with inverted boundaries and ticks
/// \sa invert()
VipScaleDiv VipScaleDiv::inverted() const
{
    VipScaleDiv other = *this;
    other.invert();

    return other;
}

/// Return a scale division with an interval [lowerBound, upperBound]
/// where all ticks outside this interval are removed
///
/// \param lowerBound Lower bound
/// \param upperBound Upper bound
///
/// \return Scale division with all ticks inside of the given interval
///
/// \note lowerBound might be greater than upperBound for inverted scales
VipScaleDiv VipScaleDiv::bounded(
    vip_double lowerBound, vip_double upperBound ) const
{
    const vip_double min = qMin( lowerBound, upperBound );
    const vip_double max = qMax( lowerBound, upperBound );

    VipScaleDiv sd;
    sd.setInterval( lowerBound, upperBound );

    for ( int tickType = 0; tickType < VipScaleDiv::NTickTypes; tickType++ )
    {
        const TickList &ticks = d_ticks[ tickType ];

        TickList boundedTicks;
        for ( int i = 0; i < ticks.size(); i++ )
        {
            const vip_double tick = ticks[i];
            if ( tick >= min && tick <= max )
                boundedTicks += tick;
        }

        sd.setTicks( tickType, boundedTicks );
    }

    return sd;

}


void VipScaleDiv::computeEpsilon()
{
	//epsilon is considered as being a 1000th of the scale range
	d_epsilon = range() / 1000.0;
}


#include <QDataStream>

QDataStream & operator<<(QDataStream & stream, const VipScaleDiv & div)
{
	return stream <<div.lowerBound() <<div.upperBound()<<div.ticks(0)<<div.ticks(1)<<div.ticks(2);
}

QDataStream & operator>>(QDataStream & stream, VipScaleDiv & div)
{
	VipScaleDiv::TickList minor,medium,major;
	vip_double lower,upper;
	stream >>lower>>upper>>minor>>medium>>major;
	div = VipScaleDiv(lower,upper,minor,medium,major);
	return stream;
}


static int registerStreamOperators()
{
	qRegisterMetaTypeStreamOperators<VipScaleDiv>("VipScaleDiv");
	return 0;
}
static int _registerStreamOperators = registerStreamOperators();
