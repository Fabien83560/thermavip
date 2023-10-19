#include "VipGui.h"
#include "VipUniqueId.h"
#include "VipCore.h"
#include "VipDisplayArea.h"
#include "VipXmlArchive.h"
#include "VipLegendItem.h"
#include "VipEnvironment.h"
#include "VipPlotWidget2D.h"
#include "VipLogging.h"
#include "VipIODevice.h"
#include "VipPlayer.h"
#include "VipProgress.h"
#include "VipStandardProcessing.h"

#include <qapplication.h>



QBrush vipWidgetTextBrush(QWidget * w)
{
	if (w)
		return w->palette().text();
	else
		return qApp->palette().text();
}

QColor vipDefaultTextErrorColor(QWidget * w)
{
	QColor c = vipWidgetTextBrush(w).color();
	if (c == Qt::black)
		return Qt::red;
	else if (c == Qt::white)
		return QColor(0xFF3D3D);//QColor(Qt::red).lighter(120);
	else
		return Qt::red;
}

QStringList vipAvailableSkins()
{
	QString skin = "skins";
	if (!QDir(skin).exists())
		skin = "../" + skin;
	if (QDir(skin).exists())
	{
		QStringList res = QDir(skin).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
		res.removeAll("standard_skin");
		return res;
	}
	return QStringList();
}

#include <QDesktopWidget>
#include <QApplication>

bool vipLoadSkin(const QString & skin_name)
{
	vip_debug("skin: %s\n", skin_name.toLatin1().data());
	QString skin = "skins/" + skin_name;
	if (!QDir(skin).exists())
	{
		skin = "../" + skin;
		vip_debug("cannot read skin dir, set dir to '%s'\n", skin.toLatin1().data());
	}
	if (!QDir(skin).exists())
	{
		skin = QFileInfo(vipAppCanonicalPath()).canonicalPath() + "/skins/" + skin_name;
		vip_debug("cannot read skin dir, set dir to '%s'\n", skin.toLatin1().data());
	}

	if (QDir(skin).exists())
	{

		QFile file(skin + "/stylesheet.css");
		if (file.open(QFile::ReadOnly))
		{
			vip_debug("skin file: '%s'\n", QFileInfo(skin + "/stylesheet.css").canonicalFilePath().toLatin1().data());

			//read skin
			QString sk = file.readAll();
#if !defined(_WIN32) && (defined(__unix__) || defined(__unix) || (defined(__APPLE__) && defined(__MACH__)))
			//on linux, we might need to change the fonts sizes based on the screen size
			//QRect desktop = QApplication::desktop()->screenGeometry();
//
			// //compute the new font size for text editors
			// int size = (1080.0/desktop.height()) * 9.5;
			// if( size > 9) size =size -1;

			//int size = 12;
			// sk += "\nQPlainTextEdit{font-size: "+ QString::number(size) + "pt;}"
			// "\nQTextEdit{font-size: "+ QString::number(size) + "pt;}";
#endif

			vipAddFrontIconPath(skin + "/icons");
			qApp->setStyleSheet(sk);
			
			
			{
				// Now, read the plot_stylesheet.css file
				QFile fin(skin + "/plot_stylesheet.css");
				if (fin.open(QFile::ReadOnly)) {
					vip_debug("plot skin fin: '%s'\n", QFileInfo(skin + "/plot_stylesheet.css").canonicalFilePath().toLatin1().data());

					// read skin
					QString _sk = fin.readAll();
					VipGlobalStyleSheet::setStyleSheet(_sk);

					return true;
				}
				else
					vip_debug("cannot open skin fin '%s'\n", (skin + "/plot_stylesheet.css").toLatin1().data());
			}
		}
		else
		{
			vip_debug("cannot open skin file '%s'\n", (skin + "/stylesheet.css").toLatin1().data());
		}

		
	}
	else
	{
		vip_debug("cannot read '%s'\n", skin.toLatin1().data());
	}

	return false;
}


static int _restart_delay = -1;

void vipSetRestartEnabled(int delay_ms)
{
	_restart_delay = delay_ms;
}
void vipDisableRestart()
{
	_restart_delay = -1;
}
bool vipIsRestartEnabled()
{
	return _restart_delay >= 0;
}
int vipRestartMSecs()
{
	return _restart_delay;
}


std::function<QString(const QString&, const QString &)> _query;
void vipSetQueryFunction(const std::function<QString(const QString&, const QString &)> & fun)
{
	_query = fun;
}
std::function<QString(const QString&, const QString &)> vipQueryFunction()
{
	return _query;
}




QImage vipRemoveColoredBorder(const QImage& img, const QColor& c, int border )
{
	// Helper function that removes the transparent border of an image while keeping at least border pixels

	const QImage im = (img.format() != QImage::Format_ARGB32) ? img.convertToFormat(QImage::Format_ARGB32) : img;
	const uint* pix = (const uint*)im.constBits();

	int minx = im.width();
	int miny = im.height();
	int maxx = -1;
	int maxy = -1;
	int h = im.height();
	int w = im.width();
	uint color = c.rgba();

	for (int y = 0; y < h; ++y)
		for (int x = 0; x < w; ++x) {
			uint p = pix[x + y * w];
			if (p != color) {
				minx = std::min(minx, x);
				maxx = std::max(maxx, x);
				miny = std::min(miny, y);
				maxy = std::max(maxy, y);
			}
		}

	minx -= border;
	miny -= border;
	maxx += border + 1;
	maxy += border + 1;


	if (minx < 0) minx = 0;
	if (miny < 0) miny = 0;
	if (maxx > w) maxx = w;
	if (maxy > h) maxy = h;

	QRect r(minx, miny, maxx - minx, maxy - miny);
	if (!r.isValid())
		return img;

	return img.copy(r);
}
QPixmap vipRemoveColoredBorder(const QPixmap& img, const QColor& c, int border )
{
	return QPixmap::fromImage(vipRemoveColoredBorder(img.toImage(), c, border));
}





#include <qsharedmemory.h>
#include <qbuffer.h>
class VipFileSharedMemory::PrivateData
{
public:
	QSharedMemory file_memory;
};

VipFileSharedMemory::VipFileSharedMemory()
{
	m_data = new PrivateData();
}

VipFileSharedMemory::~VipFileSharedMemory()
{
	delete m_data;
}

VipFileSharedMemory & VipFileSharedMemory::instance()
{
	static VipFileSharedMemory inst;
	return inst;
}

bool VipFileSharedMemory::addFilesToOpen(const QStringList & lst, bool new_workspace)
{
	if (!m_data->file_memory.isAttached())
	{
		m_data->file_memory.setKey("Thermavip_Files");
		if (!m_data->file_memory.attach())
		{
			if (!m_data->file_memory.create(200000))
				return false;
			m_data->file_memory.lock();
			memset(m_data->file_memory.data(), 0, m_data->file_memory.size());
			m_data->file_memory.unlock();
		}
	}

	QByteArray ar;
	QDataStream stream(&ar, QIODevice::WriteOnly);
	stream << new_workspace;
	stream << lst;

	if (!m_data->file_memory.lock())
		return false;

	int size = ar.size();
	memcpy(m_data->file_memory.data(), &size, sizeof(int));
	memcpy((char*)m_data->file_memory.data() + sizeof(int), ar.data(), ar.size());
	m_data->file_memory.unlock();
	return true;
}


QStringList VipFileSharedMemory::retrieveFilesToOpen(bool * new_workspace)
{
	if (!m_data->file_memory.isAttached())
	{
		m_data->file_memory.setKey("Thermavip_Files");
		if (!m_data->file_memory.attach())
		{
			if (!m_data->file_memory.create(200000))
				return QStringList();
			m_data->file_memory.lock();
			memset(m_data->file_memory.data(), 0, m_data->file_memory.size());
			m_data->file_memory.unlock();
		}
	}

	if (!m_data->file_memory.lock())
		return QStringList();

	int size = 0;
	memcpy(&size, m_data->file_memory.data(), sizeof(int));
	if (!size)
	{
		m_data->file_memory.unlock();
		return QStringList();
	}

	QByteArray ar = QByteArray::fromRawData((char*)m_data->file_memory.data() + sizeof(int), m_data->file_memory.size() - sizeof(int));
	QDataStream stream(ar);
	QStringList res;
	bool nw = false;
	stream >> nw;
	stream >> res;
	memset(m_data->file_memory.data(), 0, m_data->file_memory.size());

	m_data->file_memory.unlock();

	if (new_workspace)
		*new_workspace = nw;
	return res;
}


bool VipFileSharedMemory::hasThermavipInstance()
{
	if (!m_data->file_memory.isAttached())
	{
		m_data->file_memory.setKey("Thermavip_Files");
		return m_data->file_memory.attach();
	}
	return true;
}


#include "VipPlayer.h"
#include "VipColorMap.h"

static bool copyPath(QString src, QString dst)
{
	QDir dir(src);
	if (!dir.exists())
		return false;

	foreach(QString d, dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
		QString dst_path = dst + "/" + d;
		QDir().mkpath(dst_path);
		if (!copyPath(src + "/" + d, dst_path))
			return false;
	}

	foreach(QString f, dir.entryList(QDir::Files)) {
		QString srcfile = src + "/" + f;
		QString dstfile = dst + "/" + f;
		if (QFileInfo(dstfile).exists())
			if (!QFile::remove(dstfile))
				return false;

		if (!QFile::copy(srcfile, dstfile))
			return false;
	}
	return true;
}


