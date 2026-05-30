#pragma once

#include "cell.hpp"

struct Pos {
    int x, y;
    bool operator<(const Pos& r) const {
        return x == r.x ? y < r.y : x < r.x;
    }
};

struct Spec {
    int x, y;
    unsigned int delay;
    const char* typeName;
    std::unique_ptr<Cell> (*factory)(unsigned int, uint8_t, const std::string&, const std::string&);
    std::string tag1;
    std::string tag2;
};

class CellArray {
public:
    using Sram = std::array<float, SRAM_SIZE>;
    using LeftColumnH = std::array<float, HEAD_DIMENTION>;

    template <class CellT>
    void add(
        Pos pos,
        unsigned int delay,
        const char* typeName,
        const std::string& tag1 = "",
        const std::string& tag2 = ""
    ) {
        specs_.push_back({pos.x, pos.y, delay, typeName, &makeCell<CellT>, tag1, tag2});
    }

    void create() {
        for (auto& s : specs_) {
            auto cell = s.factory(s.delay, static_cast<uint8_t>(s.y), s.tag1, s.tag2);
            cell->attach(this);
            cells_[{s.x, s.y}] = std::move(cell);
        }
    }

    void tick() {
        for (auto& s : specs_) {
            at(s.x, s.y).run(makeInput(s), makeSideEffectInput());
        }
        for (auto& s : specs_) {
            at(s.x, s.y).commit();
        }
    }

    Cell& at(int x, int y) {
        return *cells_.at({x, y});
    }

    const Cell& at(int x, int y) const {
        return *cells_.at({x, y});
    }

    std::optional<std::reference_wrapper<const Cell>> resolve(int x, int y) const {
        auto it = cells_.find({x, y});
        if (it == cells_.end()) {
            return std::nullopt;
        }
        return std::cref(*it->second);
    }

    const std::vector<Spec>& specs() const {
        return specs_;
    }

    Sram& qsram() { return qsram_; }
    Sram& ksram() { return ksram_; }
    Sram& vsram() { return vsram_; }
    Sram& osram() { return osram_; }
    const Sram& osram() const { return osram_; }

    uint8_t& index() { return index_; }
    uint8_t& qaddrReg() { return qaddrReg_; }
    uint8_t& kaddrReg() { return kaddrReg_; }
    uint8_t& lastReg() { return lastReg_; }

    std::queue<uint8_t>& vaddrFifo() { return vaddrFifo_; }
    std::queue<uint8_t>& oaddrFifo() { return oaddrFifo_; }

private:
    template <class CellT>
    static std::unique_ptr<Cell> makeCell(
        unsigned int delay,
        uint8_t y,
        const std::string& tag1,
        const std::string& tag2
    ) {
        return std::make_unique<CellT>(delay, y, tag1, tag2);
    }

    Cell::PhaseInput makeInput(const Spec& s) {
        const Cell& cell = at(s.x, s.y);
        const auto left = resolve(s.x - 1, s.y);
        const auto top = resolve(s.x, s.y + 1);
        LeftColumnH leftColumn{};
        for (std::size_t posi = 0; posi < leftColumn.size(); ++posi) {
            const auto source = resolve(s.x - 1, s.y - static_cast<int>(posi));
            leftColumn[posi] = source ? source->get().floatH() : 0.0f;
        }
        const auto expCell = resolve(EXP_X, SCORE_ROW_Y);
        const auto maxOldCell = resolve(MAX_OLD_X, SCORE_ROW_Y);

        return {
            static_cast<uint8_t>(s.y),
            s.tag1,
            s.tag2,
            cell.floatCalcu0(),
            cell.floatCalcu1(),
            cell.floatH(),
            cell.floatV(),
            cell.floatOld(),

            left.has_value(),
            left ? static_cast<int>(left->get().boolLast()) : 0,
            left ? left->get().byteIndex()  : static_cast<uint8_t>(BYTE_INDEX_IDLE),
            left ? left->get().delay() : static_cast<uint8_t>(1),
            left ? left->get().floatCalcu0() : 0.0f,
            left ? left->get().floatCalcu1() : 0.0f,
            left ? left->get().floatH()     : 0.0f,
            left ? left->get().floatV()     : 0.0f,
            left ? left->get().floatOld()   : 0.0f,
            leftColumn,
            top ? top->get().floatV() : 0.0f,
            expCell ? expCell->get().byteIndex() : static_cast<uint8_t>(BYTE_INDEX_IDLE),
            maxOldCell ? maxOldCell->get().floatOld() : 0.0f,

            index_,
            qaddrReg_,
            kaddrReg_,
            lastReg_,

            vaddrFifo_,
            oaddrFifo_,

            qsram_,
            ksram_,
            vsram_
        };
    }

    Cell::SideEffectInput makeSideEffectInput() {
        return {
            vaddrFifo_,
            oaddrFifo_,
            osram_
        };
    }

private:
    std::vector<Spec> specs_;
    std::map<Pos, std::unique_ptr<Cell>> cells_;

    uint8_t index_ = BYTE_INDEX_IDLE;
    uint8_t qaddrReg_ = 0;
    uint8_t kaddrReg_ = 0;
    uint8_t lastReg_ = 0;

    std::queue<uint8_t> vaddrFifo_;
    std::queue<uint8_t> oaddrFifo_;

    Sram qsram_{};
    Sram ksram_{};
    Sram vsram_{};
    Sram osram_{};
};

