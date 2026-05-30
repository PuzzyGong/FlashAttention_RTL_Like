#pragma once

#include "cell.hpp"

class KLoadCell : public Cell {
public:
    explicit KLoadCell(unsigned int delay, uint8_t y, const std::string& tag1, const std::string& tag2)
        : Cell({Option::UseH}, y, delay, tag1, tag2) {}
protected:
    FloatOut phaseH(const PhaseInput& in) override {
        return {
            in.ksram[((in.kaddrReg + in.index) * HEAD_DIMENTION + in.y - 1) % SRAM_SIZE],
            in.index < TILE_SIZE ? "K" : ""
        };
    }
};

class VLoadCell : public Cell {
public:
    explicit VLoadCell(unsigned int delay, uint8_t y, const std::string& tag1, const std::string& tag2)
        : Cell({Option::UseH, Option::FireFromExp}, y, delay, tag1, tag2) {}
protected:
    FloatOut phaseH(const PhaseInput& in) override {
        const int dim = TILE_SIZE - 1 - static_cast<int>(in.y);
        return {
            (!in.vaddrFifo.empty() && in.byteIndexFromExp < TILE_SIZE && dim >= 0)
                ? in.vsram[((in.vaddrFifo.front() + in.byteIndexFromExp) * HEAD_DIMENTION + dim) % SRAM_SIZE]
                : in.floatHFromSelf,
            (!in.vaddrFifo.empty() && in.byteIndexFromExp < TILE_SIZE && dim >= 0) ? "V" : ""
        };
    }
};

class QLoadCell : public Cell {
public:
    explicit QLoadCell(unsigned int delay, uint8_t y, const std::string& tag1, const std::string& tag2)
        : Cell({Option::UseV}, y, delay, tag1, tag2) {}
protected:
    FloatOut phaseV(const PhaseInput& in) override {
        return {
            in.qsram[(in.qaddrReg + in.index) % SRAM_SIZE],
            in.index < TILE_SIZE ? "Q" : ""
        };
    }
};

class CopyCell : public Cell {
public:
    explicit CopyCell(unsigned int delay, uint8_t y, const std::string& tag1, const std::string& tag2)
        : Cell({Option::UseH}, y, delay, tag1, tag2) {}
protected:
    FloatOut phaseH(const PhaseInput& in) override {
        return {
            in.floatHFromLeft,
            in.byteIndexFromLeft < TILE_SIZE ? in.tag1 : ""
        };
    }
};

class MacCell : public Cell {
public:
    explicit MacCell(unsigned int delay, uint8_t y, const std::string& tag1, const std::string& tag2)
        : Cell({Option::UseH, Option::UseV}, y, delay, tag1, tag2) {}
protected:
    FloatOut phaseV(const PhaseInput& in) override {
        return {
            in.floatVFromTop,
            in.byteIndexFromLeft < TILE_SIZE ? in.tag1 : ""
        };
    }

    FloatOut phaseH(const PhaseInput& in) override {
        const uint8_t lane = static_cast<uint8_t>(TILE_SIZE - in.y);
        const uint8_t token = static_cast<uint8_t>(
            in.kaddrReg + lane - ((lane == 0 || in.index == BYTE_INDEX_IDLE) ? 0 : TILE_SIZE)
        );
        float score = 0.0f;
        for (int dim = 0; dim < HEAD_DIMENTION; ++dim) {
            score += in.qsram[(in.qaddrReg + dim) % SRAM_SIZE]
                * in.ksram[(token * HEAD_DIMENTION + dim) % SRAM_SIZE];
        }

        return {
            score,
            in.byteIndexFromLeft < TILE_SIZE ? in.tag1 + "*" + in.tag2 : ""
        };
    }
};

class PvMacCell : public Cell {
public:
    explicit PvMacCell(unsigned int delay, uint8_t y, const std::string& tag1, const std::string& tag2)
        : Cell({Option::UseH, Option::UseV, Option::UseOld}, y, delay, tag1, tag2) {}
protected:
    FloatOut phaseV(const PhaseInput& in) override {
        return {
            in.floatVFromTop,
            in.byteIndexFromLeft < TILE_SIZE ? in.tag1 : ""
        };
    }

    FloatOut phaseH(const PhaseInput& in) override {
        return {
            (in.byteIndexFromLeft == 0)
                ? in.floatHFromLeft * in.floatVFromTop
            : (in.byteIndexFromLeft == TILE_SIZE)
                ? in.floatHFromSelf + in.floatOldFromSelf * in.floatVFromTop
                : in.floatHFromSelf + in.floatHFromLeft * in.floatVFromTop,
            in.byteIndexFromLeft < TILE_SIZE ? in.tag1 + "*" + in.tag2 : ""
        };
    }

    FloatOut phaseOld(const PhaseInput& in) override {
        return {
            (in.byteIndexFromLeft == TILE_SIZE)
                ? in.floatHFromSelf + in.floatOldFromSelf * in.floatVFromTop
                : in.floatOldFromSelf,
            ""
        };
    }
};

