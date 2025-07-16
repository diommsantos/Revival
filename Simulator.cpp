#include "Simulator.h"
#include <fstream>
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonArray>
#include <algorithm>
#include <cmath>
#include <QThreadPool>
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/async.h"

Simulator::SimulatorLogger::SimulatorLogger()
{
    std::string dirPath{QCoreApplication::applicationDirPath().toStdString()+"/Logs/"};
    std::string currentTime{QDateTime::currentDateTime().toString("dd-MM-yyyy-HH'h'mm'm'").toStdString()};
    timestepLogger = spdlog::basic_logger_mt<spdlog::async_factory>("timestepLogger", dirPath+"timesteps-"+currentTime+".csv");
    actionsLogger = spdlog::basic_logger_mt<spdlog::async_factory>("actionsLogger", dirPath+"actions-"+currentTime+".csv");
    portfolioLogger = spdlog::basic_logger_mt<spdlog::async_factory>("portfolioLogger", dirPath+"portfolio-"+currentTime+".csv");

    timestepLogger->set_pattern("%v");
    actionsLogger->set_pattern("%v");
    portfolioLogger->set_pattern("%v");

    timestepLogger->info("timepoint, orderBookIndex, marketHistoryIndex");
    actionsLogger->info("timepoint, state, id, type, side, quantity, price, orderId, processedQuantity, processedPrice, total");
    portfolioLogger->info("timestamp, timepoint, authMoney, pendingMoney, authQuantity, pendingQuantity, value");
};

const char* Simulator::SimulatorLogger::actionStateString(ActionState s){
    switch(s){
    case ActionState::Awaiting: return "a";
    case ActionState::Processed: return "f";
    case ActionState::Pending: return "p";
    case ActionState::Cancelled: return "c";
    case ActionState::Error: return "e";
    }
};

void Simulator::SimulatorLogger::logTimestep(TimePoint t, int marketHistoryIndex, int orderBookIndex){
    timestepLogger->info("{}, {}, {}", t.count(), orderBookIndex, marketHistoryIndex);
};

void Simulator::SimulatorLogger::logAction(TimePoint t, ActionState s, const MarketOrder &mo){
    actionsLogger->info("{}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}", t.count(), actionStateString(s), mo.actionId, 
        "M", mo.actionType == BUY ? "b" : "s", mo.quantity, "", "", "", "", "");
};

void Simulator::SimulatorLogger::logAction(TimePoint t, ActionState s, const MarketOrder &mo, double processedQuantity, double processedPrice, double total){
    actionsLogger->info("{}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}", t.count(), actionStateString(s), mo.actionId, 
        "M", mo.actionType == BUY ? "b" : "s", mo.quantity, "", "", processedQuantity, processedPrice, total);
};

void Simulator::SimulatorLogger::logAction(TimePoint t, ActionState s, const LimitOrder &lo){
    actionsLogger->info("{}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}", t.count(), actionStateString(s), lo.actionId, 
        "L", lo.actionType == BUY ? "b" : "s", lo.quantity, lo.price, "", "", "", "");
};

void Simulator::SimulatorLogger::logAction(TimePoint t, ActionState s, const LimitOrder &lo, double processedQuantity, double processedPrice, double total){
    actionsLogger->info("{}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}", t.count(), actionStateString(s), lo.actionId, 
        "L", lo.actionType == BUY ? "b" : "s", lo.quantity, lo.price, "", processedQuantity, processedPrice, total);
};

void Simulator::SimulatorLogger::logAction(TimePoint t, ActionState s, const StopOrder &so){
    actionsLogger->info("{}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}", t.count(), actionStateString(s), so.actionId, 
        "S", so.actionType == BUY ? "b" : "s", so.quantity, so.price, "", "", "", "");
};

void Simulator::SimulatorLogger::logAction(TimePoint t, ActionState s, const StopOrder &so, double processedQuantity, double processedPrice, double total){
    actionsLogger->info("{}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}", t.count(), actionStateString(s), so.actionId, 
        "S", so.actionType == BUY ? "b" : "s", so.quantity, so.price, "", processedQuantity, processedPrice, total);
};

void Simulator::SimulatorLogger::logAction(TimePoint t, ActionState s, const Cancel &c){
    actionsLogger->info("{}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}", t.count(), actionStateString(s), c.actionId, 
        "C", "", "", "", c.orderId, "", "", "");
};

