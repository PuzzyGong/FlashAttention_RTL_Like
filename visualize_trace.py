import json
import sys
import tkinter as tk
from pathlib import Path


COLORS = ["#d7263d", "#1b998b", "#f46036", "#2e86de"]
DEFAULT_FILL = "#f7f7f7"
DEFAULT_OUTLINE = "#b8b8b8"
HEADER_FONT = ("Consolas", -24, "bold")
CELL_FONT = ("Consolas", -15)
CELL_FONT_BOLD = ("Consolas", -15, "bold")


def enable_dpi_awareness():
    if sys.platform != "win32":
        return
    try:
        import ctypes

        ctypes.windll.shcore.SetProcessDpiAwareness(2)
    except Exception:
        try:
            ctypes.windll.user32.SetProcessDPIAware()
        except Exception:
            pass


class TraceView:
    def __init__(self, root, trace):
        self.root = root
        self.trace = trace
        self.frames = trace["frames"]
        self.tick = 0
        self.cell_w = trace.get("cellWidth", 360)
        self.cell_h = trace.get("cellHeight", 136)
        self.pad_x = 18
        self.pad_y = 56

        self.canvas = tk.Canvas(root, bg="white")
        self.xbar = tk.Scrollbar(root, orient=tk.HORIZONTAL, command=self.canvas.xview)
        self.ybar = tk.Scrollbar(root, orient=tk.VERTICAL, command=self.canvas.yview)
        self.canvas.configure(xscrollcommand=self.xbar.set, yscrollcommand=self.ybar.set)
        self.canvas.grid(row=0, column=0, sticky="nsew")
        self.ybar.grid(row=0, column=1, sticky="ns")
        self.xbar.grid(row=1, column=0, sticky="ew")
        root.grid_rowconfigure(0, weight=1)
        root.grid_columnconfigure(0, weight=1)

        root.title("Flash Array Trace")
        root.geometry("1200x680")
        root.bind("<Up>", lambda event: self.step(1))
        root.bind("<Down>", lambda event: self.step(-1))
        root.bind("<Right>", lambda event: self.goto(len(self.frames) - 1))
        root.bind("<Left>", lambda event: self.goto(0))
        root.bind("<MouseWheel>", self.on_wheel)
        root.bind("<Button-4>", lambda event: self.step(3))
        root.bind("<Button-5>", lambda event: self.step(-3))
        root.bind("<Configure>", lambda event: self.draw())

        self.draw()

    def on_wheel(self, event):
        self.step(3 if event.delta > 0 else -3)

    def step(self, delta):
        self.goto(self.tick + delta)

    def goto(self, tick):
        self.tick = max(0, min(len(self.frames) - 1, tick))
        self.draw()

    def draw(self):
        self.canvas.delete("all")
        frame = self.frames[self.tick]
        cells = frame["cells"]
        max_y = max(cell["y"] for cell in cells)

        self.canvas.create_text(
            18,
            24,
            text=f"tick {self.tick}/{len(self.frames) - 1}    Up:+1 Down:-1 Wheel:+/-3 Right:last Left:first",
            anchor="w",
            fill="#222",
            font=HEADER_FONT,
        )

        for cell in cells:
            self.draw_cell(cell, max_y)

        max_x = max(cell["x"] for cell in cells)
        self.canvas.configure(
            scrollregion=(
                0,
                0,
                self.pad_x * 2 + (max_x + 1) * self.cell_w,
                self.pad_y * 2 + (max_y + 1) * self.cell_h,
            )
        )

    def draw_cell(self, cell, max_y):
        x0 = self.pad_x + cell["x"] * self.cell_w
        y0 = self.pad_y + (max_y - cell["y"]) * self.cell_h
        x1 = x0 + self.cell_w - 8
        y1 = y0 + self.cell_h - 8
        active = any(cell.get(key, "") != "" for key in ("hString", "vString", "oldString"))
        color = COLORS[cell["color"] % len(COLORS)] if active else "#333333"
        fill = "#fff3f3" if active and cell["color"] % 4 == 0 else DEFAULT_FILL

        self.canvas.create_rectangle(
            x0,
            y0,
            x1,
            y1,
            fill=fill,
            outline=color if active else DEFAULT_OUTLINE,
            width=2 if active else 1,
        )
        title = f"({cell['x']},{cell['y']}) {cell['type']}"
        self.canvas.create_text(
            x0 + 8,
            y0 + 8,
            text=title,
            anchor="nw",
            fill=color,
            font=CELL_FONT_BOLD if active else CELL_FONT,
        )
        self.canvas.create_text(
            x0 + 8,
            y0 + 34,
            text=f"last={cell['last']}  idx={cell['index']}",
            anchor="nw",
            fill=color,
            font=CELL_FONT,
        )
        delay = cell.get("delay")
        if delay is not None and cell["index"] <= delay:
            self.canvas.create_text(
                x0 + 146,
                y0 + 34,
                text=f"delay={cell['index']}/{delay}",
                anchor="nw",
                fill=color,
                font=CELL_FONT,
            )

        base = 58
        self.line(x0, y0, base + 0, "H", cell["useH"], cell["floatH"], cell.get("hString", cell.get("string", "")), color, active)
        self.line(x0, y0, base + 22, "V", cell.get("useV", False), cell.get("floatV", 0.0), cell.get("vString", ""), color, active)
        self.line(x0, y0, base + 44, "Old", cell["useOld"], cell["floatOld"], cell.get("oldString", ""), color, active)

    def line(self, x0, y0, dy, name, enabled, value, tag, color, active):
        tag = tag[:28]
        text = f"{name:<3}= {value: .4f} {tag:<28}" if enabled else f"{name:<3}= {'':>9} {tag:<28}"
        self.canvas.create_text(
            x0 + 8,
            y0 + dy,
            text=text,
            anchor="nw",
            fill=color if active else "#aaaaaa",
            font=CELL_FONT,
        )


def main():
    enable_dpi_awareness()
    path = Path(sys.argv[1] if len(sys.argv) > 1 else "trace.json")
    trace = json.loads(path.read_text(encoding="utf-8"))
    root = tk.Tk()
    TraceView(root, trace)
    root.mainloop()


if __name__ == "__main__":
    main()
