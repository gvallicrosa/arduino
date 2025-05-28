from __future__ import annotations

import argparse
import re
from dataclasses import dataclass

import matplotlib.pyplot as plt
from convert_from_gcode import TOOL_DISABLE, TOOL_ENABLE
from cork import Parameters
from hersey_text import Pen


@dataclass
class Options:
    filename_gcode: str
    filename_eggbot: str


def parse_arguments() -> Options:
    parser = argparse.ArgumentParser()
    parser.add_argument("-g", "--filename-gcode", default="")
    parser.add_argument("-e", "--filename-eggbot", default="")
    args = parser.parse_args()
    if not (args.filename_gcode or args.filename_eggbot):
        parser.error("at least one file provided to plot")
    return Options(
        filename_gcode=args.filename_gcode,
        filename_eggbot=args.filename_eggbot,
    )


def plot_eggbot(filename: str, ax: plt.Axes, ax_mm: plt.axes) -> None:
    pen_state = Pen.Move
    current_x_steps = 0
    current_y_steps = 0

    with open(filename, "r") as fh:
        for line in fh.readlines():
            # remove whitespace and convert to uppercase for easier parsing
            line = line.strip().upper()

            # skip empty lines and comments
            if not line or line.startswith(";"):
                continue

            # toolhead
            if line.startswith(TOOL_ENABLE):
                pen_state = Pen.Write
                continue
            elif line.startswith(TOOL_DISABLE):
                pen_state = Pen.Move
                continue

            # moves
            if line.startswith("SM,"):
                temp = line.split(",")
                delta_x_steps = int(temp[2])
                delta_y_steps = int(temp[3])

                # Calculate incremental steps
                ax.plot(
                    [current_x_steps, current_x_steps + delta_x_steps],
                    [current_y_steps, current_y_steps + delta_y_steps],
                    "r-" if pen_state == Pen.Write else "r--",
                )
                ax_mm.plot(
                    [
                        current_x_steps / Parameters.steps_per_mm_x,
                        (current_x_steps + delta_x_steps) / Parameters.steps_per_mm_x,
                    ],
                    [
                        current_y_steps / Parameters.steps_per_mm_y,
                        (current_y_steps + delta_y_steps) / Parameters.steps_per_mm_y,
                    ],
                    "r-" if pen_state == Pen.Write else "r--",
                )
                current_x_steps += delta_x_steps
                current_y_steps += delta_y_steps
            else:
                print(f"ignored line {repr(line)}")


def plot_gcode(filename: str, ax: plt.Axes) -> None:
    pen_state = Pen.Move
    last_x_mm = 0.0
    last_y_mm = 0.0

    with open(filename, "r") as fh:
        for line in fh.readlines():
            # remove whitespace and convert to uppercase for easier parsing
            line = line.strip().upper()

            # skip empty lines and comments
            if not line or line.startswith(";"):
                continue

            # toolhead
            if line.startswith("M3") or line.startswith("M4"):
                pen_state = Pen.Write
                continue
            elif line.startswith("M5"):
                pen_state = Pen.Move
                continue

            # Regular expression to find G-code commands and coordinates
            match = re.match(r"^(G[01])(?:\s+X([\d.\-]+))?(?:\s+Y([\d.\-]+))?(?:\s+F([\d.\-]+))?", line)

            if match:
                current_x_mm = last_x_mm
                current_y_mm = last_y_mm

                x_str = match.group(2)
                y_str = match.group(3)
                if x_str is not None:
                    current_x_mm = float(x_str)
                if y_str is not None:
                    current_y_mm = float(y_str)

                # Calculate incremental steps
                ax.plot(
                    [last_x_mm, current_x_mm],
                    [last_y_mm, current_y_mm],
                    "b-" if pen_state == Pen.Write else "b--",
                )

                last_x_mm = current_x_mm
                last_y_mm = current_y_mm
            else:
                print(f"ignored line {repr(line)}")

        # ending: home to X0 Y0
        ax.plot(
            [last_x_mm, 0.0],
            [last_y_mm, 0.0],
            "b-" if pen_state == Pen.Write else "b--",
        )


if __name__ == "__main__":
    # parse arguments
    options = parse_arguments()

    # translate
    fig, axs = plt.subplots(1, 2, figsize=(16, 8))

    # limits
    max_x_steps = Parameters.max_x_mm * Parameters.steps_per_mm_x
    max_y_steps = Parameters.max_y_mm * Parameters.steps_per_mm_y
    axs[0].plot(
        [0, Parameters.max_x_mm, Parameters.max_x_mm, 0, 0],
        [0, 0, Parameters.max_y_mm, Parameters.max_y_mm, 0],
        "k-",
    )
    axs[0].set_title("gcode (mm)")
    axs[0].axis("equal")
    axs[1].plot(
        [0, max_x_steps, max_x_steps, 0, 0],
        [0, 0, max_y_steps, max_y_steps, 0],
        "k-",
    )
    axs[1].set_title("eggbot (steps)")
    axs[1].axis("equal")

    # parsed files
    if options.filename_gcode:
        plot_gcode(filename=options.filename_gcode, ax=axs[0])
    if options.filename_eggbot:
        plot_eggbot(filename=options.filename_eggbot, ax=axs[1], ax_mm=axs[0])

    plt.show()
