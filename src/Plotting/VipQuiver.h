#ifndef QUIVER_H
#define QUIVER_H

#include <QVector2D>
#include <QPolygonF>
#include <QLineF>
#include <QMetaType>
#include <QPen>
#include <QBrush>

#include "VipGlobals.h"

/// \addtogroup Plotting
/// @{

/// @brief Class representing an arrow based on a start position and a 2d vector
class VIP_PLOTTING_EXPORT VipQuiver
{
	QPointF m_origin;
	QVector2D m_vector;

public:
	/// @brief Construct from origin and vector
	VipQuiver(const QPointF & origin = QPointF(), const QVector2D vector = QVector2D());
	/// @brief Construct from origin and end points
	VipQuiver(const QPointF & p1, const QPointF & p2);
	/// @brief Construct from line
	VipQuiver(const QLineF & line);

	const QPointF & origin() const;
	const QVector2D & vector() const;
	QLineF line() const;
	QPointF p1() const;
	QPointF p2() const;
	double length() const;

	void setOrigin(const QPointF&);
	void setVector(const QVector2D&);
	void setLine(const QLineF&);

	/// @brief Draw the arrow
	/// @param painter QPainter object
	/// @param extremityPen array od 2 QPen: one for the origin symbol (if any) and one for the destination symbol (if any)
	/// @param extremityBrush array od 2 QBrush: one for the origin symbol (if any) and one for the destination symbol (if any)
	/// @param angles array of 2 angles in degrees for the origin and destination arrow symbols (if any)
	/// @param lengths array of 2 lengths for the origin and destination symbols (if any)
	/// @param style combination of VipQuiverPath::QuiverStyle
	/// @return Actual line size taking into account the drawn symbols
	QLineF draw(QPainter* painter, const QPen * extremityPen, const QBrush * extremityBrush,  const double * angles, const double * lengths, int style = 0);
};

/// @brief Class used to draw a polyline starting/ending with a symbol: square, circle or arrow
class VIP_PLOTTING_EXPORT VipQuiverPath
{
public:

	/// @brief Extremity (start/end)
	enum Extremity
	{
		Start = 0,
		End = 1
	};
	/// @brief Style of the VipQuiverPath (start and end symbols)
	enum QuiverStyle
	{
		Line = 0,
		StartArrow = 0x01,
		StartSquare = 0x02,
		StartCircle = 0x04,
		EndArrow = 0x08,
		EndSquare = 0x10,
		EndCircle = 0x20
	};
	//! VipQuiver styles
	typedef QFlags<QuiverStyle> QuiverStyles;

	VipQuiverPath();

	void setPen(const QPen & );
	const QPen & pen() const;

	void setExtremityPen(Extremity, const QPen & );
	const QPen & extremetyPen(Extremity) const;

	void setExtremityBrush(Extremity, const QBrush & );
	const QBrush & extremetyBrush(Extremity) const;

	/// @brief Set the color off all pen/brush used
	void setColor(const QColor &);

	void setStyle(const QuiverStyles & );
	QuiverStyles style() const;

	void setAngle(Extremity, double);
	double angle(Extremity) const;

	void setLength(Extremity ,double);
	double length(Extremity ) const;

	void setVisible(bool);
	bool isVisible() const;

	/// @brief Draw a line using this VipQuiverPath parameters
	/// @return additional start and end length due to drawn symbols
	QPair<double,double> draw(QPainter * , const QLineF& ) const;
	/// @brief Draw a polyline using this VipQuiverPath parameters
	/// @return additional start and end length due to drawn symbols
	QPair<double,double> draw(QPainter * , const QPolygonF& ) const;
	/// @brief Draw a polyline using this VipQuiverPath parameters
	/// @return additional start and end length due to drawn symbols
	QPair<double,double> draw(QPainter * , const QPointF*, int ) const;

private:

	QBrush d_extremityBrush[2];
	QPen d_extremityPen[2];
	QPen d_pathPen;

	double d_angles[2];
	double d_lengths[2];
	QuiverStyles d_style;
	bool d_visible;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(VipQuiverPath::QuiverStyles);
Q_DECLARE_METATYPE(VipQuiver);
Q_DECLARE_METATYPE(VipQuiverPath);

/// @}
//end Plotting

#endif
