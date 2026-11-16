#include <iostream>
#include <gtkmm.h>
#include "TaskManager.h"

class CdDevice;
class CdrDriver;
class Toc;

class FastTocRead : public Task
{
public:
    FastTocRead(CdDevice* dev = nullptr);
    ~FastTocRead();

    void run();
    void completed();

    sigc::signal<void(Toc*)> signalDone;

private:
    CdDevice* dev_ = nullptr;
    CdrDriver* driver_ = nullptr;
    Toc* toc_ = nullptr;
};
