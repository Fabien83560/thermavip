#include "VipManualAnnotationHelper.h"
#include "VipCore.h"
#include "VipLogging.h"
#include <qprocess.h>
#include <qfileinfo.h>
#include <qdir.h>

ManualAnnotationHelper::ManualAnnotationHelper()
{

	QString path = vipAppCanonicalPath();
	path = QFileInfo(path).canonicalPath();
	//printf("%s\n", path.toLatin1().data());
	QString thermavip_interface = path + "/Python/thermavip_interface.py";
	QString activate = path + "/miniconda/condabin/activate.bat";

	if (!QFileInfo(thermavip_interface).exists())
		return;

	m_support_segm = QFileInfo(path + "/Python/model_to_mask.py").exists();

	if (QFileInfo(activate).exists()) {

		// Use embedded miniconda installation

		QString cd_path = path + "/Python";
		// printf("%s\n", path.toLatin1().data());
		QString python_path = QFileInfo(vipAppCanonicalPath()).canonicalPath() + "/miniconda/python";
		// printf("%s\n", path.toLatin1().data());

		

		QString cmd = "cmd /c \"cd " + cd_path + " && " + activate + " && " + python_path + " " + thermavip_interface + "\"";
		printf("cmd: '%s'\n", cmd.toLatin1().data());
		// m_process.start(cmd);
		m_process.start("cmd");
		if (!m_process.waitForStarted(5000)) {
			printf("error: %s\n", m_process.errorString().toLatin1().data());
			return;
		}

		m_process.write(("cd " + cd_path + "\n").toLatin1());
		m_process.write((activate + "\n").toLatin1());

		m_process.waitForReadyRead(500);
		// QByteArray out = m_process.readAllStandardError() + m_process.readAllStandardOutput();
		// printf("out: %s\n", out.data());

		m_process.write((python_path + " " + thermavip_interface + "\n").toLatin1());
		m_process.waitForBytesWritten();
	}
	else
	{
		m_process.start(( "python " + thermavip_interface ).toLatin1());
		m_process.waitForStarted();
	}

	QByteArray out;
	while(m_process.waitForReadyRead(3000))
		out += m_process.readAllStandardOutput();
	//m_process.waitForReadyRead(3000);
	//out = m_process.readAllStandardError() + m_process.readAllStandardOutput();
	//printf("out: %s\n", out.data());

	
	//QByteArray out = m_process.readAllStandardOutput();
	if (!out.contains("ready")) {
		printf("out: %s\n", out.data());
		QByteArray err = m_process.readAllStandardError();
		printf("err: %s\n", err.data());
		m_process.kill();
	}
	return;
}

ManualAnnotationHelper::~ManualAnnotationHelper()
{
	if (m_process.state() == QProcess::Running) {
		m_process.write("stop\n");
		m_process.waitForBytesWritten(1000);
		m_process.waitForFinished(1000);
		if (m_process.state() == QProcess::Running)
			m_process.kill();
	}
}