class MacDelay1Cell : public Cell {
public:
    explicit MacDelay1Cell(unsigned int delay, uint8_t y, const std::string& tag1, const std::string& tag2)
        : Cell({Option::UseH}, y, delay, tag1, tag2) {}
protected:
    FloatOut phaseH(const PhaseInput& in) override {
        return {
            in.byteIndexFromLeft == TILE_SIZE - 1 ? in.floatHFromLeft : in.floatHFromSelf,
            in.byteIndexFromLeft == TILE_SIZE - 1 ? in.tag1 : ""
        };
    }
};

class MacDelay2Cell : public Cell {
public:
    explicit MacDelay2Cell(unsigned int delay, uint8_t y, const std::string& tag1, const std::string& tag2)
        : Cell({Option::UseH}, y, delay, tag1, tag2) {}
protected:
    FloatOut phaseH(const PhaseInput& in) override {
        return {
            in.floatHFromLeft,
            in.byteIndexFromLeft == TILE_SIZE ? in.tag1 : ""
        };
    }
};

class Mux1Cell : public Cell {
public:
    explicit Mux1Cell(unsigned int delay, uint8_t y, const std::string& tag1, const std::string& tag2)
        : Cell({Option::UseH}, y, delay, tag1, tag2) {}
protected:
    FloatOut phaseH(const PhaseInput& in) override {
        return {
            in.floatHFromLeftColumn[in.byteIndexFromLeft % TILE_SIZE],
            in.byteIndexFromLeft < TILE_SIZE ? in.tag1 : ""
        };
    }
};

class MaxOldMCell : public Cell {
public:
    explicit MaxOldMCell(unsigned int delay, uint8_t y, const std::string& tag1, const std::string& tag2)
        : Cell({Option::UseCalcu0, Option::UseH, Option::UseOld}, y, delay, tag1, tag2) {}
protected:
    FloatOut phaseH(const PhaseInput& in) override {
        return {
            in.byteIndexFromLeft < TILE_SIZE ? in.floatHFromLeft : in.floatOldFromSelf,
            in.byteIndexFromLeft < TILE_SIZE ? "S" : in.byteIndexFromLeft == TILE_SIZE ? "old_m" : ""
        };
    }

    FloatOut phaseCalcu0(const PhaseInput& in) override {
        return {
            (in.byteIndexFromLeft == 0)
                ? in.floatHFromLeft
            : (in.byteIndexFromLeft < TILE_SIZE)
                ? ((in.floatCalcu0FromSelf > in.floatHFromLeft) ? in.floatCalcu0FromSelf : in.floatHFromLeft)
            : (in.byteIndexFromLeft == TILE_SIZE)
                ? ((in.floatCalcu0FromSelf > in.floatOldFromSelf) ? in.floatCalcu0FromSelf : in.floatOldFromSelf)
                : 0.0f,
            ""
        };
    }

    FloatOut phaseOld(const PhaseInput& in) override {
        return {
            (in.byteIndexFromLeft == TILE_SIZE && in.lastFromLeft == 1)
                ? 0.0f
            : (in.byteIndexFromLeft == TILE_SIZE)
                ? ((in.floatCalcu0FromSelf > in.floatOldFromSelf) ? in.floatCalcu0FromSelf : in.floatOldFromSelf)
                : in.floatOldFromSelf,
            ""
        };
    }
};

class MaxCopyCell : public Cell {
public:
    explicit MaxCopyCell(unsigned int delay, uint8_t y, const std::string& tag1, const std::string& tag2)
        : Cell({Option::UseH}, y, delay, tag1, tag2) {}
protected:
    FloatOut phaseH(const PhaseInput& in) override {
        return {
            in.floatHFromLeft,
            in.byteIndexFromLeft < TILE_SIZE ? "S" : in.byteIndexFromLeft == TILE_SIZE ? "old_m" : ""
        };
    }
};

class SubCell : public Cell {
public:
    explicit SubCell(unsigned int delay, uint8_t y, const std::string& tag1, const std::string& tag2)
        : Cell({Option::UseH}, y, delay, tag1, tag2) {}
protected:
    FloatOut phaseH(const PhaseInput& in) override {
        return {
            in.floatHFromLeft - in.floatOldFromMaxOld,
            in.byteIndexFromLeft < TILE_SIZE ? "N" : in.byteIndexFromLeft == TILE_SIZE ? "a" : ""
        };
    }
};

class SDelayCell : public Cell {
public:
    explicit SDelayCell(unsigned int delay, uint8_t y, const std::string& tag1, const std::string& tag2)
        : Cell({Option::UseH}, y, delay, tag1, tag2) {}
protected:
    FloatOut phaseH(const PhaseInput& in) override {
        return {
            in.floatHFromLeft,
            in.byteIndexFromLeft < TILE_SIZE ? "S" : in.byteIndexFromLeft == TILE_SIZE ? "old_m" : ""
        };
    }
};

