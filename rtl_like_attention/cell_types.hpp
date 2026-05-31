#pragma once

#include "cell.hpp"

namespace label {
inline int u(uint8_t value) {
    return static_cast<int>(value);
}

inline int j(const Cell::PhaseInput& in) {
    return u(in.byteIndexFromLeft);
}

inline int jFromExp(const Cell::PhaseInput& in) {
    return u(in.byteIndexFromExp);
}

inline int dimFromY(const Cell::PhaseInput& in) {
    return HEAD_DIMENTION - 1 - u(in.y);
}

inline int kDim(const Cell::PhaseInput& in) {
    return u(in.y) - 1;
}

inline std::string one(const char* name, int i) {
    return std::string(name) + "[" + std::to_string(i) + "]";
}

inline std::string two(const char* name, int i, int k) {
    return std::string(name) + "[" + std::to_string(i) + "][" + std::to_string(k) + "]";
}

inline std::string upto(const char* name, int i, int last) {
    return std::string(name) + "[" + std::to_string(i) + "][:" + std::to_string(last) + "]";
}

inline std::string prefix(const char* name, int last) {
    return std::string(name) + "[:" + std::to_string(last) + "]";
}
}

class KLoadCell : public Cell {
public:
    explicit KLoadCell(unsigned int delay, uint8_t y, const std::string& tag1, const std::string& tag2)
        : Cell({Option::UseH}, y, delay, tag1, tag2) {}
protected:
    FloatOut phaseH(const PhaseInput& in) override {
        return {
            in.ksram[((in.kaddrReg + in.index) * HEAD_DIMENTION + in.y - 1) % SRAM_SIZE],
            in.index < TILE_SIZE ? label::two("K", label::u(in.index), label::kDim(in)) : ""
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
            (!in.vaddrFifo.empty() && in.byteIndexFromExp < TILE_SIZE && dim >= 0)
                ? label::two("V", label::jFromExp(in), dim)
                : ""
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
            in.index < TILE_SIZE ? label::one("Q", label::u(in.index)) : ""
        };
    }
};

class CopyCell : public Cell {
public:
    explicit CopyCell(unsigned int delay, uint8_t y, const std::string& tag1, const std::string& tag2)
        : Cell({Option::UseH}, y, delay, tag1, tag2) {}
protected:
    FloatOut phaseH(const PhaseInput& in) override {
        std::string out;
        if (in.byteIndexFromLeft < TILE_SIZE) {
            if (in.tag1 == "K") {
                out = label::two("K", label::j(in), label::kDim(in));
            } else if (in.tag1 == "V") {
                out = label::two("V", label::j(in), label::dimFromY(in));
            } else if (in.tag1 == "S") {
                out = label::one("S", label::j(in));
            } else {
                out = in.tag1;
            }
        } else if (in.byteIndexFromLeft == TILE_SIZE) {
            out = (in.tag1 == "S") ? "old_m" : in.tag1;
        }
        return {
            in.floatHFromLeft,
            out
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
            in.byteIndexFromLeft < TILE_SIZE ? label::one("Q", label::j(in)) : ""
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
            in.byteIndexFromLeft < TILE_SIZE
                ? label::one("S", label::u(lane)) + "=Q[:]*" + label::one("K", label::u(lane)) + "[:]"
                : ""
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
            in.byteIndexFromLeft < TILE_SIZE ? label::one("P", label::j(in)) :
            in.byteIndexFromLeft == TILE_SIZE ? "b" : ""
        };
    }

    FloatOut phaseH(const PhaseInput& in) override {
        const int j = label::j(in);
        const int dim = label::dimFromY(in);
        return {
            (in.byteIndexFromLeft == 0)
                ? in.floatHFromLeft * in.floatVFromTop
            : (in.byteIndexFromLeft == TILE_SIZE)
                ? in.floatHFromSelf + in.floatOldFromSelf * in.floatVFromTop
                : in.floatHFromSelf + in.floatHFromLeft * in.floatVFromTop,
            in.byteIndexFromLeft < TILE_SIZE
                ? label::one("P", j) + "*" + label::two("V", j, dim) + "->" + label::upto("local_O", dim, j)
            : in.byteIndexFromLeft == TILE_SIZE
                ? label::one("new_O", dim) + "=b*old_O+" + label::one("local_O", dim)
                : ""
        };
    }