const OrderBook Simulator::initialOrderBook{0, TimePoint::zero(), std::vector<Order>(), std::vector<Order>()};
int Simulator::nextActionId{1};

Simulator::Simulator()
{
}

Simulator::~Simulator()
{
}


void Simulator::fromJSONString(std::string &str, std::vector<Order> &v){
    QJsonDocument json = QJsonDocument::fromJson(QByteArray::fromStdString(str));
    QJsonArray array = json.array();
    v.resize(array.size());
    for(int i = 0; i < array.size(); i++){
        QJsonArray order = array[i].toArray();
        v[i].price = order[0].toDouble();
        v[i].quantity = order[1].toDouble();
    }
}

void Simulator::loadMarketData(std::ifstream &marketData){
    for(int i = 0; marketData.peek() != EOF; i++){
        Trade trade;

        std::string tradeId;
        std::getline(marketData, tradeId, ',');
        trade.tradeId = std::stoll(tradeId);

        std::string price;
        std::getline(marketData, price, ',');
        trade.price = std::stod(price);

        std::string quantity;
        std::getline(marketData, quantity, ',');
        trade.quantity = std::stod(quantity);

        std::getline(marketData, tradeId, ',');
        std::getline(marketData, tradeId, ',');

        std::string timestamp;
        std::getline(marketData, timestamp, ',');
        trade.timestamp = duration_cast<TimePoint>(std::chrono::microseconds(std::stoull(timestamp)));

        marketHistory.push_back(trade);
        std::getline(marketData, tradeId);
    }
    loadedMarketData();
}

void Simulator::loadOrderBookData(std::ifstream &orderBookData){

    for(int i = 0; orderBookData.peek() != EOF; i++){
        OrderBook orderBook;

        std::string lastUpdateId;
        std::getline(orderBookData, lastUpdateId, ',');
        orderBook.lastUpdateId = std::stoll(lastUpdateId);

        std::string E;
        std::getline(orderBookData, E, ',');
        orderBook.E = TimePoint(std::stoull(E));

        std::string asks;
        std::getline(orderBookData, asks, '\"');
        std::getline(orderBookData, asks, '\"');
        fromJSONString(asks, orderBook.asks);
        std::sort(orderBook.asks.begin(), orderBook.asks.end(), [](const Order& a, const Order& b) -> bool{
            return a.price > b.price ? true : false; 
        });

        std::string bids;
        std::getline(orderBookData, bids, '\"');
        std::getline(orderBookData, bids, '\"');
        fromJSONString(bids, orderBook.bids);
        std::sort(orderBook.bids.begin(), orderBook.bids.end(), [](const Order& a, const Order& b) -> bool{
            return a.price < b.price ? true : false; 
        });

        orderBooks.push_back(orderBook);
        
        std::getline(orderBookData, lastUpdateId);
    }
    loadedOrderBookData();
}

void Simulator::loadHistoricalData(std::ifstream &&marketData, std::ifstream &&orderBookData){
    loadOrderBookData(orderBookData);
    loadMarketData(marketData);
};

void Simulator::loadHistoricalData(std::string marketFile, std::string orderBookFile){
    loadHistoricalData(std::ifstream(marketFile), std::ifstream(orderBookFile));
};

void Simulator::init(Model *model, Portfolio portfolio, TIMESTEP_MODE tsMode, double makerFee, double takerFee){
    this->logger = new SimulatorLogger(); 
    this->model = model;
    this->portfolio = portfolio;
    this->tsMode = tsMode;
    this->makerFee = makerFee;
    this->takerFee = takerFee;
    switch(tsMode){
        case BOTH:
            timesteps = getBothTimesteps();
            break;
        case ORDER_BOOK:
            timesteps = getOrderBookTimesteps();
            break;
        case MARKET:
            timesteps = getMarketTimesteps();
    }
    gotTimesteps();
}

std::vector<Simulator::Timestep> Simulator::getBothTimesteps(){
    std::vector<Timestep> r{};
    for(int i = 0, j = 0; i < orderBooks.size() || j < marketHistory.size(); ){
        TimePoint orderBookTime{TimePoint::max()};
        TimePoint marketTime{TimePoint::max()};
        if(i < orderBooks.size())
            orderBookTime = orderBooks[i].E;
        if(j < marketHistory.size())
            marketTime = marketHistory[j].timestamp;

        if(orderBookTime > marketTime){
            j++;
            r.push_back(Timestep(
                marketTime, 
                MarketHistory{std::span(marketHistory).subspan(0, j)},
                (i-1 >= 0 ? orderBooks[i-1] : initialOrderBook)));
            logger->logTimestep(marketTime, j, i-1);
        }
        else{    //TODO: consider the case orderBookTime == marketTime
            i++;
            r.push_back(Timestep(
                orderBookTime, 
                MarketHistory{std::span(marketHistory).subspan(0, j)},
                orderBooks[i-1]));
            logger->logTimestep(orderBookTime, j, i-1);
        }
    }
    return r;
}

