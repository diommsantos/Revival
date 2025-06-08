#pragma once

#include <chrono>
#include <vector>
#include <list>
#include <span>
#include <variant>
#include <QtCore/qglobal.h>

#if defined(MODEL_LIBRARY)
# define MODEL_API Q_DECL_EXPORT
#else
# define MODEL_API Q_DECL_IMPORT
#endif

using TimePoint = std::chrono::milliseconds;

typedef struct Portfolio
{
    double authMoney;
    double pendingMoney;
    double authQuantity;
    double pendingQuantity;
    bool operator==(const Portfolio&) const = default;
    bool operator!=(const Portfolio&) const = default;
} Portfolio;

struct Order
{
    double price;
    double quantity;
    bool operator==(const Order&) const = default;
    bool operator!=(const Order&) const = default;
};

typedef struct OrderBook
{
    long long lastUpdateId;
    TimePoint E; // Event time
    std::vector<Order> bids;
    std::vector<Order> asks;
    bool operator==(const OrderBook&) const = default;
    bool operator!=(const OrderBook&) const = default;
} OrderBook;

typedef struct Trade{
    long long tradeId;
    double price;
    double quantity;
    TimePoint timestamp;
    bool operator==(const Trade&) const = default;
    bool operator!=(const Trade&) const = default;
} Trade;

typedef struct MarketHistory
{
    std::span<const Trade> trades;
    bool operator==(const MarketHistory& m) const{
        return this->trades.data() == m.trades.data() && this->trades.size() == m.trades.size();
    }
    bool operator!=(const MarketHistory& m) const{
        return !operator==(m);
    }
} MarketHistory;

struct Hold;
struct MarketOrder;
struct LimitOrder;
struct StopOrder;
struct Cancel;

class Action
{
    friend class Simulator;

protected:
    int actionId;
    std::size_t actionIndex;

public:

    Action(int actionIndex) :
    actionIndex(actionIndex)
    {
    }

    int getId() const {return actionId;}

    template<typename T>
    inline constexpr static int typeIndex(){
        if(std::is_same_v<T, Hold>)
            return 0;
        else if(std::is_same_v<T, Hold>)
            return 1;
        else if(std::is_same_v<T, MarketOrder>)
            return 2;
        else if(std::is_same_v<T, LimitOrder>)
            return 3;
        else if(std::is_same_v<T, StopOrder>)
            return 4;
        else if(std::is_same_v<T, Cancel>)
            return 5;         
    }

    std::size_t index() const { return actionIndex;}

};

struct Hold : public Action{
    friend class Simulator;
    Hold() : 
    Action(Action::typeIndex<Hold>())
    {}
};

enum ActionType{
    BUY,
    SELL
};

struct MarketOrder : public Action{
    friend class Simulator;
    ActionType actionType;
    double quantity;
    MarketOrder(ActionType actionType, double quantity) : 
    Action(Action::typeIndex<MarketOrder>()),
    actionType{actionType},
    quantity{quantity}
    {}
};

struct LimitOrder : public Action{
    friend class Simulator;
    ActionType actionType;  
    double quantity;
    double price;
    LimitOrder(ActionType actionType, double quantity, double price) : 
    Action(Action::typeIndex<LimitOrder>()),
    actionType{actionType},
    quantity{quantity},
    price{price}
    {}
};

struct StopOrder : public Action{
    friend class Simulator;
    ActionType actionType;  
    double quantity;
    double price;
    StopOrder(ActionType actionType, double quantity, double price) : 
    Action(Action::typeIndex<StopOrder>()),
    actionType{actionType},
    quantity{quantity},
    price{price}
    {}
};

struct Cancel : public Action{
    friend class Simulator;
    int orderId;
    Cancel(int orderId) : 
    Action(Action::typeIndex<Cancel>()),
    orderId{orderId}
    {}
};

class MODEL_API Model{

public:
    
    Model() = default;
    virtual ~Model() = default;

    virtual std::vector<Action *> run(const Portfolio &portfolio, const std::vector<const Action*> &pendingActions, 
               const MarketHistory &market, const OrderBook &orderBook) = 0;
};