#pragma once

#include <memory>
#include <QMainWindow>
#include "qcustomplot.h"
#include "Model.h"
#include "Simulator.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class Simulator;
}
QT_END_NAMESPACE

class SimulatorUI : public QMainWindow
{
private:
    Ui::Simulator *ui;
    QCustomPlot *portfValuePlot;
    QCustomPlot *marketHistPlot;
    QPushButton *runButton;
private slots:
    void plotMarketHistory();
    void plotBWR();
    void plotPortfolioValue();
    void plotOrder(const TimePoint &timePoint, double price, std::shared_ptr<const Action> action);
public:
    Simulator *sim;
    SimulatorUI(QWidget *parent = nullptr);
};
