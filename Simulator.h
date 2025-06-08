#pragma once

#include "Model.h"
#include <string>
#include <QObject>
#include <QList>
#include <QDateTime>
#include <memory>
#include "SimulatorUI.h"

class Q_DECL_EXPORT Simulator : public QObject
{
    Q_OBJECT
    using Timestep = std::tuple<TimePoint, MarketHistory, const OrderBook&>;
    friend class SimulatorUI;

public:
    enum TIMESTEP_MODE {
        BOTH,
        ORDER_BOOK,
        MARKET
    };

private:
    SimulatorUI ui;

    static const OrderBook initialOrderBook;
    static int nextActionId;

    Portfolio portfolio;
    std::vector<const Action*> pendingActions; //faster if it is std::vector instead of std::list, probably due to way more cache hits
    Model *model;
    TIMESTEP_MODE tsMode;

    std::vector<OrderBook> orderBooks;
    std::vector<Trade> marketHistory;
    std::vector<Timestep> timesteps;

    std::vector<double> portfolioValue;

    void fromJSONString(std::string &str, std::vector<Order> &v);
    void loadMarketData(std::ifstream &market);
    void loadOrderBookData(std::ifstream &orderBookData);

    std::vector<Timestep> getBothTimesteps();
    std::vector<Timestep> getOrderBookTimesteps();
    std::vector<Timestep> getMarketTimesteps();

    void processMarketOrder(const Timestep &ts, MarketOrder &mo);
    void processLimitOrder(const Timestep &ts, LimitOrder &lo);
    void processStopOrder(const Timestep &ts, StopOrder &so);
    void processCancel(Cancel &c);
    void processAction(const Timestep &ts, Action *action);
    void processPendingActions(const Timestep &ts);

public:
    Simulator();
    ~Simulator();
    void loadHistoricalData(std::ifstream &&marketData, std::ifstream &&orderBookData);
    void loadHistoricalData(std::string marketFile, std::string orderBookFile);
    void init(Model *model, Portfolio = Portfolio{1000, 0}, TIMESTEP_MODE tsMode = ORDER_BOOK);
    std::vector<double> run();
    void reset();
    std::vector<double> best();
    std::vector<double> worst();
    std::vector<double> random();
    double getPriceMean();
    double getPriceStdDev();
    double getQuantityMean();
    double getQuantityStdDev();
    void show();

signals:
    void loadedOrderBookData();
    void loadedMarketData();
    void gotTimesteps();
    void orderFilled(const TimePoint &timePoint, double price, std::shared_ptr<const Action> action);
    void portfolioValueUpdated();
    
};