class VipGuiDisplayParamaters::PrivateData
{
public:
	QFont editorFont;
	int itemPaletteFactor;
	bool videoPlayerShowAxis;
	bool displayTimeOffset;
	bool showPlayerToolBar;
	bool showTimeMarkerAlways;
	bool globalColorScale;
	int flatHistogramStrength;
	int videoRenderingStrategy;
	int plotRenderingStrategy;
	int renderingThreads;
	bool displayExactPixels;
	bool dirty;
	bool setAndApply;
	VipLinearColorMap::StandardColorMap playerColorScale;
	Vip::PlayerLegendPosition legendPosition;
	VipValueToTime::DisplayType displayType;
	QPointer<VipPlotWidget2D> resetPlotWidget;
	QPointer<VipPlotWidget2D> defaultPlotWidget;
	QPointer<VipPlotArea2D> defaultArea;
	QPointer<VipPlotCurve> defaultCurve;

	QSharedPointer<VipTextStyle> titleTextStyle;
	QSharedPointer<VipTextStyle> defaultTextStyle;

	QPen shapePen;
	QBrush shapeBrush;
	VipPlotShape::DrawComponents shapeComponents;
};

VipGuiDisplayParamaters::VipGuiDisplayParamaters(VipMainWindow * win)
{
	m_data = new PrivateData();
	m_data->setAndApply = true;

	m_data->shapePen = QPen(QColor(Qt::black),1);
	m_data->shapeBrush = QBrush(QColor(255, 0, 0, 70));
	m_data->shapeComponents = VipPlotShape::Background | VipPlotShape::Border | VipPlotShape::Id;

	m_data->itemPaletteFactor = 0;
	m_data->playerColorScale = VipLinearColorMap::Jet;
	m_data->videoPlayerShowAxis = true;
	m_data->displayTimeOffset = false;
	m_data->legendPosition = Vip::LegendBottom;
	m_data->showPlayerToolBar = true;
	m_data->showTimeMarkerAlways = false;
	m_data->globalColorScale = false;
	m_data->flatHistogramStrength = 1;
	m_data->dirty = false;
	m_data->displayType = VipValueToTime::Double;

	m_data->videoRenderingStrategy = VipBaseGraphicsView::Raster;
	m_data->plotRenderingStrategy = VipBaseGraphicsView::Raster;
	
	m_data->displayExactPixels = false;

	m_data->defaultPlotWidget = new VipPlotWidget2D(win);
	m_data->defaultPlotWidget->hide();
	m_data->resetPlotWidget = new VipPlotWidget2D(win);
	m_data->resetPlotWidget->hide();
	m_data->defaultArea = m_data->defaultPlotWidget->area();//new VipPlotArea2D();
	m_data->defaultArea->setVisible(true);
	m_data->defaultArea->grid()->setVisible(false);
	//m_data->defaultArea->titleAxis()->setVisible(true);
	m_data->defaultCurve = new VipPlotCurve();
	m_data->defaultCurve->setPen(QPen(Qt::blue,1.5));
	m_data->defaultCurve->setBrush(QBrush(QColor(0, 0, 255, 200), Qt::NoBrush));
	m_data->defaultCurve->setRawData(VipPointVector() << QPointF(3, 3) << QPointF(6, 6) << QPointF(9, 4) << QPointF(12, 7));
	VipSymbol * s = new VipSymbol();
	s->setSize(QSizeF(9, 9));
	s->setStyle(VipSymbol::Ellipse);
	s->setBrush(QBrush(Qt::blue));
	s->setPen(QPen(QColor(Qt::blue).darker(120)));
	m_data->defaultCurve->setSymbol(s);

#ifdef Q_OS_WIN
	m_data->editorFont.setFixedPitch(true);
	m_data->editorFont.setFamily("Consolas");
	m_data->editorFont.setPointSize(10);
#else
	//Use a font embeded within Thermavip
	m_data->editorFont.setFixedPitch(true);
	m_data->editorFont.setFamily("Inconsolata");
	m_data->editorFont.setPointSize(13);
#endif

	connect(this, SIGNAL(changed()), this, SLOT(delaySaveToFile()), Qt::QueuedConnection);

	//use the one in Thermavip installation if more recent
	QFileInfo current = vipGetDataDirectory() + "gui_settings.xml";
	QString apppath = QFileInfo(vipAppCanonicalPath()).canonicalPath();
	apppath.replace("\\", "/");
	if (!apppath.endsWith("/")) apppath += "/";
	QFileInfo thermavip = apppath + "gui_settings.xml";
	//printf("current: %s\n", current.canonicalFilePath().toLatin1().data());
	//printf("thermavip: %s, %s\n", thermavip.canonicalFilePath().toLatin1().data(), (apppath + "gui_settings.xml").toLatin1().data());
	if (thermavip.exists() && (!current.exists() || current.lastModified() < thermavip.lastModified())) {
		if (current.exists()) {
			if (!QFile::remove(current.canonicalFilePath()))
				return;
		}
		QFile::copy(thermavip.canonicalFilePath(), current.canonicalFilePath());
	}

	m_data->setAndApply = false;
	restore();
	m_data->setAndApply = true;
}

VipGuiDisplayParamaters::~VipGuiDisplayParamaters()
{
	if (m_data->defaultArea)
		delete m_data->defaultArea.data();
	if (m_data->defaultCurve)
		delete m_data->defaultCurve.data();
	delete m_data;
}

VipGuiDisplayParamaters * VipGuiDisplayParamaters::instance(VipMainWindow * win)
{
	static VipGuiDisplayParamaters *inst = new VipGuiDisplayParamaters(win);
	return inst;
}

QPen VipGuiDisplayParamaters::shapeBorderPen()
{
	return m_data->shapePen;
}
QBrush VipGuiDisplayParamaters::shapeBackgroundBrush()
{
	return m_data->shapeBrush;
}
VipPlotShape::DrawComponents VipGuiDisplayParamaters::shapeDrawComponents()
{
	return m_data->shapeComponents;
}

void VipGuiDisplayParamaters::setShapeBorderPen(const QPen & pen)
{
	m_data->shapePen = pen;
	if (m_data->setAndApply) {

		QList<VipPlayer2D*> players = VipUniqueId::objects<VipPlayer2D>();
		for (int i = 0; i < players.size(); ++i) {
			QList<VipPlotSceneModel*> models = players[i]->plotSceneModels();
			for (int j = 0; j < models.size(); ++j) {
				models[j]->setPen("All", pen);
			}
		}
	}
	emitChanged();
}
void VipGuiDisplayParamaters::setShapeBackgroundBrush(const QBrush & brush)
{
	m_data->shapeBrush = brush;
	if (m_data->setAndApply) {
		QList<VipPlayer2D*> players = VipUniqueId::objects<VipPlayer2D>();
		for (int i = 0; i < players.size(); ++i) {
			QList<VipPlotSceneModel*> models = players[i]->plotSceneModels();
			for (int j = 0; j < models.size(); ++j) {
				models[j]->setBrush("All", brush);
			}
		}
	}
	emitChanged();
}
void VipGuiDisplayParamaters::setShapeDrawComponents(const VipPlotShape::DrawComponents & c)
{
	m_data->shapeComponents = c;
	if (m_data->setAndApply) {
		QList<VipPlayer2D*> players = VipUniqueId::objects<VipPlayer2D>();
		for (int i = 0; i < players.size(); ++i) {
			QList<VipPlotSceneModel*> models = players[i]->plotSceneModels();
			for (int j = 0; j < models.size(); ++j) {
				models[j]->setDrawComponents("All", c);
			}
		}
	}
	emitChanged();
}

int VipGuiDisplayParamaters::itemPaletteFactor() const
{
	return m_data->itemPaletteFactor;
}
void VipGuiDisplayParamaters::setItemPaletteFactor(int factor)
{
	m_data->itemPaletteFactor = factor;
	
	// retrieve the color palette from the global style sheet
	QVariant palette = VipGlobalStyleSheet::cstyleSheet().findProperty("VipAbstractPlotArea", "colorpalette");
	if (palette.isNull())
		return;

	if (palette.userType() == QMetaType::Int) {
		// enum value
		VipLinearColorMap::StandardColorMap map = (VipLinearColorMap::StandardColorMap)palette.toInt();
		QByteArray name = VipLinearColorMap::colorMapToName(map);
		name += ":" + QByteArray::number(factor);
		VipGlobalStyleSheet::styleSheet().setProperty("VipAbstractPlotArea", "colorpalette", QVariant::fromValue(name));
	}
	else {
		QByteArray name = palette.toByteArray();
		if (!name.contains(":"))
			name += ":" + QByteArray::number(factor);
		else {
			QList<QByteArray> lst = name.split(':');
			name = lst[0] + ":" + QByteArray::number(factor);
		}
		VipGlobalStyleSheet::styleSheet().setProperty("VipAbstractPlotArea", "colorpalette", QVariant::fromValue(name));
	}
		
	if (m_data->setAndApply) {
		// apply palette
		QList<VipAbstractPlayer*> players = VipUniqueId::objects<VipAbstractPlayer>();
		for (int i = 0; i < players.size(); ++i)
			players[i]->update();
	}
	emitChanged();
}

bool VipGuiDisplayParamaters::videoPlayerShowAxes() const
{
	return m_data->videoPlayerShowAxis;
}
void VipGuiDisplayParamaters::setVideoPlayerShowAxes(bool enable)
{
	if (enable != m_data->videoPlayerShowAxis)
	{
		m_data->videoPlayerShowAxis = enable;
		if (m_data->setAndApply) {
			QList<VipVideoPlayer*> players = VipUniqueId::objects<VipVideoPlayer>();
			for (int i = 0; i < players.size(); ++i) {
				players[i]->showAxes(enable);
			}
		}
		emitChanged();
	}
}

