from __future__ import annotations

import argparse
from dataclasses import dataclass

from svg_to_gcode import TOLERANCES
from svg_to_gcode.compiler import Compiler, interfaces
from svg_to_gcode.svg_parser import parse_file

TOLERANCES["approximation"] = 1.0


@dataclass
class Options:
    input_filename: str
    output_filename: str


def parse_input() -> Options:
    parser = argparse.ArgumentParser()
    parser.add_argument("filename", type=str)
    parser.add_argument("-o", "--output-filename", type=str, default="from_svg.gcode")
    args = parser.parse_args()
    return Options(
        input_filename=args.filename,
        output_filename=args.output_filename,
    )


def main() -> None:
    # input
    options = parse_input()

    # parse
    gcode_compiler = Compiler(
        interface_class=interfaces.Gcode,
        movement_speed=100,
        cutting_speed=100,
        pass_depth=0,
        unit="mm",
    )
    curves = parse_file(file_path=options.input_filename)
    gcode_compiler.append_curves(curves=curves)

    # save
    gcode_compiler.compile_to_file(file_name=options.output_filename)
    print(f"Translation from SVG complete. Output saved to '{options.output_filename}'")


if __name__ == "__main__":
    main()
