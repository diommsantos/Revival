#include "SimulatorUI.h"
#include <QApplication>
#include <iostream>

typedef Model * (*GetModelType)(Simulator *);

Model *LoadModel(const char * libName, Simulator *sim){
    QLibrary lib(libName);
    if (lib.load()) {
        GetModelType getModel = (GetModelType) lib.resolve("getModel");
        if (getModel) {
            return getModel(sim);
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
    QApplication a(argc, argv);
    SimulatorUI simUI;
    std::cout << "Hello, Worlda!" << std::endl;
    simUI.sim->loadHistoricalData("C:/Users/Diogo/Desktop/Projetos/RevivalProject/Revival/BTCUSDT-aggTrades-2025-07-02.csv", 
                           "C:/Users/Diogo/Desktop/Projetos/RevivalProject/Revival/BTCUSDT-orderBook-02-07-2025-14h14m.csv");
    Model *test = LoadModel("Model", simUI.sim);
    simUI.sim->init(test, Portfolio{1000, 0}, Simulator::ORDER_BOOK, 0, 0);

    simUI.show();
    return a.exec();
    
}