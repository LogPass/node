#pragma once

namespace logpass {

class Connection;
class Packet;

struct ConnectionCallbacks {
    const std::function<void(Connection*)> onEnd;
    const std::function<bool(Connection*, MinerId)> onParams;
    const std::function<bool(Connection*)> onReady;
    const std::function<bool(Connection*, std::shared_ptr<Packet>)> onPacket;
};

}
