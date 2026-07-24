#include "wiimesh/MeshtasticProtocol.h"
#include "wiimesh/MockTransport.h"

#include <cassert>
#include <iostream>

using namespace wiimesh;

int main() {
    AppState state;
    MockTransport transport;
    Logger *logger = nullptr;
    MeshtasticProtocol protocol(logger);

    std::vector<UsbDeviceInfo> devices;
    transport.pollDevices(devices);
    assert(!devices.empty());
    assert(transport.openFirstSupported(devices));
    assert(protocol.startConfig(transport));

    transport.pushFrame(buildMockTextFromRadio(0x12345678, BroadcastNode, 0, "hello from mesh"));
    protocol.poll(transport, state);

    assert(state.messages.size() == 1);
    assert(state.messages[0].from == 0x12345678);
    assert(!state.messages[0].direct);
    assert(state.messages[0].text == "hello from mesh");
    std::cout << "parsed: " << state.messages[0].senderId << " " << state.messages[0].text << "\n";
    return 0;
}
