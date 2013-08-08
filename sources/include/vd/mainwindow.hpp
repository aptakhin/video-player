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
	std::unique_ptr<Preview> preview_;
	std::unique_ptr<QThread> preview_thread_;
	
	QStateMachine machine_;

	std::unique_ptr<QState> play_state_;
	std::unique_ptr<QState> pause_state_;
};

} // namespace xdd