Vip::PlayerLegendPosition VipGuiDisplayParamaters::legendPosition() const
{
	return m_data->legendPosition;
}

void VipGuiDisplayParamaters::setLegendPosition(Vip::PlayerLegendPosition  pos)
{
	if (pos != m_data->legendPosition)
	{
		m_data->legendPosition = pos;
		if (m_data->setAndApply) {
			QList<VipPlotPlayer*> players = VipUniqueId::objects<VipPlotPlayer>();
			for (int i = 0; i < players.size(); ++i)
				players[i]->setLegendPosition(pos);
		}
		emitChanged();
	}
}

void VipGuiDisplayParamaters::setAlwaysShowTimeMarker(bool enable)
{
	if (enable != m_data->showTimeMarkerAlways) {
		m_data->showTimeMarkerAlways = enable;
		if (m_data->setAndApply) {
			QList<VipPlotPlayer*> players = VipUniqueId::objects<VipPlotPlayer>();
			for (int i = 0; i < players.size(); ++i)
				players[i]->setTimeMarkerAlwaysVisible(enable);
		}
	}
	emitChanged();
}

void VipGuiDisplayParamaters::setPlotTitleInside(bool enable)
{
	if (m_data->defaultArea->titleAxis()->titleInside() != enable) {
		m_data->defaultArea->titleAxis()->setTitleInside(enable);
		if (m_data->setAndApply) {
			QList<VipPlotPlayer*> players = VipUniqueId::objects<VipPlotPlayer>();
			for (int i = 0; i < players.size(); ++i)
				players[i]->plotWidget2D()->area()->titleAxis()->setTitleInside(enable);
		}
	}

	emitChanged();
}
void VipGuiDisplayParamaters::setPlotGridVisible(bool visible)
{
	if (m_data->defaultArea->grid()->isVisible() != visible) {
		m_data->defaultArea->grid()->setVisible(visible);
		if (m_data->setAndApply) {
			QList<VipPlotPlayer*> players = VipUniqueId::objects<VipPlotPlayer>();
			for (int i = 0; i < players.size(); ++i)
				players[i]->showGrid(visible);
		}
	}

	emitChanged();
}

void VipGuiDisplayParamaters::setGlobalColorScale(bool enable)
{
	if (m_data->globalColorScale != enable) {
		m_data->globalColorScale = enable;
		if (m_data->setAndApply) {
			VipDisplayArea* a = vipGetMainWindow()->displayArea();
			for (int i = 0; i < a->count(); ++i)
				a->widget(i)->setUseGlobalColorMap(enable);
		}
	}

	emitChanged();
}

void VipGuiDisplayParamaters::setFlatHistogramStrength(int strength)
{
	if (m_data->flatHistogramStrength != strength) {
		m_data->flatHistogramStrength = strength;
		QList<VipVideoPlayer*> players = VipUniqueId::objects<VipVideoPlayer>();
		if (m_data->setAndApply) {
			for (int i = 0; i < players.size(); ++i)
				players[i]->setFlatHistogramStrength(strength);

			VipDisplayArea* a = vipGetMainWindow()->displayArea();
			for (int i = 0; i < a->count(); ++i)
				a->widget(i)->colorMapAxis()->setFlatHistogramStrength(strength);
		}
	}

	emitChanged();
}

void VipGuiDisplayParamaters::autoScaleAll()
{
	if (m_data->setAndApply) {
		QList<VipPlotPlayer*> lst = vipListCast<VipPlotPlayer*>(VipPlayerLifeTime::instance()->players());
		for (int i = 0; i < lst.size(); ++i) {
			lst[i]->setAutoScale(true);
		}
	}
}

VipPlotArea2D * VipGuiDisplayParamaters::defaultPlotArea() const
{
	if (!m_data->defaultArea->isVisible())
		m_data->defaultArea->setVisible(true);
	return m_data->defaultArea;
}
VipPlotCurve * VipGuiDisplayParamaters::defaultCurve() const
{
	if (!m_data->defaultCurve->isVisible())
		m_data->defaultCurve->setVisible(true);
	return m_data->defaultCurve;
}

#include "VipMultiPlotWidget2D.h"

void VipGuiDisplayParamaters::applyDefaultPlotArea(VipPlotArea2D * area)
{
	if (!area)
		return;
	if (VipVMultiPlotArea2D * a = qobject_cast<VipVMultiPlotArea2D*>(area)) {
		a->leftMultiAxis()->setVisible(defaultPlotArea()->leftAxis()->isVisible());
		a->rightMultiAxis()->setVisible(defaultPlotArea()->rightAxis()->isVisible());
	}
	else {
		area->leftAxis()->setVisible(defaultPlotArea()->leftAxis()->isVisible());
		area->rightAxis()->setVisible(defaultPlotArea()->rightAxis()->isVisible());
	}

	area->topAxis()->setVisible(defaultPlotArea()->topAxis()->isVisible());
	area->titleAxis()->setTitleInside(defaultPlotArea()->titleAxis()->titleInside());
	area->titleAxis()->setVisible(defaultPlotArea()->titleAxis()->isVisible());
	area->bottomAxis()->setVisible(defaultPlotArea()->bottomAxis()->isVisible());

	QList<VipPlotGrid*> grids = area->findItems<VipPlotGrid*>();
	for (int i = 0; i < grids.size(); ++i) {
		grids[i]->enableAxis(0, defaultPlotArea()->grid()->axisEnabled(0));
		grids[i]->enableAxis(1, defaultPlotArea()->grid()->axisEnabled(1));
		grids[i]->enableAxisMin(0, defaultPlotArea()->grid()->axisMinEnabled(0));
		grids[i]->enableAxisMin(1, defaultPlotArea()->grid()->axisMinEnabled(1));
		grids[i]->setMajorPen(defaultPlotArea()->grid()->majorPen());
		grids[i]->setMinorPen(defaultPlotArea()->grid()->minorPen());
		grids[i]->setVisible(defaultPlotArea()->grid()->isVisible());
	}
	QList<VipPlotCanvas*> canvas = area->findItems<VipPlotCanvas*>();
	for (int i = 0; i < canvas.size(); ++i)
		canvas[i]->boxStyle().setBackgroundBrush(defaultPlotArea()->canvas()->boxStyle().backgroundBrush());

	QList<VipPlotCurve*> curves = area->findItems<VipPlotCurve*>();
	for (int i = 0; i < curves.size(); ++i)
		applyDefaultCurve(curves[i]);
}

void VipGuiDisplayParamaters::applyDefaultCurve(VipPlotCurve * c)
{
	//apply the curve parameters, but keep the pen and brush color unchanged, as well as the symbol colors
	QColor border = c->pen().color();
	QColor brush = c->brush().color();

	QColor s_border = c->symbol() ? c->symbol()->pen().color() : QColor();
	QColor s_brush = c->symbol() ? c->symbol()->brush().color() : QColor();

	c->setRenderHints(defaultCurve()->renderHints());
	c->setStyle(defaultCurve()->style());
	c->setPen(defaultCurve()->pen());
	c->setBrush(defaultCurve()->brush());
	c->setSymbolVisible(defaultCurve()->symbolVisible());
	//c->setBaseline(defaultCurve()->baseline());
	if (defaultCurve()->symbol())
		c->setSymbol(new VipSymbol(*defaultCurve()->symbol()));

	//reset colors
	c->setPenColor(border);
	c->setBrushColor(brush);
	if (c->symbol()) {
		c->symbol()->setPenColor(s_border);
		c->symbol()->setBrushColor(s_brush);
	}

	//TODO: reapply stylesheet
	
}

bool VipGuiDisplayParamaters::displayTimeOffset() const
{
	return m_data->displayTimeOffset;
}
void VipGuiDisplayParamaters::setDisplayTimeOffset(bool enable)
{
	m_data->displayTimeOffset = enable;
	emitChanged();
}

VipValueToTime::DisplayType VipGuiDisplayParamaters::displayType() const
{
	return m_data->displayType;
}
void VipGuiDisplayParamaters::setDisplayType(VipValueToTime::DisplayType type)
{
	m_data->displayType = type;
	emitChanged();
}

QFont VipGuiDisplayParamaters::defaultEditorFont() const { return m_data->editorFont; }
bool VipGuiDisplayParamaters::alwaysShowTimeMarker() const { return m_data->showTimeMarkerAlways; }
bool VipGuiDisplayParamaters::globalColorScale() const { return m_data->globalColorScale; }

void VipGuiDisplayParamaters::setDefaultEditorFont(const QFont & font)
{
	m_data->editorFont = font;
	emitChanged();
}

bool VipGuiDisplayParamaters::titleVisible() const
{
	return m_data->defaultArea->titleAxis()->isVisible();
}

void VipGuiDisplayParamaters::setTitleVisible(bool vis)
{
	m_data->defaultArea->titleAxis()->setVisible(vis);
	if (m_data->setAndApply) {
		QList<VipPlayer2D*> pls = vipListCast<VipPlayer2D*>(VipPlayerLifeTime::instance()->players());
		for (int i = 0; i < pls.size(); ++i) {
			pls[i]->plotWidget2D()->area()->titleAxis()->setVisible(vis);
		}
	}
	emitChanged();
}



