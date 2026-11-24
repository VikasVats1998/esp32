// Human readable text for robot events.
import { GESTURES } from '@/api/catalog';
import type { RobotEvent } from '@/api/types';

export function describeEvent(e: RobotEvent, padNames: string[]) {
  switch (e.type) {
    case 'pad':
      return `${padNames[e.a] ?? `pad ${e.a}`}: ${GESTURES[e.b] ?? e.b}`;
    case 'obstacle':
      return `obstacle at ${e.a} cm`;
    case 'motion':
      return `motion command ${e.a} x${e.b}`;
    case 'button':
      return e.a === 2 ? 'BOOT button long press' : 'BOOT button short press';
    case 'tap':
      return 'screen tap';
    case 'long':
      return 'screen long press';
    case 'swipe':
      return e.a > 0 ? 'screen swipe right' : 'screen swipe left';
    case 'power':
      return `power state ${['active', 'dim', 'sleep'][e.a] ?? e.a}`;
    default:
      return e.a || e.b ? `${e.type} (${e.a}, ${e.b})` : e.type;
  }
}

export function fmtTime(ms: number) {
  const s = Math.floor(ms / 1000);
  const mm = String(Math.floor(s / 60)).padStart(2, '0');
  const ss = String(s % 60).padStart(2, '0');
  return `${mm}:${ss}.${String(ms % 1000).padStart(3, '0')}`;
}