class ExpCell : public Cell {
public:
    explicit ExpCell(unsigned int delay, uint8_t y, const std::string& tag1, const std::string& tag2)
        : Cell({Option::UseH}, y, delay, tag1, tag2) {}
protected:
    FloatOut phaseH(const PhaseInput& in) override {
        return {
            std::exp(in.floatHFromLeft / std::sqrt(static_cast<float>(HEAD_DIMENTION))),
            in.byteIndexFromLeft < TILE_SIZE ? "P" : in.byteIndexFromLeft == TILE_SIZE ? "b" : ""
        };
    }
};

class ExpDelayCell : public Cell {
public:
    explicit ExpDelayCell(unsigned int delay, uint8_t y, const std::string& tag1, const std::string& tag2)
        : Cell({Option::UseH, Option::UseV}, y, delay, tag1, tag2) {}
protected:
    FloatOut phaseV(const PhaseInput& in) override {
        return {
            in.floatHFromLeft,
            in.byteIndexFromLeft < TILE_SIZE ? "P" : in.byteIndexFromLeft == TILE_SIZE ? "b" : ""
        };
    }

    FloatOut phaseH(const PhaseInput& in) override {
        return {
            in.floatHFromLeft,
            in.byteIndexFromLeft < TILE_SIZE ? "P" : in.byteIndexFromLeft == TILE_SIZE ? "b" : ""
        };
    }
};

class LocalLCell : public Cell {
public:
    explicit LocalLCell(unsigned int delay, uint8_t y, const std::string& tag1, const std::string& tag2)
        : Cell({Option::UseCalcu0, Option::UseH, Option::UseOld}, y, delay, tag1, tag2) {}
protected:
    FloatOut phaseH(const PhaseInput& in) override {
        return {
            in.byteIndexFromLeft == TILE_SIZE
                ? in.floatOldFromSelf * in.floatHFromLeft + in.floatCalcu0FromSelf
                : 0.0f,
            in.byteIndexFromLeft == TILE_SIZE - 1 ? "local_l" : in.byteIndexFromLeft == TILE_SIZE ? "new_l" : ""
        };
    }

    FloatOut phaseCalcu0(const PhaseInput& in) override {
        return {
            (in.byteIndexFromLeft == 0)
                ? in.floatHFromLeft
            : (in.byteIndexFromLeft < TILE_SIZE)
                ? in.floatCalcu0FromSelf + in.floatHFromLeft
                : 0.0f,
            ""
        };
    }

    FloatOut phaseOld(const PhaseInput& in) override {
        return {
            (in.byteIndexFromLeft == TILE_SIZE && in.lastFromLeft == 1)
                ? 0.0f
            : (in.byteIndexFromLeft == TILE_SIZE)
                ? in.floatOldFromSelf * in.floatHFromLeft + in.floatCalcu0FromSelf
                : in.floatOldFromSelf,
            ""
        };
    }

    void phaseSideEffect(const PhaseInput& in, SideEffectInput& sideEffect) override {
        if (in.byteIndexFromLeft == TILE_SIZE && !sideEffect.vaddrFifo.empty()) {
            sideEffect.vaddrFifo.pop();
        }
    }
};

class RecipCell : public Cell {
public:
    explicit RecipCell(unsigned int delay, uint8_t y, const std::string& tag1, const std::string& tag2)
        : Cell({Option::UseV}, y, delay, tag1, tag2) {}
protected:
    FloatOut phaseV(const PhaseInput& in) override {
        return {
            in.byteIndexFromLeft == 0
                ? (in.floatHFromLeft != 0.0f ? 1.0f / in.floatHFromLeft : 0.0f)
                : in.floatVFromSelf,
            in.byteIndexFromLeft == 0 ? "recip" : ""
        };
    }
};

class FinalOCell : public Cell {
public:
    explicit FinalOCell(unsigned int delay, uint8_t y, const std::string& tag1, const std::string& tag2)
        : Cell({Option::UseH}, y, delay, tag1, tag2) {}
protected:
    FloatOut phaseH(const PhaseInput& in) override {
        return {
            in.floatHFromLeft * in.floatVFromTop,
            in.byteIndexFromLeft < TILE_SIZE ? "O" : ""
        };
    }
};

class FinalSramCell : public Cell {
public:
    explicit FinalSramCell(unsigned int delay, uint8_t y, const std::string& tag1, const std::string& tag2)
        : Cell({}, y, delay, tag1, tag2) {}
protected:
    FloatOut phaseH(const PhaseInput& in) override {
        return {
            in.floatHFromSelf,
            in.byteIndexFromLeft < TILE_SIZE ? "OSRAM" : ""
        };
    }

    void phaseSideEffect(const PhaseInput& in, SideEffectInput& sideEffect) override {
        if (in.byteIndexFromLeft < TILE_SIZE && !sideEffect.oaddrFifo.empty()) {
            sideEffect.osram[static_cast<std::size_t>((sideEffect.oaddrFifo.front() + in.byteIndexFromLeft) % sideEffect.osram.size())] = in.floatHFromLeft;
        }

        if (in.byteIndexFromLeft == TILE_SIZE - 1 && !sideEffect.oaddrFifo.empty()) {
            sideEffect.oaddrFifo.pop();
        }
    }
};
