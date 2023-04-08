struct Note {
  unsigned long pitch;
  unsigned long duration;
};

// note durations: 4 = quarter note, 8 = eighth note, etc.:
constexpr Note m_victory[] = {
  { NOTE_G4, 8 },
  { NOTE_A4, 8 },
  { NOTE_B4, 8 },
  { NOTE_C5, 4 },
  { NOTE_B4, 8 },
  { NOTE_C5, 2 },
};
constexpr size_t m_victory_size = sizeof(m_victory) / sizeof(Note);

constexpr Note m_start[]{
  { NOTE_G3, 2 },
  { NOTE_G3, 2 },
  { NOTE_G3, 2 },
  { NOTE_G4, 1 },
};
constexpr size_t m_start_size = sizeof(m_start) / sizeof(Note);

constexpr Note m_error[]{
  { 153, 2 },  // NOTA ENTRE D4 I DS4, malament expressament
};
constexpr size_t m_error_size = sizeof(m_error) / sizeof(Note);

constexpr Note m_correct[]{
  { NOTE_G4, 16 },
};
constexpr size_t m_correct_size = sizeof(m_correct) / sizeof(Note);

constexpr Note m_countdown[]{
  { NOTE_G4, 2 },
  { NOTE_G4, 2 },
  { NOTE_G4, 2 },
  { NOTE_G4, 4 },
  { NOTE_G4, 4 },
  { NOTE_G4, 4 },
  { NOTE_G4, 4 },
  { NOTE_G4, 8 },
  { NOTE_G4, 8 },
  { NOTE_G4, 8 },
  { NOTE_G4, 8 },
  { NOTE_G4, 8 },
  { NOTE_G4, 8 },
  { NOTE_G4, 8 },
  { NOTE_G4, 8 },
  { NOTE_G4, 12 },
  { NOTE_G4, 12 },
  { NOTE_G4, 12 },
  { NOTE_G4, 12 },
  { NOTE_G4, 12 },
  { NOTE_G4, 12 },
  { NOTE_G4, 12 },
  { NOTE_G4, 12 },
  { NOTE_G4, 12 },
  { NOTE_G4, 12 },
  { NOTE_G4, 12 },
  { NOTE_G4, 12 },  // total time ms = 10310
};
constexpr size_t m_countdown_size = sizeof(m_countdown) / sizeof(Note);
