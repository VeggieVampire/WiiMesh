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
    const auto directFrame = buildTextToRadioFrame(0x12345678, 0, "direct test", true);
    assert(directFrame.size() > 16);
    assert(directFrame[0] == 0x94);
    assert(directFrame[1] == 0xc3);
    assert(protocol.sendText(transport, 0x12345678, 0, "direct test", true));
    std::cout << "parsed: " << state.messages[0].senderId << " " << state.messages[0].text << "\n";
    return 0;
}
