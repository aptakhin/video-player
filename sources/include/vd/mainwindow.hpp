/** VD */
#pragma once

#include <QMainWindow>
#include <QStateMachine>
#include <vd/proto.hpp>

namespace Ui {
	class MainWindow;
}

namespace vd {

class MainWindow : public QMainWindow
{
	Q_OBJECT

public:

	explicit MainWindow(QWidget *parent = 0);
	~MainWindow();

public slots:

	void audio_volume_changed(int volume);

private /*overriden*/:


protected:
	void closeEvent(QCloseEvent* ev);

	void keyPressEvent(QKeyEvent* ev);

private:
	::Ui::MainWindow* ui;
	ProjectUPtr project_;
	Preview* preview_;
	QThread* preview_thread_;
	
	QStateMachine machine_;

	QState* play_state_;
	QState* pause_state_;
};

} // namespace xdd