VipTextStyle VipGuiDisplayParamaters::titleTextStyle() const
{
	if (!m_data->titleTextStyle) {

		m_data->titleTextStyle.reset(new VipTextStyle(m_data->resetPlotWidget->area()->titleAxis()->title().textStyle()));
		// use global style sheet
		QVariant color = VipGlobalStyleSheet::cstyleSheet().findProperty("VipAbstractPlotArea", "title-color");
		if (!color.isNull())
			m_data->titleTextStyle->setTextPen(QPen(color.value<QColor>()));
		QVariant font = VipGlobalStyleSheet::cstyleSheet().findProperty("VipAbstractPlotArea", "title-font");
		if (!font.isNull())
			m_data->titleTextStyle->setFont((font.value<QFont>()));
	}
	return *m_data->titleTextStyle;
}

void VipGuiDisplayParamaters::setTitleTextStyle(const VipTextStyle & style)
{
	m_data->titleTextStyle.reset(new VipTextStyle(style));

	
	VipGlobalStyleSheet::styleSheet().setProperty("VipAbstractPlotArea", "title-color", QVariant::fromValue(style.textPen().color()));
	VipGlobalStyleSheet::styleSheet().setProperty("VipAbstractPlotArea", "title-font", QVariant::fromValue(style.font()));

	VipGlobalStyleSheet::styleSheet().setProperty("VipAbstractScale", "title-color", QVariant::fromValue(style.textPen().color()));
	VipGlobalStyleSheet::styleSheet().setProperty("VipAbstractScale", "title-font", QVariant::fromValue(style.font()));

	VipGlobalStyleSheet::styleSheet().setProperty("VipPlotItem", "title-color", QVariant::fromValue(style.textPen().color()));
	VipGlobalStyleSheet::styleSheet().setProperty("VipPlotItem", "title-font", QVariant::fromValue(style.font()));

	if (m_data->setAndApply) 
		vipGetMainWindow()->update();
	

	emitChanged();
}

void VipGuiDisplayParamaters::setTitleTextStyle2(const VipText & text)
{
	setTitleTextStyle(text.textStyle());
}

void VipGuiDisplayParamaters::setDefaultTextStyle(const VipTextStyle & style)
{
	m_data->defaultTextStyle.reset(new VipTextStyle(style));

	VipGlobalStyleSheet::styleSheet().setProperty("VipLegend", "color", QVariant::fromValue(style.textPen().color()));
	VipGlobalStyleSheet::styleSheet().setProperty("VipLegend", "font", QVariant::fromValue(style.font()));

	VipGlobalStyleSheet::styleSheet().setProperty("VipAbstractScale", "label-color", QVariant::fromValue(style.textPen().color()));
	VipGlobalStyleSheet::styleSheet().setProperty("VipAbstractScale", "label-font", QVariant::fromValue(style.font()));

	VipGlobalStyleSheet::styleSheet().setProperty("VipPlotItem", "color", QVariant::fromValue(style.textPen().color()));
	VipGlobalStyleSheet::styleSheet().setProperty("VipPlotItem", "font", QVariant::fromValue(style.font()));

	if (m_data->setAndApply) 
		vipGetMainWindow()->update();

	emitChanged();
}
void VipGuiDisplayParamaters::setDefaultTextStyle2(const VipText & t)
{
	setDefaultTextStyle(t.textStyle());
}
VipTextStyle VipGuiDisplayParamaters::defaultTextStyle() const
{
	if (!m_data->defaultTextStyle) {

		m_data->defaultTextStyle.reset(new VipTextStyle(m_data->resetPlotWidget->area()->titleAxis()->title().textStyle()));
		// use global style sheet
		QVariant color = VipGlobalStyleSheet::cstyleSheet().findProperty("VipAbstractScale", "label-color");
		if (!color.isNull())
			m_data->defaultTextStyle->setTextPen(QPen(color.value<QColor>()));
		QVariant font = VipGlobalStyleSheet::cstyleSheet().findProperty("VipAbstractPlotArea", "label-font");
		if (!font.isNull())
			m_data->defaultTextStyle->setFont((font.value<QFont>()));
	}
	return *m_data->defaultTextStyle;
}

QColor VipGuiDisplayParamaters::defaultPlayerTextColor() const
{
	QVariant v = VipGlobalStyleSheet::cstyleSheet().findProperty("VipAbstractPlotArea", "title-color");
	if (v.isNull())
		return Qt::black;
	return v.value<QColor>();
	//return m_data->resetPlotWidget->titlePen().color();
}

QColor VipGuiDisplayParamaters::defaultPlayerBackgroundColor() const
{
	return m_data->resetPlotWidget->backgroundColor();
}

bool VipGuiDisplayParamaters::hasTitleTextStyle() const
{
	return m_data->titleTextStyle;
}
bool VipGuiDisplayParamaters::hasDefaultTextStyle() const
{
	return m_data->defaultTextStyle;
}

int VipGuiDisplayParamaters::flatHistogramStrength() const
{
	return m_data->flatHistogramStrength;
}

VipLinearColorMap::StandardColorMap VipGuiDisplayParamaters::playerColorScale() const
{
	return m_data->playerColorScale;
}
void VipGuiDisplayParamaters::setPlayerColorScale(VipLinearColorMap::StandardColorMap map)
{
	if (m_data->setAndApply) {
		// apply color scale to video players
		QList<VipVideoPlayer*> vplayers = VipUniqueId::objects<VipVideoPlayer>();
		for (int i = 0; i < vplayers.size(); ++i) {
			if (vplayers[i]->spectrogram()->colorMap() && qobject_cast<VipLinearColorMap*>(vplayers[i]->spectrogram()->colorMap()->colorMap()) &&
			    static_cast<VipLinearColorMap*>(vplayers[i]->spectrogram()->colorMap()->colorMap())->type() == m_data->playerColorScale) {
				bool flat_hist = vplayers[i]->spectrogram()->colorMap()->useFlatHistogram();
				int str = vplayers[i]->spectrogram()->colorMap()->flatHistogramStrength();
				vplayers[i]->spectrogram()->colorMap()->setColorMap(vplayers[i]->spectrogram()->colorMap()->gripInterval(), VipLinearColorMap::createColorMap(map));
				vplayers[i]->spectrogram()->colorMap()->setUseFlatHistogram(flat_hist);
				vplayers[i]->spectrogram()->colorMap()->setFlatHistogramStrength(str);
			}
		}

		// apply to workspace
		VipDisplayArea* d = vipGetMainWindow()->displayArea();
		for (int i = 0; i < d->count(); ++i) {
			d->widget(i)->setColorMap(map);
			d->widget(i)->colorMapAxis()->setFlatHistogramStrength(flatHistogramStrength());
		}
	}
	m_data->playerColorScale = map;
	emitChanged();
}


int VipGuiDisplayParamaters::videoRenderingStrategy() const
{
	return m_data->videoRenderingStrategy;
}
int VipGuiDisplayParamaters::plotRenderingStrategy() const
{
	return m_data->plotRenderingStrategy;
}

void VipGuiDisplayParamaters::setVideoRenderingStrategy(int st)
{
	if (st != m_data->videoRenderingStrategy)
	{
		m_data->videoRenderingStrategy = st;
		if (m_data->setAndApply) {
			QList<VipVideoPlayer*> pls = vipListCast<VipVideoPlayer*>(VipPlayerLifeTime::instance()->players());
			for (int i = 0; i < pls.size(); ++i) {
				if (st >= 0 && st <= VipBaseGraphicsView::OpenGLThread)
					pls[i]->plotWidget2D()->setRenderingMode((VipBaseGraphicsView::RenderingMode)st);
			}
		}
		emitChanged();
	}
}
void VipGuiDisplayParamaters::setPlotRenderingStrategy(int st)
{
	if (st != m_data->plotRenderingStrategy)
	{
		m_data->plotRenderingStrategy = st;
		if (m_data->setAndApply) {
			QList<VipPlotPlayer*> pls = vipListCast<VipPlotPlayer*>(VipPlayerLifeTime::instance()->players());
			for (int i = 0; i < pls.size(); ++i) {
				if (st >= 0 && st <= VipBaseGraphicsView::OpenGLThread)
					pls[i]->plotWidget2D()->setRenderingMode((VipBaseGraphicsView::RenderingMode)st);
			}
		}
		emitChanged();
	}
}


void VipGuiDisplayParamaters::setDisplayExactPixels(bool enable)
{
	if (enable != m_data->displayExactPixels)
	{
		m_data->displayExactPixels = enable;
		emitChanged();
	}
}
bool VipGuiDisplayParamaters::displayExactPixels() const
{
	return m_data->displayExactPixels;
}

void VipGuiDisplayParamaters::apply(QWidget * w)
{
	if (VipAbstractPlayer * pl = qobject_cast<VipAbstractPlayer*>(w)) {
		
		
		if (VipVideoPlayer * v = qobject_cast<VipVideoPlayer*>(pl)) {
			if (v->spectrogram()->colorMap() && qobject_cast<VipLinearColorMap*>(v->spectrogram()->colorMap()->colorMap())
				&& static_cast<VipLinearColorMap*>(v->spectrogram()->colorMap()->colorMap())->type() == m_data->playerColorScale) {
				bool flat_hist = v->spectrogram()->colorMap()->useFlatHistogram();
				v->spectrogram()->colorMap()->setColorMap(v->spectrogram()->colorMap()->gripInterval(), VipLinearColorMap::createColorMap(playerColorScale()));
				v->spectrogram()->colorMap()->setUseFlatHistogram(flat_hist);
			}

			v->showAxes(videoPlayerShowAxes());
			v->plotWidget2D()->area()->titleAxis()->setVisible(defaultPlotArea()->titleAxis()->isVisible());
			v->setFlatHistogramStrength(flatHistogramStrength());

			if (m_data->videoRenderingStrategy >= 0 && m_data->videoRenderingStrategy <= VipBaseGraphicsView::OpenGLThread)
				v->plotWidget2D()->setRenderingMode((VipBaseGraphicsView::RenderingMode)m_data->videoRenderingStrategy);
		}

		if (VipPlotPlayer * p = qobject_cast<VipPlotPlayer*>(pl)) {
			p->setLegendPosition(this->legendPosition());
			//p->valueToTimeButton()->setDisplayTimeOffset(m_data->displayTimeOffset);
			p->setTimeMarkerAlwaysVisible(alwaysShowTimeMarker());
			if (m_data->plotRenderingStrategy >= 0 && m_data->plotRenderingStrategy <= VipBaseGraphicsView::OpenGLThread)
				p->plotWidget2D()->setRenderingMode((VipBaseGraphicsView::RenderingMode)m_data->plotRenderingStrategy);
			applyDefaultPlotArea(qobject_cast<VipPlotArea2D*>(p->plotWidget2D()->area()));
		}
	}

}

