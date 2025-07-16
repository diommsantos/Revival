#pragma once

#include "Model.h"
#include <string>
#include <QObject>
#include <QList>
#include <QDateTime>
#include <memory>
#include "spdlog/spdlog.h"

#if defined(REVIVAL_LIBRARY)
# define REVIVAL_API Q_DECL_EXPORT
#else
# define REVIVAL_API Q_DECL_IMPORT
#endif

class REVIVAL_API Simulator : public QObject
{
    Q_OBJECT
    using Timestep = std::tuple<TimePoint, MarketHistory, const OrderBook&>;
    friend class SimulatorUI;

    class SimulatorLogger {
    public:
        static enum ActionState {
            Awaiting,
            Processed,
            Pending,
            Cancelled,
            Error,
        };
    private:
        std::shared_ptr<spdlog::logger> timestepLogger;
        std::shared_ptr<spdlog::logger> actionsLogger;
        std::shared_ptr<spdlog::logger> portfolioLogger;
        static const char *actionStateString(ActionState s);
    public:
        SimulatorLogger();
        void logTimestep(TimePoint t, int marketHistoryIndex, int orderBookIndex);
        void logAction(TimePoint t, ActionState s, const MarketOrder &mo);
        void logAction(TimePoint t, ActionState s, const MarketOrder &mo, double processedPrice, double processedQuantity, double total);
        void logAction(TimePoint t, ActionState s, const LimitOrder &lo);
        void logAction(TimePoint t, ActionState s, const LimitOrder &lo, double processedPrice, double processedQuantity, double total);
        void logAction(TimePoint t, ActionState s, const StopOrder &so);
        void logAction(TimePoint t, ActionState s, const StopOrder &so, double processedPrice, double processedQuantity, double total);
        void logAction(TimePoint t, ActionState s, const Cancel &c);
        void logPortfolio(TimePoint t, const Portfolio &p, double value);
    };

public:
    enum TIMESTEP_MODE {
        BOTH,
        ORDER_BOOK,
        MARKET
    };

private:

    SimulatorLogger *logger;

    static const OrderBook initialOrderBook;
    static int nextActionId;

    Portfolio portfolio;
    std::vector<const Action*> pendingActions; //faster if it is std::vector instead of std::list, probably due to way more cache hits
    Model *model;
    TIMESTEP_MODE tsMode;

    std::vector<OrderBook> orderBooks;
    std::vector<Trade> marketHistory;
    std::vector<Timestep> timesteps;
    double makerFee;
    double takerFee;

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
    void processCancel(const Timestep &ts, Cancel &c);
    void processAction(const Timestep &ts, Action *action);
    void processPendingActions(const Timestep &ts);

public:
    Simulator();
    ~Simulator();
    void loadHistoricalData(std::ifstream &&marketData, std::ifstream &&orderBookData);
    void loadHistoricalData(std::string marketFile, std::string orderBookFile);
    void init(Model *model, Portfolio = Portfolio{1000, 0}, TIMESTEP_MODE tsMode = ORDER_BOOK, double makerFee = 0.001, double takerFee = 0.001);
    std::vector<double> run();
    void reset();
    std::vector<double> best();
    std::vector<double> worst();
    std::vector<double> random();
    double getPriceMean();
    double getPriceStdDev();
    double getQuantityMean();
    double getQuantityStdDev();

signals:
    void loadedOrderBookData();
    void loadedMarketData();
    void gotTimesteps();
    void orderFilled(const TimePoint &timePoint, double price, std::shared_ptr<const Action> action);
    void portfolioValueUpdated();
    
};