Vip_event_list ManualAnnotationHelper::createFromUserProposal(const QList<QPolygonF>& polygons,
							  Vip_experiment_id pulse,
							  const QString& camera,
							  const QString& device,
								const QString& userName,
							  qint64 time,
							  const QString& type,
							  const QString& filename)
{
	if (m_process.state() != QProcess::Running)
		return Vip_event_list();

	Vip_event_list lst;
	for (int i = 0; i < polygons.size(); ++i)
	{
		VipShape sh;
		sh.setPolygon(polygons[i]);
		QRect r = polygons[i].boundingRect().toRect();
		sh.setAttribute("timestamp_ns", time);
		sh.setAttribute("bbox_x", r.left());
		sh.setAttribute("bbox_y", r.top());
		sh.setAttribute("bbox_width", r.width());
		sh.setAttribute("bbox_height", r.height());
		sh.setAttribute("pixel_area", r.width() * r.height());
		sh.setAttribute("experiment_id", pulse);
		sh.setAttribute("line_of_sight", camera);
		sh.setAttribute("device", device);

		sh.setAttribute("initial_timestamp_ns", time);
		sh.setAttribute("final_timestamp_ns", time);
		sh.setAttribute("duration_ns", 0);
		sh.setAttribute("category", QString("hot spot"));
		sh.setAttribute("is_automatic_detection", 0);
		sh.setAttribute("confidence", 1);
		sh.setAttribute("user", userName);


		lst[i].push_back(sh);
	}

	//create JSON file
	QString file = QDir::tempPath();
	file.replace("\\", "/");
	if (!file.endsWith("/"))
		file += "/";
	file += QString::number(QDateTime::currentMSecsSinceEpoch()) + ".json";
	QString json = file;
	printf("json file: %s\n", json.toLatin1().data());
	//sendToJSON(file, userName, camera, device, pulse, lst);
	vipEventsToJsonFile(file,lst);

	//TEST
	//sendToJSON("C:/Users/VM213788/Desktop/tmp_json.json", userName, camera, pulse, lst);
	//printf("filename: '%s'\n'", filename.toLatin1().data());
	QString cmd = (type + " " + json + (filename.isEmpty() ? QString() : (" " + filename)) + "\n");
	printf("cmd: %s\n", cmd.toLatin1().data());
	m_process.write((type + " " + json + (filename.isEmpty() ? QString() : (" " + filename)) + "\n").toLatin1()); // use 'segm' for segmentation

	VipProgress p;
	p.setRange(0, 100);
	p.setValue(0);
	p.setModal(true);

	while (true) {
		if (m_process.state() != QProcess::Running) {
			m_process.waitForReadyRead(500);
			QByteArray ar = m_process.readAllStandardOutput();
			QByteArray err = m_process.readAllStandardError();
			QString error = m_process.errorString();
			if (error.size())
				VIP_LOG_INFO(error);
			if(ar.size())
				VIP_LOG_INFO(ar);
			if (err.size())
				VIP_LOG_INFO(err);
			VIP_LOG_ERROR("Python manual annotation tool just crashed!");
			return Vip_event_list();
		}
		m_process.waitForReadyRead(500);
		QByteArray ar = m_process.readAllStandardOutput();
		QByteArray err = m_process.readAllStandardError();
		
		if (err.size()) {
			VIP_LOG_INFO(err);
			//printf("err %s\n", err.data());
			int index = err.indexOf("|");
			if (index > 0) {
				err = err.mid(0, index);
				index = err.indexOf(":");
				QString text = err.mid(0, index);
				QString percent = err.mid(index + 1);
				percent.replace("%", "");
				percent.replace(" ", "");
				//printf("txt: '%s'\n", text.toLatin1().data());
				//printf("value: '%s'\n", percent.toLatin1().data());
				double value = percent.toDouble();
				p.setText(text);
				p.setValue(value);
			}
		}
		if(ar.size())
			VIP_LOG_INFO(ar);
		if (ar.contains("finished"))
			break;
	}
	//read the 'ready' flag if not done
	m_process.waitForReadyRead(1000);
	QByteArray ar = m_process.readAllStandardOutput();

	QFile fin(json);
	fin.open(QFile::ReadOnly | QFile::Text);
	QByteArray content = fin.readAll();
	Vip_event_list res = vipEventsFromJson(content);
	if (res.empty()) {
		VIP_LOG_ERROR("Error while loading JSON file ", json.toLatin1().data());
		VIP_LOG_INFO("JSON content:\n", content.data());
		return Vip_event_list();
	}
	fin.close();

	//QFile::remove(json);

	return res;
}

Vip_event_list ManualAnnotationHelper::createBBoxesFromUserProposal(const QList<QPolygonF>& polygons,
								Vip_experiment_id pulse,
								const QString& camera,
								const QString& device,
								const QString& userName,
								qint64 time,
								const QString& filename )
{
	return createFromUserProposal(polygons, pulse, camera, device, userName, time, "bbox", filename);
}
Vip_event_list ManualAnnotationHelper::createSegmentationFromUserProposal(const QList<QPolygonF>& polygons,
								      Vip_experiment_id pulse,
								      const QString& camera,
								      const QString& device,
								      const QString& userName,
								      qint64 time,
								      const QString& filename)
{
	return createFromUserProposal(polygons, pulse, camera, device, userName, time, "segm", filename);
}

static ManualAnnotationHelper* _last = nullptr;

ManualAnnotationHelper* ManualAnnotationHelper::instance()
{
	static ManualAnnotationHelper* inst = new ManualAnnotationHelper();
	if (inst && inst->m_process.state() != QProcess::Running) {
		delete inst;
		_last = nullptr;
		_last = inst = new ManualAnnotationHelper();
	}
	return inst;
}

void ManualAnnotationHelper::deleteInstance()
{
	if (_last)
		delete _last;
}

bool ManualAnnotationHelper::isValidState()
{
	ManualAnnotationHelper* inst = instance();
	if (inst && inst->m_process.state() == QProcess::Running)
		return true;
	return false;
}

bool ManualAnnotationHelper::supportSegmentation()
{
	QString path = vipAppCanonicalPath();
	path = QFileInfo(path).canonicalPath();
	QString model_to_mask = path + "/Python/model_to_mask.py";

	if (!QFileInfo(model_to_mask).exists())
		return false;
	return true;
}

bool ManualAnnotationHelper::supportBBox()
{
	QString path = vipAppCanonicalPath();
	path = QFileInfo(path).canonicalPath();
	QString thermavip_interface = path + "/Python/thermavip_interface.py";
	
	if (!QFileInfo(thermavip_interface).exists())
		return false;
	return true;
}



#include "VipPlayer.h"
#include "VipIODevice.h"
#include "VipProcessMovie.h"