void VipGuiDisplayParamaters::reset()
{
	blockSignals(true);
	setItemPaletteFactor(0);
	setPlayerColorScale(VipLinearColorMap::Jet);
	setLegendPosition(Vip::LegendBottom);
	setVideoPlayerShowAxes(true);
	setDisplayTimeOffset(false);
	setDisplayType(VipValueToTime::Double);
	setAlwaysShowTimeMarker(false);
	setGlobalColorScale(false);
	setPlotGridVisible(true);
	setPlotTitleInside(false);
	setFlatHistogramStrength(1);
	setVideoRenderingStrategy(VipBaseGraphicsView::Raster);
	setPlotRenderingStrategy(VipBaseGraphicsView::Raster);
	//setTitleTextStyle(VipTextStyle());
	//setDefaultTextStyle(VipTextStyle());
	m_data->titleTextStyle.reset();
	m_data->defaultTextStyle.reset();
	m_data->defaultPlotWidget->setStyleSheet(QString());
	QList<VipPlayer2D*> pls = vipListCast<VipPlayer2D*>(VipPlayerLifeTime::instance()->players());
	for (int i = 0; i < pls.size(); ++i)
		pls[i]->setStyleSheet(QString());

	setShapeBorderPen( QPen(QColor(Qt::black), 1));
	setShapeBackgroundBrush( QBrush(QColor(255, 0, 0, 70)));
	setShapeDrawComponents( VipPlotShape::Background | VipPlotShape::Border | VipPlotShape::Id);


	blockSignals(false);
	emitChanged();
}


bool  VipGuiDisplayParamaters::save(const QString & file)
{
	QString fname = file;
	if (fname.isEmpty())
		fname = vipGetDataDirectory() + "gui_settings.xml";
	VipXOfArchive ar;
	if (!ar.open(fname))
		return false;
	return save(ar);
}

bool  VipGuiDisplayParamaters::restore(const QString & file)
{
	QString fname = file;
	if (fname.isEmpty())
		fname = vipGetDataDirectory() + "gui_settings.xml";
	VipXIfArchive ar;
	if (!ar.open(fname))
		return false;
	return restore(ar);
}

void VipGuiDisplayParamaters::emitChanged()
{
	m_data->dirty = true;
	Q_EMIT changed();
}
void VipGuiDisplayParamaters::delaySaveToFile()
{
	if (m_data->dirty) {
		m_data->dirty = false;
		save();
	}
}




static QString _skin;

static void serialize_VipGuiDisplayParamaters(VipGuiDisplayParamaters * inst, VipArchive & arch)
{
	if (arch.mode() == VipArchive::Read)
	{
		arch.save();
		if (arch.start("VipGuiDisplayParamaters"))
		{
			arch.save();
			QString version = arch.read("version").toString();
			if (version.isEmpty())
				arch.restore();

			//check version
			QStringList vers = version.split(".", VIP_SKIP_BEHAVIOR::SkipEmptyParts);
			QList<int> ivers;
			for (int i = 0; i < vers.size(); ++i) ivers.append(vers[i].toInt());

			arch.save();
			QString skin;
			bool diff_skin = true;
			if (arch.content("skin", skin)) {
				//printf("current: %s, read: %s\n", VipCoreSettings::instance()->skin().toLatin1().data(), skin.toLatin1().data());
				diff_skin = VipCoreSettings::instance()->skin() != skin;
				_skin = VipCoreSettings::instance()->skin();
			}
			else
				arch.restore();

			inst->setItemPaletteFactor(arch.read("itemPaletteFactor").value<int>());
			if (ivers < (QList<int>() << 3 << 3 << 4)) {
				inst->setPlayerColorScale(VipLinearColorMap::Jet);
			}
			else
				inst->setPlayerColorScale(VipLinearColorMap::StandardColorMap(arch.read("playerColorScale").value<int>()));

			bool show_axes = arch.read("video_player_axes").toBool();
			if (arch)
				inst->setVideoPlayerShowAxes(show_axes);
			else
				arch.resetError();

			//new in 2.2.17
			arch.save();
			int legendPosition = 1;
			arch.content("legendPosition", legendPosition);
			if (!arch)
				arch.restore();
			else {
				inst->setLegendPosition((Vip::PlayerLegendPosition)legendPosition);
			}

			arch.content("defaultPlotArea", inst->defaultPlotArea());
			arch.content("defaultCurve", inst->defaultCurve());
			inst->setDisplayTimeOffset(arch.read("displayTimeOffset").toBool());
			inst->setDisplayType((VipValueToTime::DisplayType)arch.read("displayType").toInt());

			if (ivers < (QList<int>() << 3 << 3 << 5)) {
				//starting version 3.3.5, the default pen width for curves is 1.5
				QPen p = inst->defaultCurve()->pen();
				p.setWidthF(1.5);
				inst->defaultCurve()->setPen(p);
				//hide grid
				inst->defaultPlotArea()->grid()->setVisible(false);
			}

			//new in 2.2.18
			arch.save();
			bool showTimeMarkerAlways;
			if (arch.content("showTimeMarkerAlways", showTimeMarkerAlways))
				inst->setAlwaysShowTimeMarker(showTimeMarkerAlways);
			else {
				arch.restore();
				showTimeMarkerAlways = false;
				inst->setAlwaysShowTimeMarker(false);
			}
			//printf("read showTimeMarkerAlways: %i\n", (int)showTimeMarkerAlways);
			arch.save();
			bool globalColorScale;
			if (arch.content("globalColorScale", globalColorScale))
				inst->setGlobalColorScale(globalColorScale);
			else {
				arch.restore();
				globalColorScale = false;
				inst->setGlobalColorScale(false);
			}
			//printf("read globalColorScale: %i\n", (int)globalColorScale);


			//new in 3.3.3
			arch.save();
			int flatHistogramStrength;
			if (arch.content("flatHistogramStrength", flatHistogramStrength))
				inst->setFlatHistogramStrength(flatHistogramStrength);
			else {
				arch.restore();
				inst->setFlatHistogramStrength(1);
			}

			//new in 3.4.7
			arch.save();
			int videoRenderingMode,plotRenderingMode;
			arch.content("videoRenderingMode", videoRenderingMode);
			arch.content("plotRenderingMode", plotRenderingMode);
			if (arch) {
				inst->setVideoRenderingStrategy(videoRenderingMode);
				inst->setPlotRenderingStrategy(plotRenderingMode);
			}
			else {
				arch.restore();
				inst->setVideoRenderingStrategy(VipBaseGraphicsView::Raster);
				inst->setPlotRenderingStrategy(VipBaseGraphicsView::Raster);
			}

			arch.save();
			VipTextStyle titleTextStyle;
			if (!diff_skin && arch.content("titleTextStyle", titleTextStyle))
				inst->setTitleTextStyle(titleTextStyle);
			else
				arch.restore();

			arch.save();
			VipTextStyle defaultTextStyle;
			if (!diff_skin && arch.content("defaultTextStyle", defaultTextStyle))
				inst->setDefaultTextStyle(defaultTextStyle);
			else
				arch.restore();


			//new in 3.0.2
			arch.save();
			QBrush b; QPen p; int c;
			if (arch.content("backgroundBrush", b)) {
				arch.content("borderPen", p);
				arch.content("drawComponents", c);
				inst->setShapeBackgroundBrush(b);
				inst->setShapeBorderPen(p);
				inst->setShapeDrawComponents((VipPlotShape::DrawComponents)c);
			}
			else
				arch.restore();


			// New in 3.11.0
			arch.save();
			int displayExactPixels = 0;
			if (arch.content("displayExactPixels", displayExactPixels)) {
				inst->setDisplayExactPixels(displayExactPixels != 0);
			}
			else
				arch.restore();

			arch.end();

			//Force some parameters for old version

			//check version
			if (ivers < (QList<int>() << 3 << 0 << 2)) {
				//make ROI more transparent
				//QBrush b = VipDefaultSceneModelDisplayOptions::backgroundBrush();
				// QColor c = b.color();
				// c.setAlpha(70);
				// b.setColor(c);
				// VipDefaultSceneModelDisplayOptions::setBackgroundBrush(b);

				//force font
				//QBrush b = vipWidgetTextBrush(vipGetMainWindow());
				// VipTextStyle st;
				// st.setTextPen(QPen(b.color()));
				// QFont f;
				// f.fromString("DejaVu Sans Display,8,-1,5,50,0,0,0,0,0,Display");
				// st.setFont(f);
				// inst->setDefaultTextStyle(st);
				// inst->setTitleTextStyle(st);
			}


		}
		else
			arch.restore();

	}
	else if (arch.mode() == VipArchive::Write)
	{
		if (arch.start("VipGuiDisplayParamaters"))
		{
			arch.content("version", QString(VIP_VERSION));
			arch.content("skin", _skin);
			//printf("save: %s\n", _skin.toLatin1().data());

			arch.content("itemPaletteFactor", inst->itemPaletteFactor());
			arch.content("playerColorScale", (int)inst->playerColorScale());
			arch.content("video_player_axes", inst->videoPlayerShowAxes());
			arch.content("legendPosition", (int)inst->legendPosition());
			arch.content("defaultPlotArea", inst->defaultPlotArea());
			arch.content("defaultCurve", inst->defaultCurve());
			arch.content("displayTimeOffset", inst->displayTimeOffset());
			arch.content("displayType", (int)inst->displayType());

			//new in 2.2.18
			arch.content("showTimeMarkerAlways", inst->alwaysShowTimeMarker());
			arch.content("globalColorScale", inst->globalColorScale());

			//new in 3.3.3
			arch.content("flatHistogramStrength", inst->flatHistogramStrength());

			//new in 3.4.7
			arch.content("videoRenderingMode", inst->videoRenderingStrategy());
			arch.content("plotRenderingMode", inst->plotRenderingStrategy());

			//printf("write showTimeMarkerAlways: %i\n", (int)inst->alwaysShowTimeMarker());
			//printf("write globalColorScale: %i\n", (int)inst->globalColorScale());

			if (inst->hasTitleTextStyle())
				arch.content("titleTextStyle", inst->titleTextStyle());
			if (inst->hasDefaultTextStyle())
				arch.content("defaultTextStyle", inst->defaultTextStyle());

			//new in 3.0.2
			arch.content("backgroundBrush", inst->shapeBackgroundBrush());
			arch.content("borderPen", inst->shapeBorderPen());
			arch.content("drawComponents", (int)inst->shapeDrawComponents());

			// New in 3.11.0
			arch.content("displayExactPixels", (int)inst->displayExactPixels());

			arch.end();
		}

	}
}