    FloatOut phaseOld(const PhaseInput& in) override {
        return {
            (in.byteIndexFromLeft == TILE_SIZE)
                ? in.floatHFromSelf + in.floatOldFromSelf * in.floatVFromTop
                : in.floatOldFromSelf,
            in.byteIndexFromLeft == TILE_SIZE ? label::one("old_O", label::dimFromY(in)) : ""
        };
    }
};

class MacDelay1Cell : public Cell {
public:
    explicit MacDelay1Cell(unsigned int delay, uint8_t y, const std::string& tag1, const std::string& tag2)
        : Cell({Option::UseH}, y, delay, tag1, tag2) {}
protected:
    FloatOut phaseH(const PhaseInput& in) override {
        const int lane = TILE_SIZE - label::u(in.y);
        return {
            in.byteIndexFromLeft == TILE_SIZE - 1 ? in.floatHFromLeft : in.floatHFromSelf,
            in.byteIndexFromLeft == TILE_SIZE - 1 ? label::one("S", lane) : ""
        };
    }
};

class MacDelay2Cell : public Cell {
public:
    explicit MacDelay2Cell(unsigned int delay, uint8_t y, const std::string& tag1, const std::string& tag2)
        : Cell({Option::UseH}, y, delay, tag1, tag2) {}
protected:
    FloatOut phaseH(const PhaseInput& in) override {
        std::string out;
        if (in.tag1 == "LocalL" && in.byteIndexFromLeft == TILE_SIZE) {
            out = "new_l";
        } else if (in.tag1 == "O") {
            const int dim = label::dimFromY(in);
            if (in.byteIndexFromLeft == TILE_SIZE) {
                out = label::one("new_O", dim);
            }
        } else if (in.byteIndexFromLeft == TILE_SIZE) {
            out = in.tag1;
        }
        return {
            in.floatHFromLeft,
            out
        };
    }
};

class Mux1Cell : public Cell {
public:
    explicit Mux1Cell(unsigned int delay, uint8_t y, const std::string& tag1, const std::string& tag2)
        : Cell({Option::UseH}, y, delay, tag1, tag2) {}
protected:
    FloatOut phaseH(const PhaseInput& in) override {
        const bool finalMux = in.tag1 == "V";
        const int idx = label::j(in);
        return {
            in.floatHFromLeftColumn[in.byteIndexFromLeft % TILE_SIZE],
            in.byteIndexFromLeft < TILE_SIZE
                ? (finalMux ? label::one("new_O", idx) : label::one("S", idx))
                : ""
        };
    }
};

class MaxOldMCell : public Cell {
public:
    explicit MaxOldMCell(unsigned int delay, uint8_t y, const std::string& tag1, const std::string& tag2)
        : Cell({Option::InfInitV, Option::UseH, Option::UseV, Option::UseOld}, y, delay, tag1, tag2) {}
protected:
    FloatOut phaseH(const PhaseInput& in) override {
        return {
            in.byteIndexFromLeft < TILE_SIZE ? in.floatHFromLeft : in.floatOldFromSelf,
            in.byteIndexFromLeft < TILE_SIZE ? label::one("S", label::j(in)) :
            in.byteIndexFromLeft == TILE_SIZE ? "old_m" : ""
        };
    }

    FloatOut phaseV(const PhaseInput& in) override {
        return {
            (in.byteIndexFromLeft == 0)
                ? in.floatHFromLeft
            : (in.byteIndexFromLeft < TILE_SIZE)
                ? ((in.floatVFromSelf > in.floatHFromLeft) ? in.floatVFromSelf : in.floatHFromLeft)
            : (in.byteIndexFromLeft == TILE_SIZE)
                ? ((in.floatVFromSelf > in.floatOldFromSelf) ? in.floatVFromSelf : in.floatOldFromSelf)
                : 0.0f,
            in.byteIndexFromLeft < TILE_SIZE ? label::prefix("local_m", label::j(in)) :
            in.byteIndexFromLeft == TILE_SIZE ? "new_m" : ""
        };
    }

