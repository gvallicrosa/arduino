from __future__ import annotations

import argparse
from dataclasses import dataclass

from convert_from_gcode import TOOL_DISABLE, TOOL_ENABLE, MoveSteps
from cork import Parameters
from hersey_text import DrawPoint, Pen, write_text


def to_machine(points_mm: list[DrawPoint]) -> list[str]:
    lines: list[str] = []
    last_pen = Pen.Write
    for i, pt in enumerate(points_mm):
        # tool
        if last_pen != pt.pen:
            last_pen = pt.pen
            if pt.pen == Pen.Move:
                lines.append(TOOL_DISABLE)
            elif pt.pen == Pen.Write:
                lines.append(TOOL_ENABLE)
        # move
        if i == 0:
            delta_x_mm = pt.x_mm
            delta_y_mm = pt.y_mm
        else:
            delta_x_mm = pt.x_mm - points_mm[i - 1].x_mm
            delta_y_mm = pt.y_mm - points_mm[i - 1].y_mm
        lines.append(
            MoveSteps.from_delta(
                delta_x_mm=delta_x_mm,
                delta_y_mm=delta_y_mm,
            ).render()
        )
    # end
    lines.append(TOOL_DISABLE)
    lines.append(
        MoveSteps.from_delta(
            delta_x_mm=-points_mm[-1].x_mm,
            delta_y_mm=-points_mm[-1].y_mm,
        ).render()
    )
    return lines


@dataclass
class Options:
    text: str
    output_filename: str


def parse_input() -> Options:
    parser = argparse.ArgumentParser()
    parser.add_argument("text", type=str)
    parser.add_argument("-o", "--output-filename", type=str, default="autoname.egg")
    args = parser.parse_args()
    return Options(
        text=args.text,
        output_filename=args.output_filename,
    )


def main() -> None:
    # input
    options = parse_input()

    # parse
    points_mm = write_text(
        text=options.text,
        max_x_mm=Parameters.max_x_mm,
    )
    lines = to_machine(points_mm=points_mm)

    # save
    with open(options.output_filename, "w") as fh:
        fh.write("\n".join(lines))
    print(f"Name generation complete. Output saved to '{options.output_filename}'")


if __name__ == "__main__":
    main()
