#pragma once

#include "mini_torch/torch.h"


#include "cell_array.hpp"
#include "cell_types.hpp"

constexpr int TOKEN = TOKEN_COUNT;
constexpr int DIM   = HEAD_DIMENTION;
constexpr int TILE  = TILE_SIZE;
constexpr int NTILE = TOKEN / TILE;

class FlashArray {
public:
    FlashArray() {
        constexpr int D = HEAD_DIMENTION;
        constexpr int qx = D;
        constexpr int macDelayX = D + 1;
        constexpr int scoreMuxX = D + 2;
        constexpr int maxCopyStartX = D + 4;
        constexpr int subX = 2 * D + 4;
        constexpr int expDelayX = 2 * D + 6;
        constexpr int localLX = 2 * D + 7;
        constexpr int localLDelayX = 2 * D + 8;
        constexpr int recipX = 2 * D + 9;
        constexpr int finalSramX = 2 * D + 10;

        for (int y = 1; y <= D; ++y) {
            cells_.add<KLoadCell>({y - 1, y}, 1, "K Load");
        }

        for (int y = 1; y < D; ++y) {
            for (int x = y; x < D; ++x) {
                cells_.add<CopyCell>({x, y}, 1, "K Copy", "K");
            }
        }

        cells_.add<QLoadCell>({qx, D + 1}, D, "Q Load");
        for (int y = D; y >= 1; --y) {
            cells_.add<MacCell>({qx, y}, D, "MAC", "Q", "V");
        }

        for (int y = D; y >= 1; --y) {
            cells_.add<MacDelay1Cell>({macDelayX, y}, 1, "MacDelay1", "S");
        }

        cells_.add<Mux1Cell>({scoreMuxX, D}, 1, "MUX1", "S");
        cells_.add<MaxOldMCell>({MAX_OLD_X, D}, 1, "newM/oldM");

        for (int x = maxCopyStartX; x < subX; ++x) {
            cells_.add<MaxCopyCell>({x, D}, 1, "Copy", "S");
        }

        cells_.add<SubCell>({subX, D}, 1, "Sub");
        cells_.add<ExpCell>({EXP_X, D}, 1, "Exp");
        cells_.add<ExpDelayCell>({expDelayX, D}, 1, "ExpDelay");

        cells_.add<LocalLCell>({localLX, D}, BYTE_INDEX_IDLE, "LocalL");
        cells_.add<MacDelay2Cell>({localLDelayX, D}, 1, "Copy", "LocalL");

        cells_.add<RecipCell>({recipX, D}, 2, "Recip");

        for (int y = D - 1; y >= 0; --y) {
            const int vLoadX = D + 6 + y;
            cells_.add<VLoadCell>({vLoadX, y}, 1, "VLoad");
            for (int x = vLoadX + 1; x <= EXP_X; ++x) {
                cells_.add<CopyCell>({x, y}, 1, "Copy", "V");
            }
        }

        for (int y = D - 1; y >= 0; --y) {
            cells_.add<PvMacCell>({expDelayX, y}, BYTE_INDEX_IDLE, "Mac", "P", "V");
        }

        for (int y = D - 1; y >= 0; --y) {
            cells_.add<MacDelay2Cell>({localLX, y}, 1, "MacDelay1", "O");
        }

        cells_.add<Mux1Cell>({localLDelayX, D - 1}, 1, "Mux1", "V");
        cells_.add<FinalOCell>({recipX, D - 1}, 1, "FinalO");

        cells_.add<FinalSramCell>({finalSramX, D - 1}, 1, "FinalSram");

        cells_.create();
    }

    torch::Tensor run(torch::Tensor Q, torch::Tensor K, torch::Tensor V) {
        load(Q, K, V);

        for (int t = 0; t < NTILE; ++t) {
            cells_.qaddrReg() = 0;
            cells_.kaddrReg() = t * TILE;
            cells_.lastReg()  = (t == NTILE - 1);

            cells_.vaddrFifo().push(t * TILE);
            cells_.oaddrFifo().push(0);

            for (int i = 0; i < BYTE_INDEX_IDLE; ++i) {
                cells_.index() = i;
                cells_.tick();
            }
        }

        cells_.index() = BYTE_INDEX_IDLE;

        for (int i = 0; i < flushTicks(); ++i) {
            cells_.tick();
        }

        return torch::from_blob(cells_.osram().data(), {DIM}, torch::kFloat32).clone();
    }

