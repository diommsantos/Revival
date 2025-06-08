#define Simulator() Simulator(); friend int GetOrderBookTimestepsTest(int argc, char* argv[]);

#include "..\Simulator.h"

#undef Simulator

int GetOrderBookTimestepsTest(int argc, char* argv[]){
    QApplication a(argc, argv);
    Simulator sim;

    Trade trades[]{
        Trade{0, 1, 1, TimePoint(4)},
        Trade{0, 2, 2, TimePoint(8)},
        Trade{0, 3, 3, TimePoint(11)},
        Trade{0, 4, 4, TimePoint(12)}
    };

    sim.marketHistory.insert(
        sim.marketHistory.end(), 
        trades, 
        trades+sizeof(trades)/sizeof(Trade)
    );

    OrderBook orderBooks[]{
        OrderBook{0, TimePoint(6)},
        OrderBook{0, TimePoint(9)}
    };

    sim.orderBooks.insert(sim.orderBooks.end(), 
        orderBooks,
        orderBooks+sizeof(orderBooks)/sizeof(OrderBook)
    );

    std::vector<Simulator::Timestep> ts = sim.getOrderBookTimesteps();

    std::vector<Simulator::Timestep> expectedts{
        Simulator::Timestep(
            orderBooks[0].E, 
            std::span(sim.marketHistory).subspan(0, 1),
            orderBooks[0]
        ),
        Simulator::Timestep(
            orderBooks[1].E, 
            std::span(sim.marketHistory).subspan(0, 2),
            orderBooks[1]
        ),
    };

    return ts == expectedts ? 0 : 1;
}