bool  VipGuiDisplayParamaters::save(VipArchive & ar)
{
	serialize_VipGuiDisplayParamaters(this,ar);
	return !ar.hasError();
}
bool  VipGuiDisplayParamaters::restore(VipArchive & ar)
{
	serialize_VipGuiDisplayParamaters(this, ar);
	return !ar.hasError();
}

static int registerVipGuiDisplayParamaters()
{
	return 0;
}
static int _registerVipGuiDisplayParamaters = registerVipGuiDisplayParamaters();












VipFunctionDispatcher<3>& vipFDCreateDisplayFromData()
{
	static VipFunctionDispatcher<3> disp;
	return disp;
}

VipDisplayObject* vipCreateDisplayFromData(const VipAnyData& any, VipAbstractPlayer* pl)
{
	VipDisplayObject* res = NULL;
	const auto lst = vipFDCreateDisplayFromData().exactMatch(any.data(), pl);
	if (lst.size())
		res = lst.back()(any.data(), pl, any).value<VipDisplayObject*>();
	else {
		// default behavior: handle VipPointVector, VipIntervalSampleVector and VipNDArray
		if (any.data().userType() == qMetaTypeId<VipPointVector>()) {
			VipDisplayCurve* curve = new VipDisplayCurve();
			curve->inputAt(0)->setData(any);
			curve->item()->setData(any.data());
			curve->setItemSuppressable(true);
			curve->formatItem(curve->item(), any, true);
			res = curve;
		}
		else if (any.data().canConvert<double>()) {
			VipDisplayCurve* curve = new VipDisplayCurve();
			curve->inputAt(0)->setData(any);
			curve->item()->setData(QVariant::fromValue(VipPointVector()));
			curve->setItemSuppressable(true);
			curve->formatItem(curve->item(), any, true);
			res = curve;
		}
		else if (any.data().canConvert<VipPoint>()) {
			VipDisplayCurve* curve = new VipDisplayCurve();
			curve->inputAt(0)->setData(any);
			curve->item()->setData(QVariant::fromValue(VipPointVector() << any.value<VipPoint>()));
			curve->setItemSuppressable(true);
			curve->formatItem(curve->item(), any, true);
			res = curve;
		}
		else if (any.data().userType() == qMetaTypeId<VipIntervalSampleVector>()) {
			VipDisplayHistogram* curve = new VipDisplayHistogram();
			curve->inputAt(0)->setData(any);
			curve->item()->setData(any.data());
			curve->setItemSuppressable(true);
			curve->formatItem(curve->item(), any, true);
			res = curve;
		}
		else if (any.data().canConvert<VipIntervalSample>()) {
			VipDisplayHistogram* curve = new VipDisplayHistogram();
			curve->inputAt(0)->setData(any);
			curve->item()->setData(QVariant::fromValue(VipIntervalSampleVector() << any.data().value<VipIntervalSample>()));
			curve->setItemSuppressable(true);
			curve->formatItem(curve->item(), any, true);
			res = curve;
		}
		else if (any.data().userType() == qMetaTypeId<VipNDArray>()) {
			VipDisplayImage* curve = new VipDisplayImage();
			curve->inputAt(0)->setData(any);
			curve->item()->setRawData(VipRasterData(any.data().value<VipNDArray>(), QPointF(0, 0)));
			curve->formatItem(curve->item(), any, true);
			res = curve;
		}
		else if (any.data().userType() == qMetaTypeId<VipSceneModel>()) {
			VipDisplaySceneModel* curve = new VipDisplaySceneModel();
			curve->inputAt(0)->setData(any);
			curve->item()->setSceneModel(any.data().value<VipSceneModel>());
			res = curve;
		}
		else if (any.data().userType() == qMetaTypeId<VipShape>()) {
			VipDisplaySceneModel* curve = new VipDisplaySceneModel();
			curve->inputAt(0)->setData(any);
			curve->item()->sceneModel().add("All", any.data().value<VipShape>());
			res = curve;
		}
	}
	if (VipDisplayPlotItem* it = qobject_cast<VipDisplayPlotItem*>(res)) {
		// set the _vip_created property to tell that the item can trigger an axis creation
		it->item()->setProperty("_vip_created", true);
	}
	return res;
}

VipFunctionDispatcher<4>& vipFDCreatePlayersFromData()
{
	static VipFunctionDispatcher<4> disp;
	return disp;
}

static void setProcessingPool(VipProcessingObject* obj, VipProcessingPool* pool)
{
	if (!pool || !obj)
		return;
	obj->setParent(pool);
	QList<VipProcessingObject*> sources = obj->allSources();
	for (int i = 0; i < sources.size(); ++i)
		// TOCHECK: check processing pool before
		if (!sources[i]->parentObjectPool())
			sources[i]->setParent(pool);
}

QList<VipAbstractPlayer*> vipCreatePlayersFromData(const VipAnyData& any, VipAbstractPlayer* pl, VipOutput* src, QObject* target, QList<VipDisplayObject*>* doutputs)
{
	const auto lst = vipFDCreatePlayersFromData().exactMatch(any.data(), pl);
	if (lst.size()) {
		QList<VipDisplayObject*> existing;
		if (pl)
			existing = pl->displayObjects();
		QList<VipAbstractPlayer*> res = lst.back()(any.data(), pl, any, target);
		if (doutputs) {
			for (int i = 0; i < res.size(); ++i) {
				// build the list of created display objects
				QList<VipDisplayObject*> disps = res[i]->displayObjects();
				for (int j = 0; j < disps.size(); ++j)
					if (existing.indexOf(disps[j]) < 0)
						*doutputs << disps[j];
			}
		}
		return res;
	}

	if (any.isEmpty())
		return QList<VipAbstractPlayer*>();

	VipDisplayObject* display = vipCreateDisplayFromData(any, pl);
	if (pl)
		setProcessingPool(display, pl->processingPool());

	if (display) {

		VipProcessingObject* source = NULL;
		if (src)
			source = src->parentProcessing();
		if (source)
			display->setParent(source->parent());
		VipIODevice* device = qobject_cast<VipIODevice*>(source);

		if (device) {
			// for numerical values, insert a VipNumericValueToPointVector before the processing list
			bool is_numeric = false;
			src->data().data().toDouble(&is_numeric);
			if (is_numeric) {
				VipNumericValueToPointVector* ConvertToPointVector = new VipNumericValueToPointVector(device->parent());
				ConvertToPointVector->setScheduleStrategies(VipProcessingObject::Asynchronous);
				ConvertToPointVector->setDeleteOnOutputConnectionsClosed(true);
				ConvertToPointVector->inputAt(0)->setConnection(src);
				src = ConvertToPointVector->outputAt(0);
			}

			// insert a VipProcessingList in between the device output and the VipDisplayObject
			VipProcessingList* _lst = new VipProcessingList();
			_lst->setParent(device->parent());
			_lst->setScheduleStrategies(VipProcessingObject::Asynchronous);

			src->setConnection(_lst->inputAt(0));
			_lst->outputAt(0)->setConnection(display->inputAt(0));

			// add an VipExtractComponent in the list for VipNDArray only
			if (any.data().userType() == qMetaTypeId<VipNDArray>()) {
				VipExtractComponent* extract = new VipExtractComponent();
				// extract->setProperty("_vip_hidden",true)
				extract->setVisible(false);
				_lst->append(extract);
				// vipGetProcessingEditorToolWidget()->setProcessingObject(display);
				// if (VipProcessingListEditor * editor = vipGetProcessingEditorToolWidget()->editor()->processingEditor<VipProcessingListEditor*>(_lst))
				//	editor->setProcessingVisible(_lst->at(_lst->size() - 1), false);
			}

			// if the device is destroyed, destroy everything
			_lst->setDeleteOnOutputConnectionsClosed(true);
			device->setDeleteOnOutputConnectionsClosed(true);
		}
		else if (src) {
			src->setConnection(display->inputAt(0));
		}

		QList<VipAbstractPlayer*> res = vipCreatePlayersFromProcessing(display, pl, NULL, target);
		if (!res.size()) {
			// we cannot insert the VipDisplayObject in an existing player: delete it
			if (device)
				device->setDeleteOnOutputConnectionsClosed(false);
			display->deleteLater();
		}
		if (doutputs)
			*doutputs << display;
		return res;
	}

	return QList<VipAbstractPlayer*>();
}