    void writeTrace(torch::Tensor Q, torch::Tensor K, torch::Tensor V, const std::string& path) {
        load(Q, K, V);

        std::ofstream out(path);
        out << std::fixed << std::setprecision(7);
        out << "{\n";
        out << "  \"cellWidth\": 236,\n";
        out << "  \"cellHeight\": 176,\n";
        out << "  \"frames\": [\n";

        dumpSnapshot(out, 0);

        int tickNo = 0;
        for (int t = 0; t < NTILE; ++t) {
            cells_.qaddrReg() = 0;
            cells_.kaddrReg() = t * TILE;
            cells_.lastReg()  = (t == NTILE - 1);

            cells_.vaddrFifo().push(t * TILE);
            cells_.oaddrFifo().push(0);

            for (int i = 0; i < BYTE_INDEX_IDLE; ++i) {
                cells_.index() = i;
                cells_.tick();
                out << ",\n";
                dumpSnapshot(out, ++tickNo);
            }
        }

        cells_.index() = BYTE_INDEX_IDLE;

        for (int i = 0; i < flushTicks(); ++i) {
            cells_.tick();
            out << ",\n";
            dumpSnapshot(out, ++tickNo);
        }

        out << "\n  ]\n";
        out << "}\n";
    }

private:
    static constexpr int flushTicks() {
        return 2 * TOKEN + 6 * DIM + 64;
    }

    void load(torch::Tensor Q, torch::Tensor K, torch::Tensor V) {
        loadSram(cells_.qsram(), Q);
        loadSram(cells_.ksram(), K.reshape({TOKEN * DIM}));
        loadSram(cells_.vsram(), V.reshape({TOKEN * DIM}));
        cells_.osram().fill(0.0f);

        while (!cells_.vaddrFifo().empty()) cells_.vaddrFifo().pop();
        while (!cells_.oaddrFifo().empty()) cells_.oaddrFifo().pop();

        cells_.index() = BYTE_INDEX_IDLE;
        cells_.qaddrReg() = 0;
        cells_.kaddrReg() = 0;
        cells_.lastReg() = 0;
    }

    static void loadSram(CellArray::Sram& dst, torch::Tensor t) {
        t = t.contiguous().to(torch::kFloat32);
        dst.fill(0.0f);
        const auto n = std::min(dst.size(), static_cast<std::size_t>(t.numel()));
        std::memcpy(dst.data(), t.data_ptr<float>(), n * sizeof(float));
    }

    void dumpSnapshot(std::ostream& out, int tickNo) {
        out << "    {\n";
        out << "      \"tick\": " << tickNo << ",\n";
        out << "      \"cells\": [\n";

        bool firstCell = true;

        const auto& specs = cells_.specs();
        for (size_t i = 0; i < specs.size(); ++i) {
            const auto& s = specs[i];
            const Cell& c = cells_.at(s.x, s.y);
            if (!firstCell) {
                out << ",\n";
            }
            firstCell = false;
            out << "        {";
            out << "\"x\":" << s.x << ",";
            out << "\"y\":" << s.y << ",";
            out << "\"type\":\"" << s.typeName << "\",";
            out << "\"last\":" << (c.boolLast() ? 1 : 0) << ",";
            out << "\"index\":" << static_cast<int>(c.byteIndex()) << ",";
            out << "\"delay\":" << s.delay << ",";
            out << "\"string\":\"" << c.label() << "\",";
            out << "\"calcu0String\":\"" << c.floatCalcu0String() << "\",";
            out << "\"calcu1String\":\"" << c.floatCalcu1String() << "\",";
            out << "\"hString\":\"" << c.floatHString() << "\",";
            out << "\"vString\":\"" << c.floatVString() << "\",";
            out << "\"oldString\":\"" << c.floatOldString() << "\",";
            out << "\"color\":" << c.color() << ",";
            out << "\"useCalcu0\":" << (c.useFloatCalcu0() ? "true" : "false") << ",";
            out << "\"useCalcu1\":" << (c.useFloatCalcu1() ? "true" : "false") << ",";
            out << "\"useH\":" << (c.useFloatH() ? "true" : "false") << ",";
            out << "\"useV\":" << (c.useFloatV() ? "true" : "false") << ",";
            out << "\"useOld\":" << (c.useFloatOld() ? "true" : "false") << ",";
            out << "\"floatCalcu0\":" << c.floatCalcu0() << ",";
            out << "\"floatCalcu1\":" << c.floatCalcu1() << ",";
            out << "\"floatH\":" << c.floatH() << ",";
            out << "\"floatV\":" << c.floatV() << ",";
            out << "\"floatOld\":" << c.floatOld();
            out << "}";
        }

        out << "\n";

        out << "      ]\n";
        out << "    }";
    }

private:
    CellArray cells_;
};