std::vector<Simulator::Timestep> Simulator::getOrderBookTimesteps(){

    std::vector<Timestep> r{};
    for(int i = 0, j = 0; i < orderBooks.size(); i++){
        TimePoint orderBookTime{orderBooks[i].E};
        TimePoint marketTime{TimePoint::max()};
        if(j < marketHistory.size())
        marketTime = marketHistory[j].timestamp;

        if(orderBookTime > marketTime){
            while(j < marketHistory.size() && marketHistory[j].timestamp <= orderBookTime)
                j++;
        }

        r.push_back(Timestep(
            orderBookTime, 
            MarketHistory{std::span(marketHistory).subspan(0, j)},
            orderBooks[i]));
        logger->logTimestep(orderBookTime, j, i);
    }
    return r;
}

std::vector<Simulator::Timestep> Simulator::getMarketTimesteps(){
    std::vector<Timestep> r{};
    for(int i = 0, j = 0; i < orderBooks.size() || j < marketHistory.size(); ){
        TimePoint orderBookTime{TimePoint::max()};
        TimePoint marketTime{TimePoint::max()};
        if(i < orderBooks.size())
        orderBookTime = orderBooks[i].E;
        if(j < marketHistory.size())
        marketTime = marketHistory[j].timestamp;

        if(marketTime > orderBookTime){
            while(i < orderBooks.size() && orderBooks[i].E <= marketTime)
                i++;
        }

        marketTime++;

        r.push_back(Timestep(
            marketTime, 
            MarketHistory{std::span(marketHistory).subspan(0, j)},
            orderBooks[i-1]));
        logger->logTimestep(marketTime, j, i-1);
    }
    return r;
};

void Simulator::processPendingActions(const Timestep &ts){
    int i = 0;
    while(i < pendingActions.size()){
        const Action &action = *pendingActions[i];
        switch (action.index())
        {
        case Action::typeIndex<LimitOrder>():{
            const LimitOrder &lo = static_cast<const LimitOrder&>(action);
            if(lo.actionType == BUY && std::get<1>(ts).trades.back().price <= lo.price){
                portfolio.pendingMoney -= lo.quantity * lo.price;
                portfolio.authQuantity += lo.quantity * (1-makerFee);
                logger->logAction(std::get<0>(ts), SimulatorLogger::ActionState::Processed, lo, lo.quantity * (1-makerFee), lo.price, lo.quantity * lo.price);
            }else if(lo.actionType == SELL && std::get<1>(ts).trades.back().price >= lo.price){
                portfolio.pendingQuantity -= lo.quantity;
                portfolio.authMoney += lo.quantity * lo.price * (1-makerFee);
                logger->logAction(std::get<0>(ts), SimulatorLogger::ActionState::Processed, lo, lo.quantity, lo.price, lo.quantity * lo.price * (1-makerFee));
            }else break;
            emit orderFilled(std::get<0>(ts), lo.price, std::shared_ptr<const LimitOrder>(&lo));
            pendingActions.erase(pendingActions.begin()+i);
            break;
        }case Action::typeIndex<StopOrder>():{
            const StopOrder &so = static_cast<const StopOrder&>(action);
            if(so.actionType == BUY && std::get<1>(ts).trades.back().price >= so.price){
                double total{so.quantity * so.price};
                portfolio.pendingMoney -= total;
                portfolio.authQuantity += total / std::get<1>(ts).trades.back().price * (1-takerFee);
                logger->logAction(std::get<0>(ts), SimulatorLogger::ActionState::Processed, so, total / std::get<1>(ts).trades.back().price * (1-takerFee), std::get<1>(ts).trades.back().price, total);
            }else if(so.actionType == SELL && std::get<1>(ts).trades.back().price <= so.price){
                portfolio.pendingQuantity -= so.quantity;
                portfolio.authMoney += so.quantity * std::get<1>(ts).trades.back().price * (1-takerFee);
                logger->logAction(std::get<0>(ts), SimulatorLogger::ActionState::Processed, so, so.quantity, std::get<1>(ts).trades.back().price, so.quantity * std::get<1>(ts).trades.back().price * (1-takerFee));
            }else break;
            emit orderFilled(std::get<0>(ts), std::get<1>(ts).trades.back().price, std::shared_ptr<const StopOrder>(&so));
            pendingActions.erase(pendingActions.begin()+i);
            break;
        }default:
            break;
        }
        i++;
    }
}

