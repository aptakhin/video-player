/** VD */
#include <vd/proto.hpp>
#include <vd/mainwindow.hpp>
#include <QApplication>

extern QApplication* app_;

#undef main
int main(int argc, char *argv[])
{	
	vd::Logger logger;
	vd::Logger::i().wl();

	vd::MediaDecoder md;
	QApplication app(argc, argv);
	app_ = &app;
	vd::MainWindow w;
	w.show();
	return app.exec();
}
