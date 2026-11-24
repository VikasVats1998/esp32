// Names shared with the firmware. Keep in sync with face.h / motion.h / audio.h / pins.h.
import type { RobotKind } from './types';

export const MOODS = [
  'normal', 'happy', 'sad', 'sleepy', 'angry', 'surprised', 'dizzy', 'love', 'thinking', 'listening',
  'jealous', 'sulk', 'shy', 'excited', 'scared', 'cry',
] as const;

// Chiku has 6 more faces from the UI kit (same order as face.h; the first 16 are shared with Jinu).
export const CHIKU_MOODS = [...MOODS, 'curious', 'confused', 'bored', 'wink', 'error', 'speaking'] as const;
export const moodsFor = (kind: RobotKind): readonly string[] => (kind === 'chiku' ? CHIKU_MOODS : MOODS);

export const MOVES = ['home', 'walk', 'back', 'left', 'right', 'bend', 'shakeleg', 'moonwalk', 'dance', 'jump', 'wiggle'] as const;

export const EMOTES = [
  'happy', 'excited', 'angry', 'scared', 'sulk', 'jealous', 'shy', 'love', 'sad', 'surprised', 'tiltl', 'tiltr',
] as const;

export const MELODIES = ['startup', 'alarm', 'happy', 'sad', 'pet', 'timer', 'dizzy', 'click'] as const;

// Chiku emotion moves from the UI kit pose table (motion.h)
export const CHIKU_EMOTES = [...EMOTES, 'sleepy', 'curious', 'confused', 'bored', 'dizzy', 'wink', 'thinking', 'listen', 'speak'] as const;
export const emotesFor = (kind: RobotKind): readonly string[] => (kind === 'chiku' ? CHIKU_EMOTES : EMOTES);

// Chiku screens, index = id in ui.h (POST /api/screen?id= or ?name=)
export const SCREENS = [
  'Face', 'Clock', 'Weather', 'Timer', 'Menu', 'Alarms', 'Moves', 'Sensors', 'Sonar', 'Camera', 'Network', 'Games', 'Desk',
  'Settings', 'Calibration', 'Orientation', 'Power', 'Goodnight', 'Ringing', 'Notify', 'Boot', 'Setup',
] as const;

export const SERVO_NAMES: Record<RobotKind, string[]> = {
  chiku: ['Left hip', 'Right hip', 'Left foot', 'Right foot', 'Neck'],
  jinu: ['Left hip', 'Right hip', 'Left ankle', 'Right ankle'],
};

export const PAD_NAMES: Record<RobotKind, string[]> = {
  chiku: ['Ear L', 'Ear R', 'Leg L', 'Leg R', 'Body'],
  jinu: ['Head L', 'Head R', 'Leg L', 'Leg R', 'Body'],
};

export const GESTURES: Record<number, string> = { 1: 'tap', 2: 'double tap', 3: 'triple tap', 4: 'long press', 5: 'released' };

export const DISPLAY_PATTERNS = [
  { id: 0, label: 'Off' },
  { id: 1, label: 'Red' },
  { id: 2, label: 'Green' },
  { id: 3, label: 'Blue' },
  { id: 4, label: 'White' },
  { id: 5, label: 'Black' },
  { id: 6, label: 'Grey ramp' },
] as const;

export const DEFAULT_HOST: Record<RobotKind, string> = { chiku: 'chiku.local', jinu: 'jinu.local' };

export const ACCENT: Record<RobotKind, string> = { chiku: '#3C87F7', jinu: '#F28B30' };
