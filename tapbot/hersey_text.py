from __future__ import annotations

from dataclasses import dataclass
from enum import IntEnum

# taken from: https://github.com/osresearch/vst/blob/master/teensyv/asteroids_font.c
#
# define P(x,y)	((((x) & 0xF) << 4) | (((y) & 0xF) << 0))
# const asteroids_char_t asteroids_font[] = {
# 	['0' - 0x20] = { P(0,0), P(8,0), P(8,12), P(0,12), P(0,0), P(8,12), FONT_LAST },
# 	['1' - 0x20] = { P(4,0), P(4,12), P(3,10), FONT_LAST },
# ...
# FONT_LAST means end of character
# FONT_UP means move to next point without writing


@dataclass
class Point2i:
    x: int
    y: int


P = Point2i

SPACING = 2
HERSEY_FONT_WIDTH = 8
HERSEY_FONT_HEIGHT = 12
HERSEY_FONT_CHARS: dict[str, list[list[Point2i]]] = {
    "0": [[P(0, 0), P(8, 0), P(8, 12), P(0, 12), P(0, 0), P(8, 12)]],
    "1": [[P(4, 0), P(4, 12), P(3, 10)]],
    "2": [[P(0, 12), P(8, 12), P(8, 7), P(0, 5), P(0, 0), P(8, 0)]],
    "3": [
        [P(0, 12), P(8, 12), P(8, 0), P(0, 0)],
        [P(0, 6), P(8, 6)],
    ],
    "4": [
        [P(0, 12), P(0, 6), P(8, 6)],
        [P(8, 12), P(8, 0)],
    ],
    "5": [[P(0, 0), P(8, 0), P(8, 6), P(0, 7), P(0, 12), P(8, 12)]],
    "6": [[P(0, 12), P(0, 0), P(8, 0), P(8, 5), P(0, 7)]],
    "7": [[P(0, 12), P(8, 12), P(8, 6), P(4, 0)]],
    "8": [
        [P(0, 0), P(8, 0), P(8, 12), P(0, 12), P(0, 0)],
        [P(0, 6), P(8, 6)],
    ],
    "9": [[P(8, 0), P(8, 12), P(0, 12), P(0, 7), P(8, 5)]],
    " ": [[]],
    ".": [[P(3, 0), P(4, 0)]],
    ",": [[P(2, 0), P(4, 2)]],
    "-": [[P(2, 6), P(6, 6)]],
    "+": [
        [P(1, 6), P(7, 6)],
        [P(4, 9), P(4, 3)],
    ],
    "!": [
        [P(4, 0), P(3, 2), P(5, 2), P(4, 0)],
        [P(4, 4), P(4, 12)],
    ],
    "#": [[P(0, 4), P(8, 4), P(6, 2), P(6, 10), P(8, 8), P(0, 8), P(2, 10), P(2, 2)]],
    "^": [[P(2, 6), P(4, 12), P(6, 6)]],
    "=": [
        [P(1, 4), P(7, 4)],
        [P(1, 8), P(7, 8)],
    ],
    "*": [[P(0, 0), P(4, 12), P(8, 0), P(0, 8), P(8, 8), P(0, 0)]],
    "_": [[P(0, 0), P(8, 0)]],
    "/": [[P(0, 0), P(8, 12)]],
    "\\": [[P(0, 12), P(8, 0)]],
    "@": [[P(8, 4), P(4, 0), P(0, 4), P(0, 8), P(4, 12), P(8, 8), P(4, 4), P(3, 6)]],
    "$": [
        [P(6, 2), P(2, 6), P(6, 10)],
        [P(4, 12), P(4, 0)],
    ],
    "&": [[P(8, 0), P(4, 12), P(8, 8), P(0, 4), P(4, 0), P(8, 4)]],
    "[": [[P(6, 0), P(2, 0), P(2, 12), P(6, 12)]],
    "]": [[P(2, 0), P(6, 0), P(6, 12), P(2, 12)]],
    "(": [[P(6, 0), P(2, 4), P(2, 8), P(6, 12)]],
    ")": [[P(2, 0), P(6, 4), P(6, 8), P(2, 12)]],
    "{": [
        [P(6, 0), P(4, 2), P(4, 10), P(6, 12)],
        [P(2, 6), P(4, 6)],
    ],
    "}": [
        [P(4, 0), P(6, 2), P(6, 10), P(4, 12)],
        [P(6, 6), P(8, 6)],
    ],
    "%": [
        [P(0, 0), P(8, 12)],
        [P(2, 10), P(2, 8)],
        [P(6, 4), P(6, 2)],
    ],
    "<": [[P(6, 0), P(2, 6), P(6, 12)]],
    ">": [[P(2, 0), P(6, 6), P(2, 12)]],
    "|": [
        [P(4, 0), P(4, 5)],
        [P(4, 6), P(4, 12)],
    ],
    ":": [
        [P(4, 9), P(4, 7)],
        [P(4, 5), P(4, 3)],
    ],
    ";": [
        [P(4, 9), P(4, 7)],
        [P(4, 5), P(1, 2)],
    ],
    '"': [
        [P(2, 10), P(2, 6)],
        [P(6, 10), P(6, 6)],
    ],
    "'": [[P(2, 6), P(6, 10)]],
    "`": [[P(2, 10), P(6, 6)]],
    "~": [[P(0, 4), P(2, 8), P(6, 4), P(8, 8)]],
    "?": [
        [P(0, 8), P(4, 12), P(8, 8), P(4, 4)],
        [P(4, 1), P(4, 0)],
    ],
    "A": [
        [P(0, 0), P(0, 8), P(4, 12), P(8, 8), P(8, 0)],
        [P(0, 4), P(8, 4)],
    ],
    "B": [[P(0, 0), P(0, 12), P(4, 12), P(8, 10), P(4, 6), P(8, 2), P(4, 0), P(0, 0)]],
    "C": [[P(8, 0), P(0, 0), P(0, 12), P(8, 12)]],
    "D": [[P(0, 0), P(0, 12), P(4, 12), P(8, 8), P(8, 4), P(4, 0), P(0, 0)]],
    "E": [
        [P(8, 0), P(0, 0), P(0, 12), P(8, 12)],
        [P(0, 6), P(6, 6)],
    ],
    "F": [
        [P(0, 0), P(0, 12), P(8, 12)],
        [P(0, 6), P(6, 6)],
    ],
    "G": [[P(6, 6), P(8, 4), P(8, 0), P(0, 0), P(0, 12), P(8, 12)]],
    "H": [
        [P(0, 0), P(0, 12)],
        [P(0, 6), P(8, 6)],
        [P(8, 12), P(8, 0)],
    ],
    "I": [
        [P(0, 0), P(8, 0)],
        [P(4, 0), P(4, 12)],
        [P(0, 12), P(8, 12)],
    ],
    "J": [[P(0, 4), P(4, 0), P(8, 0), P(8, 12)]],
    "K": [
        [P(0, 0), P(0, 12)],
        [P(8, 12), P(0, 6), P(6, 0)],
    ],
    "L": [[P(8, 0), P(0, 0), P(0, 12)]],
    "M": [[P(0, 0), P(0, 12), P(4, 8), P(8, 12), P(8, 0)]],
    "N": [[P(0, 0), P(0, 12), P(8, 0), P(8, 12)]],
    "O": [[P(0, 0), P(0, 12), P(8, 12), P(8, 0), P(0, 0)]],
    "P": [[P(0, 0), P(0, 12), P(8, 12), P(8, 6), P(0, 5)]],
    "Q": [
        [P(0, 0), P(0, 12), P(8, 12), P(8, 4), P(0, 0)],
        [P(4, 4), P(8, 0)],
    ],
    "R": [
        [P(0, 0), P(0, 12), P(8, 12), P(8, 6), P(0, 5)],
        [P(4, 5), P(8, 0)],
    ],
    "S": [[P(0, 2), P(2, 0), P(8, 0), P(8, 5), P(0, 7), P(0, 12), P(6, 12), P(8, 10)]],
    "T": [
        [P(0, 12), P(8, 12)],
        [P(4, 12), P(4, 0)],
    ],
    "U": [[P(0, 12), P(0, 2), P(4, 0), P(8, 2), P(8, 12)]],
    "V": [[P(0, 12), P(4, 0), P(8, 12)]],
    "W": [[P(0, 12), P(2, 0), P(4, 4), P(6, 0), P(8, 12)]],
    "X": [
        [P(0, 0), P(8, 12)],
        [P(0, 12), P(8, 0)],
    ],
    "Y": [
        [P(0, 12), P(4, 6), P(8, 12)],
        [P(4, 6), P(4, 0)],
    ],
    "Z": [
        [P(0, 12), P(8, 12), P(0, 0), P(8, 0)],
        [P(2, 6), P(6, 6)],
    ],
}