VipFunctionDispatcher<4>& vipFDCreatePlayersFromProcessing()
{
	static VipFunctionDispatcher<4> disp;
	return disp;
}

QList<VipAbstractPlayer*> vipCreatePlayersFromProcessing(VipProcessingObject* disp, VipAbstractPlayer* pl, VipOutput* output, QObject* target, QList<VipDisplayObject*>* doutputs)
{
	if (!disp)
		return QList<VipAbstractPlayer*>();

	const auto lst = vipFDCreatePlayersFromProcessing().match(disp, pl);
	if (lst.size()) {
		QList<VipAbstractPlayer*> res = lst.back()(disp, pl, output, target);
		QList<VipDisplayObject*> existing;
		if (pl && doutputs)
			existing = pl->displayObjects();
		if (doutputs) {
			for (int i = 0; i < res.size(); ++i) {
				// build the list of created display objects
				QList<VipDisplayObject*> disps = res[i]->displayObjects();
				for (int j = 0; j < disps.size(); ++j)
					if (existing.indexOf(disps[j]) < 0)
						*doutputs << disps[j];
			}
		}
		return res;
	}

	// default behavior

	// default behavior: handle VipDisplayCurve, VipDisplayImage, VipDisplayHistogram, VipVideoPlayer and VipPlotPlayer
	if (VipDisplayCurve* curve = qobject_cast<VipDisplayCurve*>(disp)) {
		if (VipPlotPlayer* player = qobject_cast<VipPlotPlayer*>(pl)) {
			setProcessingPool(curve, player->processingPool());
			if (VipPlotItem* item = qobject_cast<VipPlotItem*>(target))
				curve->item()->setAxes(item->axes(), VipCoordinateSystem::Cartesian);
			else
				curve->item()->setAxes(player->defaultXAxis(), player->defaultYAxis(), player->defaultCoordinateSystem());
			if (doutputs)
				*doutputs << curve;
			return QList<VipAbstractPlayer*>() << pl;
		}
		else if (!pl) {
			VipPlotPlayer* plot_player = new VipPlotPlayer();
			plot_player->setWindowTitle(curve->item()->title().text());
			curve->item()->setAxes(plot_player->defaultXAxis(), plot_player->defaultYAxis(), plot_player->defaultCoordinateSystem());
			if (doutputs)
				*doutputs << curve;
			return QList<VipAbstractPlayer*>() << plot_player;
		}
		else
			return QList<VipAbstractPlayer*>();
	}
	else if (VipDisplayHistogram* hist = qobject_cast<VipDisplayHistogram*>(disp)) {
		if (VipPlotPlayer* player = qobject_cast<VipPlotPlayer*>(pl)) {
			setProcessingPool(hist, player->processingPool());
			if (VipPlotItem* item = qobject_cast<VipPlotItem*>(target))
				hist->item()->setAxes(item->axes(), VipCoordinateSystem::Cartesian);
			else
				hist->item()->setAxes(player->defaultXAxis(), player->defaultYAxis(), player->defaultCoordinateSystem());
			if (doutputs)
				*doutputs << hist;
			return QList<VipAbstractPlayer*>() << pl;
		}
		else if (!pl) {
			VipPlotPlayer* plot_player = new VipPlotPlayer();
			plot_player->setWindowTitle(hist->item()->title().text());
			hist->item()->setAxes(plot_player->defaultXAxis(), plot_player->defaultYAxis(), plot_player->defaultCoordinateSystem());
			if (doutputs)
				*doutputs << hist;
			return QList<VipAbstractPlayer*>() << plot_player;
		}
		else
			return QList<VipAbstractPlayer*>();
	}
	else if (VipDisplayImage* img = qobject_cast<VipDisplayImage*>(disp)) {
		// if(VipVideoPlayer * player = qobject_cast<VipVideoPlayer*>(pl))
		// {
		// player->setSpectrogram(img->item());
		// return QList<VipAbstractPlayer*>()<< player;
		// }
		// else
		if (pl) // we cannot add a VipDisplayImage in an existing player which is not a VipVideoPlayer
			return QList<VipAbstractPlayer*>();
		else {
			VipVideoPlayer* player = new VipVideoPlayer();
			player->setSpectrogram(img->item());
			player->setWindowTitle(img->item()->title().text());
			if (doutputs)
				*doutputs << img;
			return QList<VipAbstractPlayer*>() << player;
		}
	}
	else if (VipDisplaySceneModel* scene_model = qobject_cast<VipDisplaySceneModel*>(disp)) {
		// a VipDisplaySceneModel can be displays in both VipVideoPlayer and VipPlotPlayer. Default player type is VipVideoPlayer
		if (VipPlayer2D* player = qobject_cast<VipPlayer2D*>(pl)) {
			if (VipVideoPlayer* video = qobject_cast<VipVideoPlayer*>(pl)) {
				setProcessingPool(scene_model, player->processingPool());
				scene_model->item()->setMode(VipPlotSceneModel::Fixed);
				scene_model->item()->setAxes(video->viewer()->area()->bottomAxis(), video->viewer()->area()->leftAxis(), VipCoordinateSystem::Cartesian);
				if (doutputs)
					*doutputs << scene_model;
				return QList<VipAbstractPlayer*>() << player;
			}
			else if (VipPlotPlayer* plot = qobject_cast<VipPlotPlayer*>(pl)) {
				setProcessingPool(scene_model, player->processingPool());
				scene_model->item()->setMode(VipPlotSceneModel::Fixed);
				if (VipPlotItem* item = qobject_cast<VipPlotItem*>(target))
					scene_model->item()->setAxes(item->axes(), VipCoordinateSystem::Cartesian);
				else
					scene_model->item()->setAxes(plot->defaultXAxis(), plot->defaultYAxis(), plot->defaultCoordinateSystem());
				if (doutputs)
					*doutputs << scene_model;
				return QList<VipAbstractPlayer*>() << player;
			}
			else
				return QList<VipAbstractPlayer*>();
		}
		else {
			VipVideoPlayer* res = new VipVideoPlayer();
			res->setWindowTitle(scene_model->item()->title().text());
			scene_model->item()->setAxes(res->viewer()->area()->bottomAxis(), res->viewer()->area()->leftAxis(), VipCoordinateSystem::Cartesian);
			if (doutputs)
				*doutputs << scene_model;
			return QList<VipAbstractPlayer*>() << res;
		}
	}
	else if (VipDisplayPlotItem* plot_item = qobject_cast<VipDisplayPlotItem*>(disp)) {
		// any other VipDisplayPlotItem
		if (VipPlotPlayer* player = qobject_cast<VipPlotPlayer*>(pl)) {
			setProcessingPool(plot_item, player->processingPool());
			if (VipPlotItem* item = qobject_cast<VipPlotItem*>(target))
				plot_item->item()->setAxes(item->axes(), VipCoordinateSystem::Cartesian);
			else
				plot_item->item()->setAxes(player->defaultXAxis(), player->defaultYAxis(), player->defaultCoordinateSystem());
			if (doutputs)
				*doutputs << plot_item;
			return QList<VipAbstractPlayer*>() << pl;
		}
		else if (VipVideoPlayer* video = qobject_cast<VipVideoPlayer*>(pl)) {
			setProcessingPool(plot_item, video->processingPool());
			plot_item->item()->setAxes(video->viewer()->area()->bottomAxis(), video->viewer()->area()->leftAxis(), VipCoordinateSystem::Cartesian);
			if (doutputs)
				*doutputs << plot_item;
			return QList<VipAbstractPlayer*>() << pl;
		}
		else {
			VipPlotPlayer* res = new VipPlotPlayer();
			res->setWindowTitle(plot_item->item()->title().text());
			plot_item->item()->setAxes(res->defaultXAxis(), res->defaultYAxis(), res->defaultCoordinateSystem());
			if (doutputs)
				*doutputs << plot_item;
			return QList<VipAbstractPlayer*>() << res;
		}
	}

	// special case: device having no output
	if (disp->topLevelOutputCount() == 0)
		return QList<VipAbstractPlayer*>();

	// compute all VipOutput object to use
	QVector<VipOutput*> proc_outputs;
	if (!output) {
		for (int i = 0; i < disp->outputCount(); ++i) {
			if (disp->outputAt(i)->isEnabled())
				proc_outputs.append(disp->outputAt(i));
		}
	}
	else
		proc_outputs.append(output);

	// the VipDisplayObject we are going to create
	QList<VipDisplayObject*> displayObjects;
	// all the inputs from the device
	QVector<VipAnyData> outputs(proc_outputs.size());

	// try to read the device until we've got all outputs
	VipIODevice* device = qobject_cast<VipIODevice*>(disp);
	if (!device)
		device = disp->parentObjectPool();

	// directly read the processing outputs
	bool all_outputs = true;
	for (int i = 0; i < proc_outputs.size(); ++i) {
		outputs[i] = proc_outputs[i]->data();
		if (outputs[i].isEmpty())
			all_outputs = false;
	}

	// if some outputs are still empty, read the VipProcessingPool until we have valid outputs
	if (device && !all_outputs) {
		qint64 time = device->firstTime();
		while (true) {
			device->read(time);

			bool has_all_output = true;
			for (int i = 0; i < proc_outputs.size(); ++i) {
				proc_outputs[i]->parentProcessing()->wait();
				outputs[i] = proc_outputs[i]->data();
				if (outputs[i].isEmpty())
					has_all_output = false;
			}

			if (has_all_output)
				break;

			qint64 next = device->nextTime(time);
			if (time == next || next == VipInvalidTime)
				break;
			time = next;
		}
	}

	QList<VipAbstractPlayer*> players;

	// we've got all possible outputs, create the corresponding players
	for (int i = 0; i < outputs.size(); ++i) {
		if (outputs[i].isEmpty())
			continue;

		QList<VipAbstractPlayer*> tmp;

		if (pl) // if a player is given, try to insert the data inside it
			tmp = vipCreatePlayersFromData(outputs[i], pl, proc_outputs[i], target, doutputs);
		else if (!players.size()) // no created players: create a new one
			tmp = vipCreatePlayersFromData(outputs[i], NULL, proc_outputs[i], target, doutputs);
		else // otherwise, try to insert it into an existing one
		{
			for (int p = 0; p < players.size(); ++p) {
				tmp = vipCreatePlayersFromData(outputs[i], players[p], proc_outputs[i], target, doutputs);
				if (tmp.size())
					break;
			}

			if (!tmp.size()) // we cannot insert it: try to create a new player
				tmp = vipCreatePlayersFromData(outputs[i], NULL, proc_outputs[i], target, doutputs);
		}

		players += tmp;

		// remove duplicates
		players = vipListUnique(players);
	}

	if (device)		  // VipProcessingPool * pool = disp->parentObjectPool())
		device->reload(); // read(pool->time());

	return players;
}

