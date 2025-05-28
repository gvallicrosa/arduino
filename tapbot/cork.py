from __future__ import annotations


class Parameters:
    steps_per_mm_x: int = 155  # linear: constant (measured line with known steps)
    steps_per_mm_y: int = 85  # rotation: depends on diameter (6400 / circumference_mm)
    steps_per_ms: float = 1.0
    max_x_mm: float = 40.0  # length of cork
    max_y_mm: float = 75.0  # circumference of cork
