from __future__ import annotations

import argparse
import math
from dataclasses import dataclass

TOOL_ENABLE = "SP,0"  # lower pen
TOOL_DISABLE = "SP,1"
MOTORS_DISABLE = "EM,0"
LINE_ENDING = "\n"


class Parameters:
    steps_per_mm_x: int = 155  # linear: constant (measured line with known steps)
    steps_per_mm_y: int = 85  # rotation: depends on diameter (6400 / circumference_mm)
    steps_per_ms: float = 1.0


@dataclass
class Options:
    input_filename: str
    output_filename: str


def parse_input() -> Options:
    parser = argparse.ArgumentParser()
    parser.add_argument("filename", type=str)
    parser.add_argument("-o", "--output-filename", type=str, default="from_gcode.egg")
    args = parser.parse_args()
    return Options(
        input_filename=args.filename,
        output_filename=args.output_filename,
    )


@dataclass
class MoveSteps:
    motor_x_steps: int
    motor_y_steps: int
    duration_ms: int

    @staticmethod
    def from_delta(delta_x_mm: float, delta_y_mm: float) -> "MoveSteps":
        # mm to steps
        motor_x_steps = int(round(delta_x_mm * Parameters.steps_per_mm_x))
        motor_y_steps = int(round(delta_y_mm * Parameters.steps_per_mm_y))

        # duration according to distance
        distance_steps = math.sqrt(motor_x_steps * motor_x_steps + motor_y_steps * motor_y_steps)
        duration_ms = int(round(distance_steps / Parameters.steps_per_ms))
        return MoveSteps(
            motor_x_steps=motor_x_steps,
            motor_y_steps=motor_y_steps,
            duration_ms=duration_ms,
        )

    def render(self) -> str:
        return f"SM,{self.duration_ms},{self.motor_x_steps},{self.motor_y_steps}\n"


def translate_gcode_to_eggbot(options: Options) -> list[str]:
    lines: list[str] = []

    last_x_mm = 0.0
    last_y_mm = 0.0
    with open(options.input_filename, "r") as fh:
        # move pen up
        lines.append(TOOL_DISABLE + LINE_ENDING)

        for line in fh.readlines():
            # remove whitespace and convert to uppercase for easier parsing
            line = line.strip().upper()

            # skip empty lines and comments
            if not line or line.startswith(";"):
                continue

            # toolhead
            if line.startswith("M3") or line.startswith("M4"):
                lines.append(TOOL_ENABLE + LINE_ENDING)
                continue
            elif line.startswith("M5"):
                lines.append(TOOL_DISABLE + LINE_ENDING)
                continue

            if line.startswith(("G0 ", "G1 ")):
                current_x_mm = last_x_mm
                current_y_mm = last_y_mm

                if "X" in line:
                    current_x_mm = float(line.split("X")[1].split()[0].split(";")[0])
                if "Y" in line:
                    current_y_mm = float(line.split("Y")[1].split()[0].split(";")[0])

                # Calculate incremental steps
                lines.append(
                    MoveSteps.from_delta(
                        delta_x_mm=current_x_mm - last_x_mm,
                        delta_y_mm=current_y_mm - last_y_mm,
                    ).render()
                    + LINE_ENDING
                )

                last_x_mm = current_x_mm
                last_y_mm = current_y_mm
            else:
                print(f"ignored line {repr(line)}")

        # home to X0 Y0 and disable
        lines.append(TOOL_DISABLE + LINE_ENDING)
        lines.append(
            MoveSteps.from_delta(
                delta_x_mm=-last_x_mm,
                delta_y_mm=-last_y_mm,
            ).render()
            + LINE_ENDING
        )
    return lines


if __name__ == "__main__":
    # parse arguments
    options = parse_input()

    # translate
    lines = translate_gcode_to_eggbot(options)

    # save
    with open(options.output_filename, "w") as fh:
        fh.write("".join(lines))
    print(f"Translation from GCODE complete. Output saved to '{options.output_filename}'")
