#include "SimulatorUI.h"
#include "Simulator.h"
#include "./ui_simulator.h"

SimulatorUI::SimulatorUI(QWidget* parent) :
sim{new Simulator},
QMainWindow{parent},
ui{new Ui::Simulator}
{
    ui->setupUi(this);
    portfValuePlot = static_cast<QCustomPlot*>(ui->verticalLayout->itemAt(0)->widget());
    marketHistPlot = static_cast<QCustomPlot*>(ui->verticalLayout->itemAt(1)->widget());
    runButton = ui->runButton;

    QObject::connect(sim, &Simulator::loadedMarketData, this, &SimulatorUI::plotMarketHistory);
    QObject::connect(sim, &Simulator::gotTimesteps, this, &SimulatorUI::plotBWR);
    QObject::connect(sim, &Simulator::gotTimesteps, [this](){portfValuePlot->addGraph();});
    QObject::connect(portfValuePlot->xAxis, QOverload<const QCPRange &>::of(&QCPAxis::rangeChanged), [this](const QCPRange &newRange){
        marketHistPlot->xAxis->setRange(newRange.lower, newRange.upper);
        marketHistPlot->replot(QCustomPlot::rpQueuedReplot);
    });
    QObject::connect(marketHistPlot->xAxis, QOverload<const QCPRange &>::of(&QCPAxis::rangeChanged), [this](const QCPRange &newRange){
        portfValuePlot->xAxis->setRange(newRange.lower, newRange.upper);
        portfValuePlot->replot(QCustomPlot::rpQueuedReplot);
    });
    portfValuePlot->setInteraction(QCP::iRangeDrag, true);
    portfValuePlot->setInteraction(QCP::iRangeZoom, true);
    marketHistPlot->setInteraction(QCP::iRangeDrag, true);
    marketHistPlot->setInteraction(QCP::iRangeZoom, true);
    
    QObject::connect(runButton, &QPushButton::clicked, [this](bool checked){
        QThreadPool::globalInstance()->start([this](){sim->run();});
    });
    QObject::connect(sim, &Simulator::portfolioValueUpdated, this, &SimulatorUI::plotPortfolioValue);
    QObject::connect(sim, &Simulator::orderFilled, this, &SimulatorUI::plotOrder, Qt::QueuedConnection);
    setFocusPolicy(Qt::StrongFocus);
}

void SimulatorUI::plotMarketHistory(){
    QVector<double> x;
    QVector<double> y;
    x.reserve(sim->marketHistory.size());
    y.reserve(sim->marketHistory.size());
    for(const auto& t: sim->marketHistory){
        x.append(t.timestamp.count());
        y.append(t.price);
    }
    double maxY = *std::max_element(y.begin(), y.end());
    double minY = *std::min_element(y.begin(), y.end());
    marketHistPlot->yAxis->setRange(minY, maxY);
    marketHistPlot->addGraph();
    marketHistPlot->graph(0)->setData(x, y);
    marketHistPlot->replot();
}

void SimulatorUI::plotBWR(){
    QVector<double> x;
    std::vector<double> bv = sim->best();
    QVector<double> best{bv.begin(), bv.end()};
    std::vector<double> wv = sim->worst();
    QVector<double> worst{wv.begin(), wv.end()};
    std::vector<double> rv = sim->random();
    QVector<double> random{rv.begin(), rv.end()};
    for(const auto& ts: sim->timesteps){
        x.append(std::get<0>(ts).count());
    }

    double maxY = *std::max_element(best.begin(), best.end());
    double minY = *std::min_element(worst.begin(), worst.end());
    portfValuePlot->xAxis->setRange(x[0], x.back());
    portfValuePlot->yAxis->setRange(minY, maxY);
    
    portfValuePlot->addGraph();
    portfValuePlot->graph(0)->setData(x, best);
    portfValuePlot->graph(0)->setPen(QPen(Qt::green));
    portfValuePlot->addGraph();
    portfValuePlot->graph(1)->setData(x, worst);
    portfValuePlot->graph(1)->setPen(QPen(Qt::red));
    portfValuePlot->addGraph();
    portfValuePlot->graph(2)->setData(x, random);
    portfValuePlot->graph(2)->setPen(QPen(Qt::lightGray));
}

void SimulatorUI::plotPortfolioValue(){
    QVector<double> x;
    QVector<double> y;
    x.reserve(sim->portfolioValue.size());
    y.reserve(x.capacity());
    for(int i = 0; i<sim->portfolioValue.size(); i++){
        x.push_back(std::get<0>(sim->timesteps[i]).count());
        y.push_back(sim->portfolioValue[i]);
    }

    portfValuePlot->graph(3)->setData(x, y);
    portfValuePlot->graph(3)->setPen(QPen(Qt::black));
    portfValuePlot->replot(QCustomPlot::rpQueuedReplot);
}

void SimulatorUI::plotOrder(const TimePoint &timePoint, double price, std::shared_ptr<const Action> action){
    QCPItemTracer *tracer = new QCPItemTracer(marketHistPlot);
    switch (action->index())
    {
    case Action::typeIndex<MarketOrder>():{
        const MarketOrder &mo = static_cast<const MarketOrder &>(*action);
        if(mo.actionType == BUY){
            tracer->setPen(QPen(Qt::green));
        }else{
            tracer->setPen(QPen(Qt::red));
        }
        break;
    }case Action::typeIndex<LimitOrder>():{
        const LimitOrder &lo = static_cast<const LimitOrder &>(*action);
        if(lo.actionType == BUY){
            tracer->setPen(QPen(Qt::green));
        }else{
            tracer->setPen(QPen(Qt::red));
        }
        break;
    }case Action::typeIndex<StopOrder>():{
        const StopOrder &so = static_cast<const StopOrder &>(*action);
        if(so.actionType == BUY){
            tracer->setPen(QPen(Qt::green));
        }else{
            tracer->setPen(QPen(Qt::red));
        }
        break;
    }default:
        break;
    }
    tracer->setGraph(marketHistPlot->graph(0));
    tracer->setGraphKey(timePoint.count());
    tracer->setStyle(QCPItemTracer::tsSquare);
}