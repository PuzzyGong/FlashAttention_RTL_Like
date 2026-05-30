#pragma once

#include "define.hpp" //去这里面取

class CellArray;

class Cell {
private:
    
    CellArray* array_ = nullptr;

    struct CellState {
        bool boolLast = 0; //只有bool 0 或者 1
        uint8_t byteIndex = BYTE_INDEX_IDLE;

        float floatCalcu0 = 0.0f; // compute slot 0
        float floatCalcu1 = 0.0f; // compute slot 1
        float floatH      = 0.0f; // horizontal transfer
        float floatV      = 0.0f; // vertical transfer
        float floatOld    = 0.0f; // old value storage
    } cur_, next_;
    
    bool useFloatCalcu0_ = false;
    bool useFloatCalcu1_ = false;
    bool useFloatH_ = false;
    bool useFloatV_ = false;
    bool useFloatOld_ = false;
    bool fireFromExp_ = false;
    uint8_t y_;
    uint8_t delay_;
    std::string tag1_;
    std::string tag2_;

    std::string floatCalcu0String_;
    std::string floatCalcu1String_;
    std::string floatHString_;
    std::string floatVString_;
    std::string floatOldString_;
    int color_ = 0;

public:


    struct PhaseInput {
        uint8_t y;
        const std::string& tag1;
        const std::string& tag2;

        float floatCalcu0FromSelf;
        float floatCalcu1FromSelf;
        float floatHFromSelf;
        float floatVFromSelf;
        float floatOldFromSelf;

        bool hasLeft;
        //bool fireFromLeft; 不要这个，这个的计算放到 prepareNext 里, 而不是cell_array 里
        int lastFromLeft;
        uint8_t byteIndexFromLeft;
        uint8_t delayFromLeft;
        float floatCalcu0FromLeft;
        float floatCalcu1FromLeft;
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
        InfInitCalcu0,
        UseCalcu0,
        UseCalcu1,
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
        bool infInitFloatCalcu0 = false;
        for (Option option : options) {
            switch (option) {
            case Option::InfInitCalcu0:
                infInitFloatCalcu0 = true;
                break;
            case Option::UseCalcu0:
                useFloatCalcu0_ = true;
                break;
            case Option::UseCalcu1:
                useFloatCalcu1_ = true;
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

        if (infInitFloatCalcu0) {
            cur_.floatCalcu0 = -std::numeric_limits<float>::infinity();
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
    virtual FloatOut phaseCalcu0(const PhaseInput& in) {
        return {in.floatCalcu0FromSelf, ""};
    }

    virtual FloatOut phaseCalcu1(const PhaseInput& in) {
        return {in.floatCalcu1FromSelf, ""};
    }

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
        const FloatOut calcu0 = phaseCalcu0(in);
        const FloatOut calcu1 = phaseCalcu1(in);
        const FloatOut h = phaseH(in);
        const FloatOut v = phaseV(in);
        const FloatOut old = phaseOld(in);

        next_.floatCalcu0 = calcu0.value;
        next_.floatCalcu1 = calcu1.value;
        next_.floatH = h.value;
        next_.floatV = v.value;
        next_.floatOld = old.value;

        floatCalcu0String_ = calcu0.label;
        floatCalcu1String_ = calcu1.label;
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

    float floatCalcu0() const {
        return cur_.floatCalcu0;
    }

    float floatCalcu1() const {
        return cur_.floatCalcu1;
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

    const std::string& floatCalcu0String() const {
        return floatCalcu0String_;
    }

    const std::string& floatCalcu1String() const {
        return floatCalcu1String_;
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

    bool useFloatCalcu0() const {
        return useFloatCalcu0_;
    }

    bool useFloatCalcu1() const {
        return useFloatCalcu1_;
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
        floatCalcu0String_ = "";
        floatCalcu1String_ = "";
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

