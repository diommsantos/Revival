#include "Simulator.h"
#include <fstream>
#include <QJsonDocument>
#include <QJsonArray>
#include <algorithm>
#include <cmath>
#include <QThreadPool>

const OrderBook Simulator::initialOrderBook{0, TimePoint::zero(), std::vector<Order>(), std::vector<Order>()};
int Simulator::nextActionId{0};

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
        }
        else{    //TODO: consider the case orderBookTime == marketTime
            i++;
            r.push_back(Timestep(
                orderBookTime, 
                MarketHistory{std::span(marketHistory).subspan(0, j)},
                orderBooks[i-1]));
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
            orderBookTime, 
            MarketHistory{std::span(marketHistory).subspan(0, j)},
            orderBooks[i-1]));
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
            }else if(lo.actionType == SELL && std::get<1>(ts).trades.back().price >= lo.price){
                portfolio.pendingQuantity -= lo.quantity;
                portfolio.authMoney += lo.quantity * lo.price * (1-makerFee);
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
            }else if(so.actionType == SELL && std::get<1>(ts).trades.back().price <= so.price){
                portfolio.pendingQuantity -= so.quantity;
                portfolio.authMoney += so.quantity * std::get<1>(ts).trades.back().price * (1-takerFee);
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
    if(mo.actionType == BUY){
        double total {mo.quantity * std::get<1>(ts).trades.back().price};
        if(!(0 <= mo.quantity && total <= portfolio.authMoney)){
            qDebug() << "The total exceeds the money in the portfolio for the timepoint " 
            << std::get<0>(ts).count() << ". Skipping Market Order Buy action...";
            delete &mo;
            return;
        }
        portfolio.authMoney -= total;
        portfolio.authQuantity += mo.quantity * (1-takerFee);
        emit orderFilled(std::get<0>(ts), std::get<1>(ts).trades.back().price, std::shared_ptr<const MarketOrder>(&mo));
    }else if(mo.actionType == SELL){
        if(!(0 <= mo.quantity && mo.quantity <= portfolio.authQuantity)){
            qDebug() << "The total exceeds the quantity in the portfolio for the timepoint " 
            << std::get<0>(ts).count() << ". Skipping Market Order Sell action...";
            delete &mo;
            return;
        }
        portfolio.authQuantity -= mo.quantity;
        portfolio.authMoney += mo.quantity * std::get<1>(ts).trades.back().price * (1-takerFee);
        emit orderFilled(std::get<0>(ts), std::get<1>(ts).trades.back().price, std::shared_ptr<const MarketOrder>(&mo));
    }    
}

void Simulator::processLimitOrder(const Timestep &ts, LimitOrder& lo){
    if(lo.actionType == BUY){
        double total {lo.quantity * lo.price};
        if(!(0 <= lo.quantity && 0 <= lo.price && total <= portfolio.authMoney)){
            qDebug() << "The total exceeds the money in the portfolio for the timepoint " 
            << std::get<0>(ts).count() << ". Skipping Limit Order Buy action...";
            delete &lo;
            return;
        }
        if(!(std::get<1>(ts).trades.back().price <= lo.price)){
            portfolio.authMoney -= total;
            portfolio.pendingMoney += total;
            lo.actionId = nextActionId; nextActionId++;
            pendingActions.push_back(&lo);
            return;
        }
        portfolio.authMoney -= total;
        portfolio.authQuantity += lo.quantity * (1-makerFee);
        emit orderFilled(std::get<0>(ts), lo.price, std::shared_ptr<const LimitOrder>(&lo));
    }else if(lo.actionType == SELL){
        if(!(0 <= lo.quantity && 0 <= lo.price && lo.quantity <= portfolio.authQuantity)){
            qDebug() << "The total exceeds the quantity in the portfolio for the timepoint " 
            << std::get<0>(ts).count() << ". Skipping Limit Order Sell action...";
            delete &lo;
            return;
        }
        if(!(std::get<1>(ts).trades.back().price >= lo.price)){
            portfolio.authQuantity -= lo.quantity;
            portfolio.pendingQuantity += lo.quantity;
            lo.actionId = nextActionId; nextActionId++;
            pendingActions.push_back(&lo);
            return;
        }
        portfolio.authQuantity -= lo.quantity;
        portfolio.authMoney += lo.quantity * lo.price * (1-makerFee);
        emit orderFilled(std::get<0>(ts), lo.price, std::shared_ptr<const LimitOrder>(&lo));
    }
}

void Simulator::processStopOrder(const Timestep &ts, StopOrder &so){
    if(so.actionType == BUY){
        double total {so.quantity * so.price};
        if(!(0 <= so.quantity && 0 <= so.price && total <= portfolio.authMoney)){
            qDebug() << "The total exceeds the money in the portfolio for the timepoint " 
            << std::get<0>(ts).count() << ". Skipping Limit Order Buy action...";
            delete &so;
            return;
        }
        if(!(std::get<1>(ts).trades.back().price >= so.price)){
            portfolio.authMoney -= total;
            portfolio.pendingMoney += total;
            so.actionId = nextActionId; nextActionId++;
            pendingActions.push_back(&so);
            return;
        }
        portfolio.authMoney -= total;
        portfolio.authQuantity += total / std::get<1>(ts).trades.back().price * (1-takerFee);
        emit orderFilled(std::get<0>(ts), std::get<1>(ts).trades.back().price, std::shared_ptr<StopOrder>(&so));

    }else if(so.actionType == SELL){
        if(!(0 <= so.quantity && 0 <= so.price && so.quantity <= portfolio.authQuantity)){
            qDebug() << "The total exceeds the quantity in the portfolio for the timepoint " 
            << std::get<0>(ts).count() << ". Skipping Limit Order Sell action...";
            delete &so;
            return;
        }
        if(!(std::get<1>(ts).trades.back().price <= so.price)){
            portfolio.authQuantity -= so.quantity;
            portfolio.pendingQuantity += so.quantity;
            so.actionId = nextActionId; nextActionId++;
            pendingActions.push_back(&so);
            return;
        }
        portfolio.authQuantity -= so.quantity;
        portfolio.authMoney += so.quantity * std::get<1>(ts).trades.back().price * (1-takerFee);
        emit orderFilled(std::get<0>(ts), std::get<1>(ts).trades.back().price, std::shared_ptr<const StopOrder>(&so));
    }
}

void Simulator::processCancel(Cancel &c){
    auto cit = std::find_if(pendingActions.begin(), pendingActions.end(), 
        [&c](const Action* a){return c.orderId == a->actionId;});
    const Action &cancelledAction{**cit};
    if(cit == pendingActions.end()){
        qDebug() << "No action with id: " << c.orderId << " exists. Skipping Cancel action...";
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
    }
    delete &cancelledAction;
    pendingActions.erase(cit);
    delete &c;
}

void Simulator::processAction(const Timestep &ts, Action* action){
    switch (action->index())
    {
    case Action::typeIndex<Hold>(): break;
    case Action::typeIndex<MarketOrder>(): processMarketOrder(ts, static_cast<MarketOrder&>(*action)); break;
    case Action::typeIndex<LimitOrder>(): processLimitOrder(ts, static_cast<LimitOrder&>(*action)); break;
    case Action::typeIndex<StopOrder>(): processStopOrder(ts, static_cast<StopOrder&>(*action)); break;
    case Action::typeIndex<Cancel>(): processCancel(static_cast<Cancel&>(*action)); break;
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