void Simulator::processMarketOrder(const Timestep &ts, MarketOrder &mo){
    logger->logAction(std::get<0>(ts), Simulator::SimulatorLogger::Awaiting, mo);
    if(mo.actionType == BUY){
        double total {mo.quantity * std::get<1>(ts).trades.back().price};
        if(!(0 <= mo.quantity && total <= portfolio.authMoney)){
            logger->logAction(std::get<0>(ts), Simulator::SimulatorLogger::Error, mo);
            delete &mo;
            return;
        }
        portfolio.authMoney -= total;
        portfolio.authQuantity += mo.quantity * (1-takerFee);
        logger->logAction(std::get<0>(ts), Simulator::SimulatorLogger::Processed, mo, mo.quantity * (1-takerFee), std::get<1>(ts).trades.back().price, total);
        emit orderFilled(std::get<0>(ts), std::get<1>(ts).trades.back().price, std::shared_ptr<const MarketOrder>(&mo));
    }else if(mo.actionType == SELL){
        if(!(0 <= mo.quantity && mo.quantity <= portfolio.authQuantity)){
            logger->logAction(std::get<0>(ts), Simulator::SimulatorLogger::Error, mo);
            delete &mo;
            return;
        }
        portfolio.authQuantity -= mo.quantity;
        portfolio.authMoney += mo.quantity * std::get<1>(ts).trades.back().price * (1-takerFee);
        logger->logAction(std::get<0>(ts), Simulator::SimulatorLogger::Processed, mo, mo.quantity, std::get<1>(ts).trades.back().price, mo.quantity * std::get<1>(ts).trades.back().price * (1-takerFee));
        emit orderFilled(std::get<0>(ts), std::get<1>(ts).trades.back().price, std::shared_ptr<const MarketOrder>(&mo));
    }    
}

void Simulator::processLimitOrder(const Timestep &ts, LimitOrder& lo){
    logger->logAction(std::get<0>(ts), Simulator::SimulatorLogger::Awaiting, lo);
    if(lo.actionType == BUY){
        double total {lo.quantity * lo.price};
        if(!(0 <= lo.quantity && 0 <= lo.price && total <= portfolio.authMoney)){
            logger->logAction(std::get<0>(ts), Simulator::SimulatorLogger::Error, lo);
            delete &lo;
            return;
        }
        if(!(std::get<1>(ts).trades.back().price <= lo.price)){
            portfolio.authMoney -= total;
            portfolio.pendingMoney += total;
            lo.actionId = nextActionId; nextActionId++;
            pendingActions.push_back(&lo);
            logger->logAction(std::get<0>(ts), Simulator::SimulatorLogger::Pending, lo);
            return;
        }
        portfolio.authMoney -= total;
        portfolio.authQuantity += lo.quantity * (1-takerFee);
        logger->logAction(std::get<0>(ts), Simulator::SimulatorLogger::Processed, lo, lo.quantity * (1-takerFee), lo.price, total);
        emit orderFilled(std::get<0>(ts), lo.price, std::shared_ptr<const LimitOrder>(&lo));
    }else if(lo.actionType == SELL){
        if(!(0 <= lo.quantity && 0 <= lo.price && lo.quantity <= portfolio.authQuantity)){
            logger->logAction(std::get<0>(ts), Simulator::SimulatorLogger::Error, lo);
            delete &lo;
            return;
        }
        if(!(std::get<1>(ts).trades.back().price >= lo.price)){
            portfolio.authQuantity -= lo.quantity;
            portfolio.pendingQuantity += lo.quantity;
            lo.actionId = nextActionId; nextActionId++;
            pendingActions.push_back(&lo);
            logger->logAction(std::get<0>(ts), Simulator::SimulatorLogger::Pending, lo);
            return;
        }
        portfolio.authQuantity -= lo.quantity;
        portfolio.authMoney += lo.quantity * lo.price * (1-takerFee);
        logger->logAction(std::get<0>(ts), Simulator::SimulatorLogger::Processed, lo, lo.quantity, lo.price, lo.quantity * lo.price * (1-takerFee));
        emit orderFilled(std::get<0>(ts), lo.price, std::shared_ptr<const LimitOrder>(&lo));
    }
}