    FloatOut phaseOld(const PhaseInput& in) override {
        return {
            (in.byteIndexFromLeft == TILE_SIZE && in.lastFromLeft == 1)
                ? 0.0f
            : (in.byteIndexFromLeft == TILE_SIZE)
                ? ((in.floatVFromSelf > in.floatOldFromSelf) ? in.floatVFromSelf : in.floatOldFromSelf)
                : in.floatOldFromSelf,
            in.byteIndexFromLeft == TILE_SIZE ? "old_m=new_m" : ""
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
            in.byteIndexFromLeft < TILE_SIZE ? label::one("S", label::j(in)) :
            in.byteIndexFromLeft == TILE_SIZE ? "old_m" : ""
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
            in.byteIndexFromLeft < TILE_SIZE ? label::one("N", label::j(in)) :
            in.byteIndexFromLeft == TILE_SIZE ? "a=old_m-new_m" : ""
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
            in.byteIndexFromLeft < TILE_SIZE ? label::one("S", label::j(in)) :
            in.byteIndexFromLeft == TILE_SIZE ? "old_m" : ""
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
            in.byteIndexFromLeft < TILE_SIZE ? label::one("P", label::j(in)) :
            in.byteIndexFromLeft == TILE_SIZE ? "b=exp(a)" : ""
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
            in.byteIndexFromLeft < TILE_SIZE ? label::one("P", label::j(in)) :
            in.byteIndexFromLeft == TILE_SIZE ? "b" : ""
        };
    }

    FloatOut phaseH(const PhaseInput& in) override {
        return {
            in.floatHFromLeft,
            in.byteIndexFromLeft < TILE_SIZE ? label::one("P", label::j(in)) :
            in.byteIndexFromLeft == TILE_SIZE ? "b" : ""
        };
    }
};

class LocalLCell : public Cell {
public:
    explicit LocalLCell(unsigned int delay, uint8_t y, const std::string& tag1, const std::string& tag2)
        : Cell({Option::UseH, Option::UseV, Option::UseOld}, y, delay, tag1, tag2) {}
protected:
    FloatOut phaseH(const PhaseInput& in) override {
        return {
            in.byteIndexFromLeft == TILE_SIZE
                ? in.floatOldFromSelf * in.floatHFromLeft + in.floatVFromSelf
                : 0.0f,
            in.byteIndexFromLeft == TILE_SIZE - 1 ? "local_l" :
            in.byteIndexFromLeft == TILE_SIZE ? "new_l=old_l*b+local_l" : ""
        };
    }

    FloatOut phaseV(const PhaseInput& in) override {
        return {
            (in.byteIndexFromLeft == 0)
                ? in.floatHFromLeft
            : (in.byteIndexFromLeft < TILE_SIZE)
                ? in.floatVFromSelf + in.floatHFromLeft
                : 0.0f,
            in.byteIndexFromLeft < TILE_SIZE ? label::prefix("local_l", label::j(in)) : ""
        };
    }

    FloatOut phaseOld(const PhaseInput& in) override {
        return {
            (in.byteIndexFromLeft == TILE_SIZE && in.lastFromLeft == 1)
                ? 0.0f
            : (in.byteIndexFromLeft == TILE_SIZE)
                ? in.floatOldFromSelf * in.floatHFromLeft + in.floatVFromSelf
                : in.floatOldFromSelf,
            in.byteIndexFromLeft == TILE_SIZE ? "old_l=new_l" : ""
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
            in.byteIndexFromLeft == 0 ? "1/new_l" : ""
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
            in.byteIndexFromLeft < TILE_SIZE ? label::one("O", label::j(in)) + "=new_O/new_l" : ""
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
            in.byteIndexFromLeft < TILE_SIZE ? label::one("OSRAM", label::j(in)) : ""
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
