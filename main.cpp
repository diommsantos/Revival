#include "Simulator.h"
#include <QApplication>
#include <iostream>

typedef Model * (*GetModelType)();

Model *LoadModel(const char * libName){
    QLibrary lib(libName);
    if (lib.load()) {
        GetModelType getModel = (GetModelType) lib.resolve("getModel");
        if (getModel) {
            return getModel();
        } else {
            qDebug() << "Could not resolve function!";
            exit(1);
            return nullptr;
        }
    } else {
        qDebug() << "Could not load " << libName << " library!";
        exit(1);
        return nullptr;
    }
}

int main(int argc, char *argv[])
{   
    std::cout << "Hello, World!" << std::endl;
    QApplication a(argc, argv);
    Simulator sim;
    sim.loadHistoricalData("BTCUSDT-aggTrades-2025-05-29.csv", "orderbook.csv");
    Model *test = LoadModel("Model");
    sim.init(test, Portfolio{1000, 0}, Simulator::ORDER_BOOK);

    sim.show();
    return a.exec();
    
}