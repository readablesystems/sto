#pragma once

namespace storia {
// Update type
enum StateUpdateType {
    BlindWrite = 0,
    ReadWrite,
    PartialBlindWrite,
    PartialReadWrite,
    Delete,
};

template <typename T>
class StateUpdate {
public:
    typedef StateUpdateType Type;

private:
    StateUpdate(const Type type, const T& value)
        : type(type), value(std::move(value)) {}

public:
    static StateUpdate NewBlindWrite(const T& value) {
        return StateUpdate(Type::BlindWrite, std::move(value));
    }

    const Type type;
    const T value;
};

};  // namespace storia