static void extractAnnotationFromPlayer(VipVideoPlayer* pl, const QList<VipShape>& shs, const QString& method = "bbox")
{
	double pulse = 0;
	qint64 time = VipInvalidTime;
	QString camera, device;
	QString user = vipUserName();

	// get pulse
	if (VipDisplayObject* disp = pl->mainDisplayObject()) {
		VipAnyData in = disp->inputAt(0)->probe();
		if (in.hasAttribute("Pulse"))
			pulse = (in.attribute("Pulse").toDouble());
	}
	// get camera
	if (VipDisplayObject* disp = pl->mainDisplayObject()) {
		VipAnyData in = disp->inputAt(0)->probe();
		camera = in.attribute("Camera").toString();
	}
	// get device
	if (VipDisplayObject* disp = pl->mainDisplayObject()) {
		VipAnyData in = disp->inputAt(0)->probe();
		device = in.attribute("Device").toString();
	}
	// get time
	if (pl->processingPool())
		time = pl->processingPool()->time();

	if (pulse == 0) {
		VIP_LOG_WARNING("Pulse is 0!");
		// VIP_LOG_ERROR("Unable to retrieve pulse number from player");
		// return;
	}
	if (camera.isEmpty()) {
		camera = "Unknown";
		VIP_LOG_WARNING("Unknown camera!");
	}

	QString filename;
	VipIODeviceList devices = vipListCast<VipIODevice*>(pl->mainDisplayObject()->allSources());
	if (devices.size() == 1) {
		QString path = devices[0]->path();
		path = devices[0]->removePrefix(path);
		if (QFileInfo(path).exists())
			filename = path;
	}

	if (time == VipInvalidTime) {
		VIP_LOG_ERROR("Wrong player time value");
		return;
	}

	// get helper
	ManualAnnotationHelper* helper = ManualAnnotationHelper::instance();
	if (!helper) {
		VIP_LOG_ERROR("unable to create Python process for manual annotation helper tool");
		return;
	}

	VIP_LOG_INFO("Extract bounding boxes for pulse ", pulse, ", camera ", camera, ", at time ", time / 1000000000., " seconds", pulse);

	// transform bounding box
	QList<QPolygonF> polygons;
	for (int i = 0; i < shs.size(); ++i)
		polygons.append(pl->imageTransform().inverted().map(shs[i].polygon()));

	// QRect rect = pl->imageTransform().inverted().map(r).boundingRect().toRect();

	Vip_event_list lst = helper->createFromUserProposal(polygons, pulse, camera, device, user, time, method, filename);

	if (lst.size()) {

		// remove drawn shapes
		for (int i = 0; i < shs.size(); ++i)
			pl->plotSceneModel()->sceneModel().remove(shs[i]);
		VipPlayerDBAccess::fromPlayer(pl)->addEvents(lst, false);
	}
}

static void extractBBoxFromPlayer(VipVideoPlayer* pl, const QList<VipShape>& shs)
{
	extractAnnotationFromPlayer(pl, shs, "bbox");
}

static void extractSegmFromPlayer(VipVideoPlayer* pl, const QList<VipShape>& shs)
{
	extractAnnotationFromPlayer(pl, shs, "segm");
}

static QList<QAction*> manualAnnotationHelperMenu(VipPlotShape* shape, VipVideoPlayer* p)
{
	(void)shape;

	QList<QAction*> actions;

	if (!ManualAnnotationHelper::supportBBox())
		return actions;

	// VipShape sh = shape->rawData();
	// QRectF r;
	QList<VipPlotShape*> shapes = p->plotSceneModel()->shapes(1);
	QList<VipShape> shs;
	for (int i = 0; i < shapes.size(); ++i) {
		if (shapes[i]->rawData().type() == VipShape::Polygon || shapes[i]->rawData().type() == VipShape::Path)
			shs.append(shapes[i]->rawData());
	}
	if (shs.size()) { // p->plotSceneModel()->shapes(1).contains(shape) && sh.type() == VipShape::polygon /*&& vipIsRect(sh.polygon(), &r)*/) {
		QAction* extract = new QAction("Create event with bounding boxes", NULL);
		QObject::connect(extract, &QAction::triggered, std::bind(extractBBoxFromPlayer, p, shs));
		actions << extract;

		if (ManualAnnotationHelper::supportSegmentation()) {
			QAction* segm = new QAction("Create event with segmentation masks", NULL);
			QObject::connect(segm, &QAction::triggered, std::bind(extractSegmFromPlayer, p, shs));
			actions << segm;
		}
	}

	if (actions.size()) {
		// add separator at the beginning
		QAction* a = new QAction(NULL);
		a->setSeparator(true);
		actions.insert(0, a);
	}
	return actions;
}


static int registerManualAnnotationHelperMenu()
{
	VipFDItemRightClick().append<QList<QAction*>(VipPlotShape*, VipVideoPlayer*)>(manualAnnotationHelperMenu);
	return 0;
}

static bool initAnnotationHelper = vipAddInitializationFunction(registerManualAnnotationHelperMenu);