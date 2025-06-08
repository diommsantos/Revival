#define Simulator() Simulator(); friend int GetBothTimestepsTest(int argc, char* argv[]);

#include "..\Simulator.h"

#undef Simulator

#include <tuple>
#include <algorithm>

int GetBothTimestepsTest(int argc, char* argv[]){
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

    std::vector<Simulator::Timestep> ts = sim.getBothTimesteps();

    std::vector<Simulator::Timestep> expectedts{
        Simulator::Timestep(
            trades[0].timestamp, 
            std::span(sim.marketHistory).subspan(0, 1),
            Simulator::initialOrderBook
        ),
        Simulator::Timestep(
            orderBooks[0].E, 
            std::span(sim.marketHistory).subspan(0, 1),
            orderBooks[0]
        ),
        Simulator::Timestep(
            trades[1].timestamp, 
            std::span(sim.marketHistory).subspan(0, 2),
            orderBooks[0]
        ),
        Simulator::Timestep(
            orderBooks[1].E, 
            std::span(sim.marketHistory).subspan(0, 2),
            orderBooks[1]
        ),
        Simulator::Timestep(
            trades[2].timestamp, 
            std::span(sim.marketHistory).subspan(0, 3),
            orderBooks[1]
        ),
        Simulator::Timestep(
            trades[3].timestamp, 
            std::span(sim.marketHistory).subspan(0, 4),
            orderBooks[1]
        ),
    };

    return ts == expectedts ? 0 : 1;
}