class Pen(IntEnum):
    Write = 0
    Move = 1


@dataclass
class DrawPoint:
    x_mm: float
    y_mm: float
    pen: Pen  # to reach it


def write_text(text: str, max_x_mm: float) -> list[DrawPoint]:
    total_width_mm = len(text) * (HERSEY_FONT_WIDTH + SPACING)
    scale = max_x_mm / total_width_mm
    lines_mm: list[DrawPoint] = []
    for i, c in enumerate(text):
        dx = i * (HERSEY_FONT_WIDTH + SPACING)
        segments = HERSEY_FONT_CHARS[c]
        for segment in segments:
            # could be an empty segment (i.e. " ")
            if not segment:
                continue
            # draw the lines
            for j, pt in enumerate(segment):
                if j == 0:
                    # move to segment start
                    lines_mm.append(
                        DrawPoint(
                            x_mm=scale * (pt.x + dx),
                            y_mm=scale * (pt.y),
                            pen=Pen.Move,
                        )
                    )
                else:
                    lines_mm.append(
                        DrawPoint(
                            x_mm=scale * (pt.x + dx),
                            y_mm=scale * (pt.y),
                            pen=Pen.Write,
                        )
                    )
    return lines_mm


if __name__ == "__main__":
    import matplotlib.pyplot as plt

    points = write_text(
        text="".join(HERSEY_FONT_CHARS),
        max_x_mm=35.0,
    )

    fig, ax = plt.subplots(1, 1, figsize=(16, 6))
    for i, pt in enumerate(points):
        if i == 0:
            xs = [0.0, pt.x_mm]
            ys = [0.0, pt.y_mm]
        else:
            xs = [points[i - 1].x_mm, pt.x_mm]
            ys = [points[i - 1].y_mm, pt.y_mm]
        color = "b--" if pt.pen == Pen.Move else "r-"
        ax.plot(
            xs,
            ys,
            color,
        )
    ax.axis("equal")
    fig.savefig("hersey_text_test.png")
    plt.show()