void Simulator::processStopOrder(const Timestep &ts, StopOrder &so){
    logger->logAction(std::get<0>(ts), Simulator::SimulatorLogger::Awaiting, so);
    if(so.actionType == BUY){
        double total {so.quantity * so.price};
        if(!(0 <= so.quantity && 0 <= so.price && total <= portfolio.authMoney)){
            logger->logAction(std::get<0>(ts), Simulator::SimulatorLogger::Error, so);
            delete &so;
            return;
        }
        if(!(std::get<1>(ts).trades.back().price >= so.price)){
            portfolio.authMoney -= total;
            portfolio.pendingMoney += total;
            so.actionId = nextActionId; nextActionId++;
            pendingActions.push_back(&so);
            logger->logAction(std::get<0>(ts), Simulator::SimulatorLogger::Pending, so);
            return;
        }
        portfolio.authMoney -= total;
        portfolio.authQuantity += total / std::get<1>(ts).trades.back().price * (1-takerFee);
        logger->logAction(std::get<0>(ts), Simulator::SimulatorLogger::Processed, so, total / std::get<1>(ts).trades.back().price * (1-takerFee), std::get<1>(ts).trades.back().price, total);
        emit orderFilled(std::get<0>(ts), std::get<1>(ts).trades.back().price, std::shared_ptr<StopOrder>(&so));

    }else if(so.actionType == SELL){
        if(!(0 <= so.quantity && 0 <= so.price && so.quantity <= portfolio.authQuantity)){
            logger->logAction(std::get<0>(ts), Simulator::SimulatorLogger::Error, so);
            delete &so;
            return;
        }
        if(!(std::get<1>(ts).trades.back().price <= so.price)){
            portfolio.authQuantity -= so.quantity;
            portfolio.pendingQuantity += so.quantity;
            so.actionId = nextActionId; nextActionId++;
            pendingActions.push_back(&so);
            logger->logAction(std::get<0>(ts), Simulator::SimulatorLogger::Pending, so);
            return;
        }
        portfolio.authQuantity -= so.quantity;
        portfolio.authMoney += so.quantity * std::get<1>(ts).trades.back().price * (1-takerFee);
        logger->logAction(std::get<0>(ts), Simulator::SimulatorLogger::Processed, so, so.quantity, std::get<1>(ts).trades.back().price, so.quantity * std::get<1>(ts).trades.back().price * (1-takerFee));
        emit orderFilled(std::get<0>(ts), std::get<1>(ts).trades.back().price, std::shared_ptr<const StopOrder>(&so));
    }
}

void Simulator::processCancel(const Timestep &ts, Cancel &c){
    auto cit = std::find_if(pendingActions.begin(), pendingActions.end(), 
        [&c](const Action* a){return c.orderId == a->actionId;});
    const Action &cancelledAction{**cit};
    if(cit == pendingActions.end()){
        logger->logAction(std::get<0>(ts), Simulator::SimulatorLogger::Error, c);
        delete &c;
        return;
    }
    if(cancelledAction.index() == Action::typeIndex<LimitOrder>()){
        const LimitOrder &lo = static_cast<const LimitOrder&>(cancelledAction);
        if(lo.actionType == BUY){
            double total {lo.quantity * lo.price};
            portfolio.authMoney += total;
            portfolio.pendingMoney -= total;
        }else if(lo.actionType == SELL){
            portfolio.authQuantity += lo.quantity;
            portfolio.pendingQuantity -= lo.quantity;
        }
        logger->logAction(std::get<0>(ts), Simulator::SimulatorLogger::Cancelled, lo);
    }else if(cancelledAction.index() == Action::typeIndex<StopOrder>()){
        const StopOrder &so = static_cast<const StopOrder&>(cancelledAction);
        if(so.actionType == BUY){
            double total {so.quantity * so.price};
            portfolio.authMoney += total;
            portfolio.pendingMoney -= total;
        }else if(so.actionType == SELL){
            portfolio.authQuantity += so.quantity;
            portfolio.pendingQuantity -= so.quantity;
        }
        logger->logAction(std::get<0>(ts), Simulator::SimulatorLogger::Cancelled, so);
    }
    delete &cancelledAction;
    pendingActions.erase(cit);
    delete &c;
    logger->logAction(std::get<0>(ts), Simulator::SimulatorLogger::Processed, c);
}

