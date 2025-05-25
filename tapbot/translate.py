import argparse
import math
import re
from dataclasses import dataclass

TOOL_ENABLE = "SP,0"  # lower pen
TOOL_DISABLE = "SP,1"
MOTORS_DISABLE = "EM,0"
LINE_ENDING = "\n"


@dataclass
class Options:
    filename: str
    steps_per_mm_x: int = 155  # linear: constant (measured line with known steps)
    steps_per_mm_y: int = 85  # rotation: depends on diameter (6400 / circumference_mm)
    steps_per_ms: float = 1.0  # 0.5


def parse_arguments() -> Options:
    parser = argparse.ArgumentParser()
    parser.add_argument("filename")
    args = parser.parse_args()
    return Options(
        filename=args.filename,
    )


@dataclass
class MoveSteps:
    motor_x_steps: int
    motor_y_steps: int
    duration_ms: int

    @staticmethod
    def from_delta(delta_x_mm: float, delta_y_mm: float, options: Options) -> "MoveSteps":
        # mm to steps
        motor_x_steps = int(round(delta_x_mm * options.steps_per_mm_x))
        motor_y_steps = int(round(delta_y_mm * options.steps_per_mm_y))

        # duration according to distance
        distance_steps = math.sqrt(motor_x_steps * motor_x_steps + motor_y_steps * motor_y_steps)
        duration_ms = int(round(distance_steps / options.steps_per_ms))
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
    with open(options.filename, "r") as fh:
        # move pen up
        lines.append(TOOL_DISABLE + LINE_ENDING)

        for line in fh.readlines():
            # remove whitespace and convert to uppercase for easier parsing
            line = line.strip().upper()

            # skip empty lines and comments
            if not line or line.startswith(";"):
                continue

            # toolhead
            if line.startswith("M4"):
                lines.append(TOOL_ENABLE + LINE_ENDING)
                continue
            elif line.startswith("M5"):
                lines.append(TOOL_DISABLE + LINE_ENDING)
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
                lines.append(
                    MoveSteps.from_delta(
                        delta_x_mm=current_x_mm - last_x_mm,
                        delta_y_mm=current_y_mm - last_y_mm,
                        options=options,
                    ).render()
                    + LINE_ENDING
                )

                last_x_mm = current_x_mm
                last_y_mm = current_y_mm
            else:
                print(f"ignored line {repr(line)}")

        # home to X0 Y0 and disable
        lines.append(
            MoveSteps.from_delta(
                delta_x_mm=-last_x_mm,
                delta_y_mm=-last_y_mm,
                options=options,
            ).render()
            + LINE_ENDING
        )
        lines.append(MOTORS_DISABLE + LINE_ENDING)
    return lines


if __name__ == "__main__":
    # parse arguments
    options = parse_arguments()

    # translate
    lines = translate_gcode_to_eggbot(options)

    # save
    output_filename = "output.egg"
    with open(output_filename, "w") as fh:
        fh.write("".join(lines))
    print(f"Translation complete. Output saved to '{output_filename}'")
