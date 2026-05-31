#pragma once

#include "define.hpp" //去这里面取

class CellArray;

class Cell {
private:
    
    CellArray* array_ = nullptr;

    struct CellState {
        bool boolLast = 0;
        uint8_t byteIndex = BYTE_INDEX_IDLE;

        float floatH      = 0.0f; // horizontal transfer
        float floatV      = 0.0f; // vertical transfer
        float floatOld    = 0.0f; // old value storage
    } cur_, next_;
    
    bool useFloatH_ = false;
    bool useFloatV_ = false;
    bool useFloatOld_ = false;
    bool fireFromExp_ = false;
    uint8_t y_;
    uint8_t delay_;
    std::string tag1_;
    std::string tag2_;

    std::string floatHString_;
    std::string floatVString_;
    std::string floatOldString_;
    int color_ = 0;

public:


    struct PhaseInput {
        uint8_t y;
        const std::string& tag1;
        const std::string& tag2;

        float floatHFromSelf;
        float floatVFromSelf;
        float floatOldFromSelf;

        bool hasLeft;
        int lastFromLeft;
        uint8_t byteIndexFromLeft;
        uint8_t delayFromLeft;
        float floatHFromLeft;
        float floatVFromLeft;
        float floatOldFromLeft;
        std::array<float, HEAD_DIMENTION> floatHFromLeftColumn;
        float floatVFromTop;
        uint8_t byteIndexFromExp;
        float floatOldFromMaxOld;

        uint8_t index;
        uint8_t qaddrReg;
        uint8_t kaddrReg;
        uint8_t lastReg;
        const std::queue<uint8_t>& vaddrFifo;
        const std::queue<uint8_t>& oaddrFifo;

        const std::array<float, SRAM_SIZE>& qsram;
        const std::array<float, SRAM_SIZE>& ksram;
        const std::array<float, SRAM_SIZE>& vsram;
    };

    struct SideEffectInput {
        std::queue<uint8_t>& vaddrFifo;
        std::queue<uint8_t>& oaddrFifo;
        std::array<float, SRAM_SIZE>& osram;
    };



protected:
    enum class Option {
        InfInitV,
        UseH,
        UseV,
        UseOld,
        FireFromExp
    };

public:
    explicit Cell(
        std::initializer_list<Option> options,
        uint8_t y,
        uint8_t delay,
        const std::string& tag1,
        const std::string& tag2
    )
        : 
          y_(y),
          delay_(delay),
          tag1_(tag1),
          tag2_(tag2) {
        bool infInitFloatV = false;
        for (Option option : options) {
            switch (option) {
            case Option::InfInitV:
                infInitFloatV = true;
                break;
            case Option::UseH:
                useFloatH_ = true;
                break;
            case Option::UseV:
                useFloatV_ = true;
                break;
            case Option::UseOld:
                useFloatOld_ = true;
                break;
            case Option::FireFromExp:
                fireFromExp_ = true;
                break;
            }
        }

        if (infInitFloatV) {
            cur_.floatV = -std::numeric_limits<float>::infinity();
            next_ = cur_;
        }
    }

    virtual ~Cell() = default;

    Cell(const Cell&) = delete;
    Cell& operator=(const Cell&) = delete;

protected:
    struct FloatOut {
        float value;
        std::string label;
    };
    virtual FloatOut phaseH(const PhaseInput& in) {
        return {in.floatHFromSelf, ""};
    }

    virtual FloatOut phaseV(const PhaseInput& in) {
        return {in.floatVFromSelf, ""};
    }

    virtual FloatOut phaseOld(const PhaseInput& in) {
        return {in.floatOldFromSelf, ""};
    }

    virtual void phaseSideEffect(const PhaseInput& in, SideEffectInput& sideEffect) {
        (void)in;
        (void)sideEffect;
    }

public:
    void attach(CellArray* array) {
        array_ = array;
    }

    void run(const PhaseInput& in, SideEffectInput sideEffect) {
        prepareNext(in);
        const FloatOut h = phaseH(in);
        const FloatOut v = phaseV(in);
        const FloatOut old = phaseOld(in);

        next_.floatH = h.value;
        next_.floatV = v.value;
        next_.floatOld = old.value;

        floatHString_ = h.label;
        floatVString_ = v.label;
        floatOldString_ = old.label;

        phaseSideEffect(in, sideEffect);
    }

    void commit() {
        cur_ = next_;
    }

    bool boolLast() const {
        return cur_.boolLast;
    }

    uint8_t byteIndex() const {
        return cur_.byteIndex;
    }

    float floatH() const {
        return cur_.floatH;
    }

    float floatV() const {
        return cur_.floatV;
    }

    float floatOld() const {
        return cur_.floatOld;
    }

    uint8_t delay() const {
        return delay_;
    }

    const std::string& label() const {
        return floatHString_;
    }

    const std::string& floatHString() const {
        return floatHString_;
    }

    const std::string& floatVString() const {
        return floatVString_;
    }

    const std::string& floatOldString() const {
        return floatOldString_;
    }

    bool useFloatH() const {
        return useFloatH_;
    }

    bool useFloatV() const {
        return useFloatV_;
    }

    bool useFloatOld() const {
        return useFloatOld_;
    }

    int color() const {
        return color_;
    }



private:
    void prepareNext(const PhaseInput& in) {
        next_ = cur_;
        floatHString_ = "";
        floatVString_ = "";
        floatOldString_ = "";

        const bool fireFromLeft =
            (in.hasLeft && in.byteIndexFromLeft == in.delayFromLeft - 1)
            || (fireFromExp_ && in.byteIndexFromExp == 0);
        if (fireFromLeft) {
            next_.boolLast = in.lastFromLeft != 0;
            next_.byteIndex = 0;
            color_ ++;
            return;
        }

        if (!in.hasLeft && !fireFromExp_ && in.index == 0) {
            next_.boolLast = in.lastReg != 0;
            next_.byteIndex = 0;
            color_ ++;
            return;
        }

        if (cur_.byteIndex == delay_ - 1) {
            next_.boolLast = false;
        }

        next_.byteIndex = cur_.byteIndex < BYTE_INDEX_IDLE ? cur_.byteIndex + 1 : BYTE_INDEX_IDLE;
    }
};