void Simulator::processAction(const Timestep &ts, Action* action){
    switch (action->index())
    {
    case Action::typeIndex<MarketOrder>(): processMarketOrder(ts, static_cast<MarketOrder&>(*action)); break;
    case Action::typeIndex<LimitOrder>(): processLimitOrder(ts, static_cast<LimitOrder&>(*action)); break;
    case Action::typeIndex<StopOrder>(): processStopOrder(ts, static_cast<StopOrder&>(*action)); break;
    case Action::typeIndex<Cancel>(): processCancel(ts, static_cast<Cancel&>(*action)); break;
    default:
        break;
    }
}

std::vector<double> Simulator::run(){
    portfolioValue.reserve(timesteps.size());
    for(const auto& ts : timesteps){
        std::vector<Action*> actions = model
            ->run(portfolio, pendingActions, std::get<1>(ts), std::get<2>(ts));
        for(auto& action : actions)
            processAction(ts, action);
        processPendingActions(ts);
        portfolioValue.push_back(
            portfolio.authMoney + portfolio.pendingMoney +
            ( portfolio.authQuantity + portfolio.pendingQuantity ) * std::get<1>(ts).trades.back().price);
        portfolioValueUpdated();
    }
    return portfolioValue;
}

std::vector<double> Simulator::best(){
    std::vector<double> best{portfolio.authMoney};
    best.reserve(timesteps.size());
    for(auto it = timesteps.begin(); it != timesteps.end()-1; ++it){
        if(std::get<1>(*it).trades.empty()){
            best.push_back(best.back());
            continue;
        }
        double nextPrice = std::get<1>(*(it+1)).trades.back().price;
        double currentPrice = std::get<1>(*it).trades.back().price;
        if(currentPrice <= nextPrice)
            best.push_back((best.back()/currentPrice)*nextPrice);
        else
            best.push_back(best.back());
            
    }
    return best;
}

std::vector<double> Simulator::worst(){
    std::vector<double> worst{portfolio.authMoney};
    worst.reserve(timesteps.size());
    for(auto it = timesteps.begin(); it != timesteps.end()-1; ++it){
        if(std::get<1>(*it).trades.empty()){
            worst.push_back(worst.back());
            continue;
        }
        double nextPrice = std::get<1>(*(it+1)).trades.back().price;
        double currentPrice = std::get<1>(*it).trades.back().price;
        if(currentPrice >= nextPrice)
            worst.push_back((worst.back()/currentPrice)*nextPrice);
        else
            worst.push_back(worst.back());
    }
    return worst;
}

std::vector<double> Simulator::random(){
    std::vector<double> worst{portfolio.authMoney};
    worst.reserve(timesteps.size());
    std::srand(std::time({}));
    for(auto it = timesteps.begin(); it != timesteps.end()-1; ++it){
        if(std::get<1>(*it).trades.empty()){
            worst.push_back(worst.back());
            continue;
        }
        double nextPrice = std::get<1>(*(it+1)).trades.back().price;
        double currentPrice = std::get<1>(*it).trades.back().price;
        if(std::rand()%2 < 1)
            worst.push_back((worst.back()/currentPrice)*nextPrice);
        else
            worst.push_back(worst.back());
    }
    return worst;
}

double Simulator::getPriceMean(){
    double mean = 0;
    for(const auto& t: marketHistory)
        mean += t.price;
    return mean / marketHistory.size();
}

double Simulator::getPriceStdDev(){
    double stdDev = 0;
    double mean = getPriceMean();
    for(const auto& t: marketHistory)
        stdDev += std::pow((mean - t.price), 2);
    return std::sqrt(stdDev / (marketHistory.size()-1));
}

double Simulator::getQuantityMean(){
    double mean = 0;
    for(const auto& t: marketHistory)
        mean += t.quantity;
    return mean / marketHistory.size();
}

double Simulator::getQuantityStdDev(){
    double stdDev = 0;
    double mean = getQuantityMean();
    for(const auto& t: marketHistory)
        stdDev += std::pow((mean - t.quantity), 2);
    return std::sqrt(stdDev / (marketHistory.size()-1));
}