QList<VipAbstractPlayer*> vipCreatePlayersFromProcessings(const QList<VipProcessingObject*>& procs, VipAbstractPlayer* pl, QObject* target, QList<VipDisplayObject*>* doutputs)
{
	if (!procs.size())
		return QList<VipAbstractPlayer*>();

	if (pl) {
		for (int i = 0; i < procs.size(); ++i) {
			QList<VipAbstractPlayer*> players = vipCreatePlayersFromProcessing(procs[i], pl, NULL, target, doutputs);
			if (!players.size())
				return QList<VipAbstractPlayer*>();
		}
		return QList<VipAbstractPlayer*>() << pl;
	}
	else {
		// create the new players
		QList<VipAbstractPlayer*> players = vipCreatePlayersFromProcessing(procs.first(), NULL, NULL, target, doutputs);

		for (int i = 1; i < procs.size(); ++i) {
			// try to insert the VipDisplayObject into an existing player
			bool inserted = false;
			for (int p = 0; p < players.size(); ++p) {
				QList<VipAbstractPlayer*> tmp = vipCreatePlayersFromProcessing(procs[i], players[p], NULL, target, doutputs);
				if (tmp.size()) {
					inserted = true;
					break;
				}
			}

			// if not inserted, create a new player (if possible)
			if (!inserted) {
				QList<VipAbstractPlayer*> tmp = vipCreatePlayersFromProcessing(procs[i], NULL, NULL, target, doutputs);
				if (tmp.size()) {
					players.append(tmp);
				}
			}
		}

		// make unique while keeping the order
		return vipListUnique(players);
	}
}

#include "VipProcessingObjectEditor.h"

QList<VipAbstractPlayer*> vipCreatePlayersFromStringList(const QStringList& lst, VipAbstractPlayer* player, QObject* target, QList<VipDisplayObject*>* doutputs)
{
	QList<VipProcessingObject*> devices;

	VipProgress p;
	p.setModal(true);
	p.setCancelable(true);
	if (lst.size() > 1)
		p.setRange(0, lst.size());

	for (int i = 0; i < lst.size(); ++i) {
		VipIODevice* device = VipCreateDevice::create(lst[i]);
		if (device) {
			device->setPath(lst[i]);

			QString name = device->removePrefix(device->name());
			p.setText("<b>Open</b> " + QFileInfo(name).fileName());
			if (lst.size() > 1)
				p.setValue(i);

			// allow VipIODevice to display a progress bar
			device->setProperty("_vip_enableProgress", true);

			if (device->open(VipIODevice::ReadOnly)) {
				devices << device;
				VIP_LOG_INFO("Open " + lst[i]);
			}
			else
				delete device;
		}

		if (p.canceled())
			break;
	}

	QList<VipAbstractPlayer*> res = vipCreatePlayersFromProcessings(devices, player, target, doutputs);
	if (res.isEmpty()) {
		// delete devices
		for (int i = 0; i < devices.size(); ++i)
			delete devices[i];
	}
	return res;
}

QList<VipAbstractPlayer*> vipCreatePlayersFromPaths(const VipPathList& paths, VipAbstractPlayer* player, QObject* target, QList<VipDisplayObject*>* doutputs)
{
	QList<VipProcessingObject*> devices;

	VipProgress p;
	p.setModal(true);
	p.setCancelable(true);
	p.setRange(0, paths.size());

	for (int i = 0; i < paths.size(); ++i) {
		p.setText("Open <b>" + paths[i].canonicalPath() + "</b>");
		VipIODevice* device = VipCreateDevice::create(paths[i]);
		if (device) {
			device->setPath(paths[i].canonicalPath());
			device->setMapFileSystem(paths[i].mapFileSystem());

			if (device->open(VipIODevice::ReadOnly)) {
				devices << device;
				VIP_LOG_INFO("Open " + paths[i].canonicalPath());
			}
			else {
				delete device;
			}
		}

		if (p.canceled())
			break;
	}

	QList<VipAbstractPlayer*> res = vipCreatePlayersFromProcessings(devices, player, target, doutputs);
	if (res.isEmpty()) {
		// delete devices
		for (int i = 0; i < devices.size(); ++i)
			delete devices[i];
	}
	return res;
}

VipProcessingObject* vipCreateProcessing(VipOutput* output, const VipProcessingObject::Info& info)
{
	VipProcessingPool* pool = NULL;
	VipProcessingList* lst = NULL;
	VipProcessingObject* res = info.create();
	if (!res)
		return NULL;
	// check output/input
	if (res->outputCount() == 0)
		goto error;
	if (res->inputCount() != 1) {
		if (VipMultiInput* multi = res->topLevelInputAt(0)->toMultiInput()) {
			if (!multi->resize(1))
				goto error;
		}
		else
			goto error;
	}
	// connect inputs
	if (!pool)
		pool = output->parentProcessing()->parentObjectPool();
	output->setConnection(res->inputAt(0));
	res->inputAt(0)->setData(output->data());

	res->setDeleteOnOutputConnectionsClosed(true);
	res->setParent(pool);
	lst = new VipProcessingList();
	lst->setDeleteOnOutputConnectionsClosed(true);
	lst->setParent(pool);
	lst->inputAt(0)->setConnection(res->outputAt(0));

	// run the processing at least once to have a valid output
	lst->update();
	if (res->hasError())
		goto error;

	res->setScheduleStrategy(VipProcessingObject::Asynchronous);
	lst->setScheduleStrategy(VipProcessingObject::Asynchronous);
	return lst;

error:
	if (res)
		delete res;
	if (lst)
		delete lst;
	return NULL;
}

VipProcessingObject* vipCreateDataFusionProcessing(const QList<VipOutput*>& outputs, const VipProcessingObject::Info& info)
{
	VipProcessingPool* pool = NULL;
	VipProcessingList* lst = NULL;
	VipProcessingObject* res = info.create();
	if (!res)
		return NULL;

	// check input count
	if (res->inputCount() != outputs.size()) {
		if (VipMultiInput* multi = res->topLevelInputAt(0)->toMultiInput()) {
			if (!multi->resize(outputs.size()))
				goto error;
		}
		else
			goto error;
	}
	// check output
	if (res->outputCount() == 0)
		goto error;

	// connect inputs
	for (int i = 0; i < outputs.size(); ++i) {
		if (!pool)
			pool = outputs[i]->parentProcessing()->parentObjectPool();
		outputs[i]->setConnection(res->inputAt(i));
		res->inputAt(i)->setData(outputs[i]->data());
	}

	res->setDeleteOnOutputConnectionsClosed(true);
	res->setParent(pool);
	lst = new VipProcessingList();
	lst->setDeleteOnOutputConnectionsClosed(true);
	lst->setParent(pool);
	lst->inputAt(0)->setConnection(res->outputAt(0));

	// run the processing at least once to have a valid output
	lst->update();
	if (res->hasError())
		goto error;

	res->setScheduleStrategy(VipProcessingObject::Asynchronous);
	lst->setScheduleStrategy(VipProcessingObject::Asynchronous);
	return lst;

error:
	if (res)
		delete res;
	if (lst)
		delete lst;
	return NULL;
}

VipProcessingObject* vipCreateDataFusionProcessing(const QList<VipPlotItem*>& items, const VipProcessingObject::Info& info)
{
	QList<VipOutput*> procs;
	for (int i = 0; i < items.size(); ++i) {
		if (VipDisplayObject* disp = items[i]->property("VipDisplayObject").value<VipDisplayObject*>()) {
			if (VipOutput* out = disp->inputAt(0)->connection()->source())
				procs.append(out);
		}
		else
			return NULL;
	}
	if (procs.size() != items.size())
		return NULL;
	return vipCreateDataFusionProcessing(procs, info);
}
